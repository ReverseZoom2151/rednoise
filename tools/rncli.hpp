// rncli: a small, dependency-free argument parser written for C++23.
//
// It models a program as a set of subcommands, each with typed options, and
// returns a parsed result via std::expected. It gives auto-generated help,
// "did you mean" suggestions for typos, and clig.dev-friendly error reporting.
// Deliberately tiny (no framework, no macros) to fit this from-scratch project.
#pragma once

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rncli {

// A usage error, reported to stderr with clig.dev conventions (exit code 2).
struct Error {
	std::string message;
	std::optional<std::string> suggestion; // "did you mean ...?"
	int code = 2;
};

// One option: a long name (used without the leading --), an optional short
// alias, whether it is a boolean flag, plus help metadata.
struct Opt {
	std::string_view name;
	char shortName = '\0';
	bool flag = false;
	std::string_view metavar;
	std::string_view help;
	std::string_view def; // default value for a value option
};

struct Command {
	std::string_view name;
	std::string_view help;
	std::string_view usage; // e.g. "<obj> [options]"
	std::vector<Opt> opts;
};

// The result of a successful parse.
struct Parsed {
	std::string_view command;
	std::vector<std::string_view> positionals;
	std::vector<std::pair<std::string_view, std::string_view>> values; // long name -> value ("1" for flags)

	bool has(std::string_view n) const {
		return std::ranges::any_of(values, [&](auto &kv) { return kv.first == n; });
	}
	std::string_view value(std::string_view n, std::string_view fallback = "") const {
		for (auto &kv : values)
			if (kv.first == n)
				return kv.second;
		return fallback;
	}
	int valueInt(std::string_view n, int fallback) const {
		std::string_view v = value(n);
		if (v.empty())
			return fallback;
		int out = fallback;
		std::from_chars(v.data(), v.data() + v.size(), out);
		return out;
	}
	float valueFloat(std::string_view n, float fallback) const {
		std::string_view v = value(n);
		if (v.empty())
			return fallback;
		std::string tmp(v); // strtof needs a terminated string; from_chars<float> is spotty on libc++
		return std::strtof(tmp.c_str(), nullptr);
	}
};

inline int editDistance(std::string_view a, std::string_view b) {
	std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
	for (size_t j = 0; j <= b.size(); j++)
		prev[j] = static_cast<int>(j);
	for (size_t i = 1; i <= a.size(); i++) {
		cur[0] = static_cast<int>(i);
		for (size_t j = 1; j <= b.size(); j++) {
			int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
			cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
		}
		std::swap(prev, cur);
	}
	return prev[b.size()];
}

// Closest candidate within a small edit distance (for typo suggestions).
inline std::optional<std::string_view> nearest(std::string_view in, std::span<const std::string_view> candidates) {
	std::optional<std::string_view> best;
	int bestD = 3; // only suggest reasonably-close matches
	for (std::string_view c : candidates) {
		int d = editDistance(in, c);
		if (d < bestD) {
			bestD = d;
			best = c;
		}
	}
	return best;
}

namespace detail {
inline const Command *findCommand(std::string_view name, std::span<const Command> commands) {
	for (const Command &c : commands)
		if (c.name == name)
			return &c;
	return nullptr;
}
inline const Opt *findLong(std::string_view name, const Command &c) {
	for (const Opt &o : c.opts)
		if (o.name == name)
			return &o;
	return nullptr;
}
inline const Opt *findShort(char s, const Command &c) {
	for (const Opt &o : c.opts)
		if (o.shortName != '\0' && o.shortName == s)
			return &o;
	return nullptr;
}
} // namespace detail

// Parse argv (excluding the program name) against the known commands.
inline std::expected<Parsed, Error> parse(std::span<char *> argv, std::span<const Command> commands) {
	if (argv.empty())
		return std::unexpected(Error{"no command given", std::nullopt, 2});

	std::string_view cmd = argv[0];
	if (cmd == "help" || cmd == "--help" || cmd == "-h")
		return Parsed{"help", {}, {}};
	if (cmd == "version" || cmd == "--version" || cmd == "-V")
		return Parsed{"version", {}, {}};

	const Command *spec = detail::findCommand(cmd, commands);
	if (!spec) {
		std::vector<std::string_view> names;
		for (const Command &c : commands)
			names.push_back(c.name);
		Error e{"unknown command '" + std::string(cmd) + "'", std::nullopt, 2};
		if (auto s = nearest(cmd, names))
			e.suggestion = "did you mean '" + std::string(*s) + "'?";
		return std::unexpected(e);
	}

	Parsed out;
	out.command = spec->name;
	for (size_t i = 1; i < argv.size(); i++) {
		std::string_view tok = argv[i];
		auto wantHelp = [&] { return tok == "--help" || tok == "-h"; };
		if (wantHelp()) {
			out.values.emplace_back("help", "1");
			continue;
		}
		if (tok.size() >= 2 && tok[0] == '-' && tok[1] == '-') { // --long or --long=value
			std::string_view body = tok.substr(2);
			std::string_view name = body, inlineVal;
			bool hasInline = false;
			if (size_t eq = body.find('='); eq != std::string_view::npos) {
				name = body.substr(0, eq);
				inlineVal = body.substr(eq + 1);
				hasInline = true;
			}
			const Opt *o = detail::findLong(name, *spec);
			if (!o) {
				std::vector<std::string_view> names;
				for (const Opt &oo : spec->opts)
					names.push_back(oo.name);
				Error e{"unknown option '--" + std::string(name) + "'", std::nullopt, 2};
				if (auto s = nearest(name, names))
					e.suggestion = "did you mean '--" + std::string(*s) + "'?";
				return std::unexpected(e);
			}
			if (o->flag) {
				out.values.emplace_back(o->name, "1");
			} else if (hasInline) {
				out.values.emplace_back(o->name, inlineVal);
			} else if (i + 1 < argv.size()) {
				out.values.emplace_back(o->name, argv[++i]);
			} else {
				return std::unexpected(Error{"option '--" + std::string(name) + "' needs a value", std::nullopt, 2});
			}
		} else if (tok.size() >= 2 && tok[0] == '-') { // -x short
			const Opt *o = detail::findShort(tok[1], *spec);
			if (!o)
				return std::unexpected(Error{"unknown option '" + std::string(tok) + "'", std::nullopt, 2});
			if (o->flag) {
				out.values.emplace_back(o->name, "1");
			} else if (tok.size() > 2) { // -sVALUE
				out.values.emplace_back(o->name, tok.substr(2));
			} else if (i + 1 < argv.size()) {
				out.values.emplace_back(o->name, argv[++i]);
			} else {
				return std::unexpected(
				    Error{"option '-" + std::string(1, tok[1]) + "' needs a value", std::nullopt, 2});
			}
		} else {
			out.positionals.push_back(tok);
		}
	}
	return out;
}

} // namespace rncli
