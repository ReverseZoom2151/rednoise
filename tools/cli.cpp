// rn: the RedNoise renderer CLI. A single front-end over the engine, parsed with
// the native C++23 rncli parser (tools/rncli.hpp). Follows clig.dev conventions:
// help on stdout, errors on stderr with "did you mean" hints and exit code 2,
// --version, and colour only on a TTY (respecting NO_COLOR).
//
//   rn render <obj> [-o out.png] [-m mode] [-s WxH] [--spp N] [--scale S] [--cam-z Z]
//   rn animate <obj> [-o prefix] [--frames N] [--ease linear|smooth|reciprocal] [-s WxH]
//   rn version | rn help | rn <command> --help

#define _CRT_SECURE_NO_WARNINGS // std::getenv is fine; silence MSVC's deprecation

#include "Camera.h"
#include "ObjLoader.h"
#include "Radiosity.h"
#include "Renderer.h"
#include "rncli.hpp"
#include <Canvas.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define RN_ISATTY(fd) _isatty(_fileno(fd))
#else
#include <unistd.h>
#define RN_ISATTY(fd) isatty(fileno(fd))
#endif

namespace {

const std::vector<rncli::Command> COMMANDS = {
    {"render",
     "render a single image",
     "<obj> [options]",
     {
         {"out", 'o', false, "FILE", "output file, .png or .ppm", "out.png"},
         {"mode", 'm', false, "MODE", "wireframe|rasterised|raytraced|pathtraced|photon|radiosity", "raytraced"},
         {"size", 's', false, "WxH", "image size", "640x480"},
         {"spp", '\0', false, "N", "samples/pixel for pathtraced", "64"},
         {"scale", '\0', false, "S", "OBJ load scale", "0.35"},
         {"cam-z", '\0', false, "Z", "camera distance on +Z", "4.0"},
     }},
    {"animate",
     "render an orbiting-camera sequence to PPM frames",
     "<obj> [options]",
     {
         {"out", 'o', false, "PREFIX", "frame prefix (writes PREFIX-000.ppm ...)", "frame"},
         {"frames", '\0', false, "N", "number of frames", "36"},
         {"ease", '\0', false, "MODE", "linear|smooth|reciprocal", "reciprocal"},
         {"size", 's', false, "WxH", "frame size", "320x240"},
     }},
    {"shell", "interactive REPL: load a scene once, then re-render", "[obj]", {}},
};

bool useColour() {
	return std::getenv("NO_COLOR") == nullptr && RN_ISATTY(stderr) != 0;
}

void reportError(const rncli::Error &e) {
	const char *red = useColour() ? "\033[31m" : "";
	const char *dim = useColour() ? "\033[2m" : "";
	const char *rst = useColour() ? "\033[0m" : "";
	std::cerr << red << "error: " << rst << e.message << "\n";
	if (e.suggestion)
		std::cerr << dim << "       " << *e.suggestion << rst << "\n";
	std::cerr << "run 'rn help' for usage\n";
}

void printOverview() {
	std::cout << "rn - RedNoise renderer\n\nUsage:\n  rn <command> [options]\n\nCommands:\n";
	for (const rncli::Command &c : COMMANDS)
		std::cout << "  " << c.name << std::string(10 - c.name.size(), ' ') << c.help << "\n";
	std::cout << "  version   print the library version\n"
	             "  help      show this help\n\n"
	             "Run 'rn <command> --help' for command-specific options.\n";
}

void printCommandHelp(const rncli::Command &c) {
	std::cout << "rn " << c.name << " " << c.usage << "\n\n" << c.help << "\n\nOptions:\n";
	for (const rncli::Opt &o : c.opts) {
		std::string left = "  --" + std::string(o.name);
		if (o.shortName)
			left += ", -" + std::string(1, o.shortName);
		if (!o.flag)
			left += " " + std::string(o.metavar);
		std::cout << left;
		if (left.size() < 28)
			std::cout << std::string(28 - left.size(), ' ');
		std::cout << o.help;
		if (!o.def.empty())
			std::cout << " (default " << o.def << ")";
		std::cout << "\n";
	}
	std::cout << "  --help, -h                  show this help\n";
}

const rncli::Command &spec(std::string_view name) {
	for (const rncli::Command &c : COMMANDS)
		if (c.name == name)
			return c;
	return COMMANDS[0];
}

void parseSize(std::string_view s, int &w, int &h) {
	if (size_t x = s.find('x'); x != std::string_view::npos) {
		w = std::atoi(std::string(s.substr(0, x)).c_str());
		h = std::atoi(std::string(s.substr(x + 1)).c_str());
	}
}

void save(const Canvas &c, const std::string &path) {
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".ppm")
		c.savePPM(path);
	else
		c.savePNG(path);
}

