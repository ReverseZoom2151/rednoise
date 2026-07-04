# RedNoise: Graphics Rendering Toolkit

A small **C++ software rasteriser** built while working through the fundamentals of
computer graphics. It draws directly into a pixel buffer (no GPU, no OpenGL) and
presents it with **SDL3**, using **glm** for vector/matrix maths. It's an educational
codebase: each feature is a self-contained step, from setting individual pixels to
loading and colouring OBJ models.

## Features

- **Direct pixel manipulation**: the "red noise" demo that gives the project its name.
- **Interpolation**: single-float and 3-element (vec3) linear interpolation, used for
  greyscale and 2D colour gradients.
- **Drawing primitives**: DDA line drawing; stroked, filled, and random triangles.
- **Texture mapping**: affine texture-mapped triangles sampled from a PPM image.
- **OBJ / MTL loading**: parses Wavefront `.obj` geometry and resolves its `.mtl`
  material colours (`mtllib` / `usemtl`).
- **Interactive window**: SDL3 event handling for keyboard/mouse input and PPM/BMP
  screenshots.

## Project structure

```
redNoise/
├── src/
│   └── RedNoise.cpp        # application: demos, drawing, OBJ loading, main loop
├── framework/             # the "sdw" teaching framework (headers + sources together)
│   ├── DrawingWindow.*     # SDL3 window + pixel buffer, save/poll
│   ├── CanvasPoint.*  CanvasTriangle.*  Colour.*
│   ├── ModelTriangle.*  TextureMap.*  TexturePoint.*  Utils.*
├── third_party/
│   └── glm/               # vendored glm 1.0.1 (header-only, trimmed)
├── assets/                # cornell-box.obj/.mtl, texture.ppm
├── CMakeLists.txt         # primary build
├── Makefile               # alternative build (clang++ + pkg-config)
└── THIRD_PARTY_NOTICES.md
```

## Dependencies

| Dependency | Version | How it's provided |
|------------|---------|-------------------|
| [SDL3](https://www.libsdl.org) | 3.x | System library (vcpkg / package manager) |
| [glm](https://github.com/g-truc/glm) | 1.0.1 | Vendored in `third_party/glm` (nothing to install) |

A C++23 compiler is required (GCC 13+, Clang 17+, or MSVC 19.34+).

## Building

Run the program **from the repository root** so that the relative `assets/` paths
resolve.

### CMake (recommended, cross-platform)

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/RedNoise            # Windows/MSVC: .\build\Release\RedNoise.exe
```

On Windows with [vcpkg](https://vcpkg.io), install SDL3 and point CMake at the
toolchain:

```sh
vcpkg install sdl3
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

### Makefile (Linux/macOS)

Requires `clang++` and SDL3 discoverable via `pkg-config sdl3`:

```sh
make            # debug build, then runs
make production # optimised build
make clean
```

## Controls

| Input | Action |
|-------|--------|
| `u` | Draw a random stroked triangle |
| `f` | Draw a random filled triangle |
| Arrow keys | Print direction (camera hooks, reserved) |
| Mouse click | Save the frame to `output.ppm` and `output.bmp` |
| `Esc` | Quit |

## Status & roadmap

The framework, primitives, texture mapping, and OBJ/MTL loading are in place; the
Cornell box is parsed (with materials) at startup. Projecting and rasterising that
3D geometry (perspective projection, a depth buffer, a movable camera, and on
toward a ray tracer with shadows and lighting) is the intended next step.

See [ROADMAP.md](ROADMAP.md) for the full phased build plan, from a first
wireframe render up through Phong shading, reflection/refraction, soft shadows,
acceleration structures, and global illumination.

## Credits

The `framework/` "sdw" classes originate from the University of Bristol Computer
Graphics unit (COMS30020). Third-party licenses are listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
