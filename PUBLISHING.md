# Publishing rednoise

This document explains how rednoise releases are published to every registry and the one-time setup a maintainer must complete. It is written as a runbook: skim the section you need.

Repository: github.com/ReverseZoom2151/rednoise
License: MIT
Current version: 0.1.0

## 1. Overview

Publishing is driven entirely by git tags. You bump the version, tag it, and push. Everything else happens in GitHub Actions.

Release flow:

- Bump the version in every manifest (see "Cutting a release").
- Commit the version bump.
- Create an annotated tag `vX.Y.Z` and push it.
- `.github/workflows/release.yml` fires on the pushed `v*` tag. It builds the Linux, macOS, and Windows binaries and CREATES the GitHub Release (via softprops/action-gh-release), attaching the binaries as assets.
- When that GitHub Release is published, three publish workflows fire automatically on `release: published`:
  - `.github/workflows/publish-python.yml` -> builds wheels (cibuildwheel) plus an sdist and publishes to PyPI.
  - `.github/workflows/publish-rust.yml` -> vendors the C++ sources and runs `cargo publish` to crates.io.
  - `.github/workflows/publish-npm.yml` -> builds the WASM package (Emscripten) and runs `npm publish --provenance` to npm.
- Each publish workflow also supports `workflow_dispatch`, so you can re-run any single registry publish by hand from the Actions tab.

Diagram:

```text
  git tag vX.Y.Z + push
          |
          v
  release.yml
   - builds Linux / macOS / Windows binaries
   - CREATES + publishes the GitHub Release
          |
          |  (on: release: published)
          |
   +------+---------------------+
   |      |                     |
   v      v                     v
 publish-python   publish-rust     publish-npm
   -> PyPI          -> crates.io      -> npm
```

## 2. One-time setup

Do this once per registry. After it is done, every future release publishes automatically.

### PyPI (Trusted Publishing, no secret)

1. Create the PyPI project named `rednoise`. Either reserve it by doing one manual upload (see "First-time / manual publish"), or create the pending publisher directly.
2. In the PyPI project settings, add a Trusted Publisher with these values:
   - Owner: `ReverseZoom2151`
   - Repository: `rednoise`
   - Workflow: `publish-python.yml`
   - Environment: optional (leave blank unless you configured one)
3. No API token or repo secret is required. Publishing uses OIDC (`id-token: write`).

Reference: https://docs.pypi.org/trusted-publishers/

### crates.io (API token)

1. Log in to crates.io.
2. Go to Account Settings -> API Tokens and create a new API token.
3. Add it as a GitHub repo secret named `CARGO_REGISTRY_TOKEN`:
   - GitHub repo -> Settings -> Secrets and variables -> Actions -> New repository secret.
4. The crate name `rednoise` must be available or already owned by you. If it is taken, you must pick a different crate name and update `bindings/rust/Cargo.toml`.

### npm (Automation token)

1. Log in to npmjs.com.
2. Create an Automation access token (Access Tokens -> Generate New Token -> Automation). Automation tokens bypass 2FA prompts, which is required for CI.
3. Add it as a GitHub repo secret named `NPM_TOKEN`.
4. Check the package name. If `rednoise` is taken on npm, publish under a scope such as `@reversezoom2151/rednoise`, update `bindings/wasm/package.json` accordingly, and ensure the workflow publishes with `--access public` (scoped packages are private by default).

### GitHub Actions permissions

- Actions must have `contents: write` so `release.yml` can create the release. This is already set.
- The publish workflows that use OIDC (PyPI Trusted Publishing and npm provenance) need `id-token: write`. Confirm this permission is present in `publish-python.yml` and `publish-npm.yml`.

## 3. Cutting a release

Keep every version string in sync. Update the version in all of these files:

- `CMakeLists.txt` -> the `project(... VERSION X.Y.Z ...)` line
- `bindings/python/pyproject.toml`
- `bindings/rust/Cargo.toml`
- `bindings/wasm/package.json`
- `include/rednoise/rednoise.h` -> the `RN_VERSION_MAJOR`, `RN_VERSION_MINOR`, and `RN_VERSION_PATCH` macros

Then commit and tag:

```bash
# after editing all version strings above
git add -A
git commit -m "release: vX.Y.Z"

git tag -a vX.Y.Z -m "rednoise vX.Y.Z"
git push origin main
git push origin vX.Y.Z
```

Note: all five version strings must match the tag. A mismatch (for example a Cargo.toml still on the old version) will cause the corresponding publish workflow to fail or publish the wrong version.

## 4. Registry ports (manual)

vcpkg and Conan are community registries. They are updated by pull request, not by a token in CI, so these steps are manual and happen after the GitHub Release exists.

### vcpkg

1. After the release tag exists, compute the source tarball SHA512:

   ```bash
   curl -sL https://github.com/ReverseZoom2151/rednoise/archive/refs/tags/vX.Y.Z.tar.gz | sha512sum
   ```

2. Fill in the `REF` and `SHA512` placeholders in `packaging/vcpkg/ports/rednoise/portfile.cmake`.
3. Open a pull request to microsoft/vcpkg to add or update the port, or use the directory as a local overlay port (`--overlay-ports`) if you do not want to upstream it yet.

### Conan

1. Submit the recipe to conan-center-index via pull request, or
2. Run `conan upload` to publish to your own Conan remote.

See `packaging/README.md` for the port layout and current placeholder values.

## 5. First-time / manual publish

If you want to publish once by hand (for example to reserve a name on PyPI, or to recover from a failed CI run), log in locally first, then:

```bash
# Rust -> crates.io
cargo publish            # run from bindings/rust

# Python -> PyPI
python -m build          # run from bindings/python; produces dist/
twine upload dist/*

# npm -> npm registry
npm publish              # run from bindings/wasm (add --access public if scoped)
```

Each command requires you to be authenticated locally (`cargo login`, a PyPI token in `~/.pypirc` or `twine` prompt, and `npm login`).

## 6. Verifying

After a release, confirm everything landed:

- GitHub -> Actions: `release.yml` and all three `publish-*` workflow runs are green.
- PyPI: https://pypi.org/project/rednoise shows the new version.
- crates.io: https://crates.io/crates/rednoise shows the new version.
- npm: https://www.npmjs.com/package/rednoise shows the new version.
- The GitHub Release page lists the Linux, macOS, and Windows binary assets.

If a single registry failed, re-run just that workflow from the Actions tab via `workflow_dispatch` once the underlying issue (secret, name availability, version mismatch) is fixed.