constexpr std::string_view MODES = "wireframe rasterised raytraced pathtraced photon radiosity";

// Render `model` into `canvas` using the named mode. Returns false on unknown mode.
bool renderMode(const std::vector<ModelTriangle> &model, const std::string &mode, int w, int h, float camZ, int spp,
                Canvas &canvas) {
	Camera camera(w, h, 2.0f, glm::vec3(0.0f, 0.0f, camZ));
	camera.lookAt(glm::vec3(0.0f));
	if (mode == "wireframe")
		renderWireframe(model, camera, canvas);
	else if (mode == "rasterised" || mode == "rasterized")
		renderRasterised(model, camera, canvas);
	else if (mode == "raytraced")
		renderRaytraced(model, camera, canvas);
	else if (mode == "pathtraced")
		renderPathTraced(model, camera, canvas, spp);
	else if (mode == "photon")
		renderPhotonMapped(model, camera, canvas, 200000);
	else if (mode == "radiosity")
		renderRadiosity(model, camera, canvas);
	else
		return false;
	return true;
}

int doRender(const rncli::Parsed &p) {
	if (p.positionals.empty()) {
		reportError({"render: missing <obj> argument", std::nullopt, 2});
		return 2;
	}
	std::string obj(p.positionals[0]);
	std::string out(p.value("out", "out.png"));
	std::string mode(p.value("mode", "raytraced"));
	float scale = p.valueFloat("scale", 0.35f), camZ = p.valueFloat("cam-z", 4.0f);
	int spp = p.valueInt("spp", 64), w = 640, h = 480;
	parseSize(p.value("size", "640x480"), w, h);

	std::vector<ModelTriangle> model = loadOBJ(obj, scale);
	if (model.empty()) {
		reportError({"render: no triangles loaded from " + obj, std::nullopt, 1});
		return 1;
	}
	Canvas canvas(w, h);
	if (!renderMode(model, mode, w, h, camZ, spp, canvas)) {
		reportError({"render: unknown mode '" + mode + "'", "modes: " + std::string(MODES), 2});
		return 2;
	}
	save(canvas, out);
	std::cout << "wrote " << out << " (" << w << "x" << h << ", " << mode << ")\n";
	return 0;
}

std::vector<float> easedParameters(int frames, const std::string &mode) {
	std::vector<float> t(frames, 0.0f);
	if (frames <= 1)
		return t;
	if (mode == "reciprocal") {
		std::vector<float> weight(frames);
		float mid = (frames - 1) / 2.0f;
		for (int i = 0; i < frames; i++)
			weight[i] = 1.0f / (1.0f + std::abs(i - mid));
		float acc = 0.0f, total = 0.0f;
		for (float wgt : weight)
			total += wgt;
		for (int i = 0; i < frames; i++) {
			t[i] = acc / total;
			acc += weight[i];
		}
	} else {
		for (int i = 0; i < frames; i++) {
			float lin = static_cast<float>(i) / frames;
			t[i] = (mode == "smooth") ? lin * lin * (3.0f - 2.0f * lin) : lin;
		}
	}
	return t;
}

int doAnimate(const rncli::Parsed &p) {
	if (p.positionals.empty()) {
		reportError({"animate: missing <obj> argument", std::nullopt, 2});
		return 2;
	}
	std::string obj(p.positionals[0]);
	std::string prefix(p.value("out", "frame"));
	std::string ease(p.value("ease", "reciprocal"));
	int frames = p.valueInt("frames", 36), w = 320, h = 240;
	parseSize(p.value("size", "320x240"), w, h);

	std::vector<ModelTriangle> model = loadOBJ(obj, 0.35f);
	if (model.empty()) {
		reportError({"animate: no triangles loaded from " + obj, std::nullopt, 1});
		return 1;
	}
	std::vector<float> t = easedParameters(frames, ease);
	Canvas canvas(w, h);
	for (int i = 0; i < frames; i++) {
		Camera camera(w, h, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
		camera.orbitY(2.0f * 3.14159265f * t[i], glm::vec3(0.0f));
		renderRasterised(model, camera, canvas);
		char name[512];
		std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), i);
		canvas.savePPM(name);
	}
	std::cout << "wrote " << frames << " frames (" << ease << " easing): " << prefix << "-000.ppm ...\n";
	return 0;
}

