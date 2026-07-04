# RedNoise Python bindings

Python `ctypes` bindings for the RedNoise CPU software renderer. They call the
stable C ABI declared in `include/rednoise/rednoise.h`. There is no compiled
extension module: the bindings load the RedNoise shared library at runtime. The
native library is built and bundled into the package, so a normal install is
self-contained.

## Install

```
pip install rednoise
```

Prebuilt wheels ship the native library, so on a supported platform this is all
you need. Installing from an sdist compiles the native library on your machine
via CMake, so you need a C++23-capable compiler (GCC 12+, Clang 15+, or MSVC
2022+) and CMake 3.24 or newer. The build uses the vendored `glm` in
`third_party/` and runs fully offline; it does not fetch SDL3 or any other
optional dependency.

Under the hood, `pip install` drives scikit-build-core and CMake to build only
the installable `rednoise` library (`-DBUILD_LIB=ON`, everything else off) and
installs the resulting shared library into the importable `rednoise/` package:

- Linux: `librednoise.so`
- macOS: `librednoise.dylib`
- Windows: `rednoise.dll`

Pillow is optional and only needed for `Image.to_pillow()` or saving non-PNG
formats:

```
pip install "rednoise[pillow]"
```

## Library discovery

The bundled library is found automatically. For development against a
separately built library, the bindings locate it in this order:

1. The library bundled next to the package (what a wheel installs).
2. The `REDNOISE_LIBRARY` environment variable (an explicit path to the file).
3. The system search path (`ctypes.util.find_library("rednoise")`).
4. Common local build paths (`build/`, next to the package, and similar).

To override with a locally built library, set `REDNOISE_LIBRARY`:

```
# Linux
export REDNOISE_LIBRARY="$PWD/build/librednoise.so"

# macOS
export REDNOISE_LIBRARY="$PWD/build/librednoise.dylib"

# Windows (cmd)
set REDNOISE_LIBRARY=%CD%\build\rednoise.dll
```

## Develop from a local checkout

Install the bindings straight from this directory:

```
pip install ./bindings/python
```

Add the Pillow extra with:

```
pip install "./bindings/python[pillow]"
```

You can still build the shared library by hand from the repository root and
point `REDNOISE_LIBRARY` at it:

```
cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF
cmake --build build
```

## Usage

```python
import rednoise

print(rednoise.version())

with rednoise.Scene.load_obj("assets/cornell-box.obj") as scene:
    print("triangles:", scene.triangle_count)
    image = scene.render(mode="pathtraced", width=400, height=300, samples=64)
    image.save("cornell.png")
```

`Scene.render` accepts a render mode as a `RenderMode` enum, an integer, or a
name string: `"wireframe"`, `"rasterised"`, `"raytraced"`, or `"pathtraced"`.
The returned `Image` exposes `.rgba` (raw RGBA8 bytes), `.size`, `.save(path)`,
and `.to_pillow()` when Pillow is installed.

A complete runnable example lives in `example.py`. Run it from the repository
root (after building the library and setting `REDNOISE_LIBRARY`):

```
python bindings/python/example.py
```
