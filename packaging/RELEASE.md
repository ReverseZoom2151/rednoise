# Releasing RedNoise

This document covers two things: how to cut a versioned release, and how to run
the project from the Docker image.

## Cutting a release

Releases are produced by the `.github/workflows/release.yml` GitHub Actions
workflow. It runs on any pushed tag matching `v*` and can also be started
manually from the Actions tab (workflow_dispatch).

Steps:

1. Make sure `main` is in the state you want to ship.
2. Create an annotated tag using the `vX.Y.Z` convention, for example:

   ```sh
   git tag -a v0.1.0 -m "RedNoise v0.1.0"
   git push origin v0.1.0
   ```

3. Pushing the tag triggers the workflow. It builds the headless-only
   configuration (`-DBUILD_APP=OFF -DBUILD_GPU=OFF -DBUILD_LIB=ON
   -DBUILD_HEADLESS=ON`) on Linux, macOS and Windows.
4. Each runner packages the `rn` CLI (and `render_headless`), the `assets/`
   folder, `README.md` and `LICENSE` into an archive:
   - `rednoise-linux-<arch>.tar.gz`
   - `rednoise-macos-<arch>.tar.gz`
   - `rednoise-windows-<arch>.zip`
5. The archives are attached to the GitHub Release for the tag via
   `softprops/action-gh-release`. If a release for the tag does not exist yet,
   the action creates it.

Notes:

- The build is dependency-light on purpose. glm is vendored under
  `third_party/glm`, so the headless build needs only a C++23 compiler, CMake
  and git. No SDL3 or OpenCL is required.
- On Linux the workflow installs gcc-13/g++-13 to guarantee a C++23 compiler.
- Binaries expect to run from a directory where `assets/` is resolvable. Unpack
  the archive and run `rn` from the extracted folder, or pass absolute asset
  paths.

## Using the Docker image

A multi-stage `Dockerfile` builds the same headless configuration and produces
a slim runtime image with just the `rn` CLI and the bundled assets.

Build the image:

```sh
docker build -t rednoise .
```

Show CLI help (the default command):

```sh
docker run --rm rednoise
```

Render a scene, writing the output back to the host via a bind mount. The
container working directory is `/work`, and the assets live at `/work/assets`:

```sh
docker run --rm -v $PWD:/out rednoise \
  render /work/assets/cornell-box.obj -o /out/out.png --mode pathtraced --spp 128
```

The `-v $PWD:/out` mount maps the current host directory to `/out` inside the
container, so `-o /out/out.png` lands the rendered image in your working
directory.