// Interactive REPL: load a scene once, tweak state, re-render. A plain getline
// loop dispatching through the same engine calls - no external line-editor.
int doShell(const rncli::Parsed &p) {
	std::vector<ModelTriangle> model;
	std::string loaded, mode = "raytraced";
	int spp = 64, w = 640, h = 480;
	float camZ = 4.0f, scale = 0.35f;
	static const std::vector<std::string_view> MODE_LIST = {"wireframe",  "rasterised", "raytraced",
	                                                        "pathtraced", "photon",     "radiosity"};
	static const std::vector<std::string_view> CMDS = {"load",   "mode", "spp",  "size", "cam", "scale",
	                                                   "render", "show", "help", "exit", "quit"};

	auto loadScene = [&](const std::string &path) {
		auto m = loadOBJ(path, scale);
		if (m.empty()) {
			std::cout << "  could not load '" << path << "'\n";
			return;
		}
		model = std::move(m);
		loaded = path;
		std::cout << "  loaded " << model.size() << " triangles from " << path << "\n";
	};
	auto showState = [&] {
		std::cout << "  scene=" << (loaded.empty() ? "(none)" : loaded) << " mode=" << mode << " size=" << w << "x" << h
		          << " spp=" << spp << " cam-z=" << camZ << " scale=" << scale << "\n";
	};

	if (!p.positionals.empty())
		loadScene(std::string(p.positionals[0]));
	std::cout << "RedNoise interactive shell. Type 'help' for commands, 'exit' to quit.\n";

	std::string line;
	while (true) {
		std::cout << "rednoise> " << std::flush;
		if (!std::getline(std::cin, line)) {
			std::cout << "\n";
			break;
		}
		std::vector<std::string> tok;
		std::istringstream iss(line);
		for (std::string word; iss >> word;)
			tok.push_back(word);
		if (tok.empty())
			continue;
		const std::string &c = tok[0];
		auto arg = [&](size_t i) { return i < tok.size() ? tok[i] : std::string(); };

		if (c == "exit" || c == "quit")
			break;
		else if (c == "help")
			std::cout << "  load <obj>        load a scene\n"
			             "  mode <mode>       "
			          << MODES
			          << "\n"
			             "  spp <n>           samples/pixel (pathtraced)\n"
			             "  size <WxH>        image size\n"
			             "  cam <z>           camera distance on +Z\n"
			             "  scale <s>         OBJ load scale (reload to apply)\n"
			             "  render [out.png]  render with the current settings\n"
			             "  show              print current settings\n"
			             "  exit              leave the shell\n";
		else if (c == "load") {
			if (arg(1).empty())
				std::cout << "  usage: load <obj>\n";
			else
				loadScene(arg(1));
		} else if (c == "mode") {
			if (std::ranges::find(MODE_LIST, arg(1)) != MODE_LIST.end())
				mode = arg(1);
			else
				std::cout << "  unknown mode '" << arg(1) << "'. modes: " << MODES << "\n";
		} else if (c == "spp")
			spp = std::max(1, std::atoi(arg(1).c_str()));
		else if (c == "size")
			parseSize(arg(1), w, h);
		else if (c == "cam")
			camZ = std::strtof(arg(1).c_str(), nullptr);
		else if (c == "scale")
			scale = std::strtof(arg(1).c_str(), nullptr);
		else if (c == "show")
			showState();
		else if (c == "render") {
			if (model.empty()) {
				std::cout << "  no scene loaded (use 'load <obj>')\n";
				continue;
			}
			std::string out = arg(1).empty() ? "out.png" : arg(1);
			Canvas canvas(w, h);
			renderMode(model, mode, w, h, camZ, spp, canvas);
			save(canvas, out);
			std::cout << "  wrote " << out << " (" << w << "x" << h << ", " << mode << ")\n";
		} else {
			std::cout << "  unknown command '" << c << "'";
			if (auto s = rncli::nearest(c, CMDS))
				std::cout << " (did you mean '" << *s << "'?)";
			std::cout << "\n";
		}
	}
	return 0;
}

} // namespace

int main(int argc, char **argv) {
	std::span<char *> args(argv + 1, argv + argc);
	auto parsed = rncli::parse(args, COMMANDS);
	if (!parsed) {
		reportError(parsed.error());
		return parsed.error().code;
	}
	const rncli::Parsed &p = *parsed;

	if (p.command == "help") {
		printOverview();
		return 0;
	}
	if (p.command == "version") {
		std::cout << "rednoise 0.1.0\n";
		return 0;
	}
	if (p.has("help")) {
		printCommandHelp(spec(p.command));
		return 0;
	}
	if (p.command == "render")
		return doRender(p);
	if (p.command == "animate")
		return doAnimate(p);
	if (p.command == "shell")
		return doShell(p);
	return 1;
}
