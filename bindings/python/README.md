# RedNoise Python bindings

Pure-Python `ctypes` bindings for the RedNoise CPU software renderer. They call
the stable C ABI declared in `include/rednoise/rednoise.h`. There is no compiled
extension: the bindings load the RedNoise shared library at runtime.

The shared library is not bundled with this package. You build it from the C++
project and point the bindings at it.

## Build the shared library

From the repository root:

```
cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF
cmake --build build
```

This produces the shared library under `build/`:

- Linux: `librednoise.so`
- macOS: `librednoise.dylib`
- Windows: `rednoise.dll`

## Point the bindings at the library

The bindings locate the library in this order:

1. The `REDNOISE_LIBRARY` environment variable (an explicit path to the file).
2. The system search path (`ctypes.util.find_library("rednoise")`).
3. Common local build paths (`build/`, next to the package, and similar).

Setting `REDNOISE_LIBRARY` is the most reliable option:

```
# Linux
export REDNOISE_LIBRARY="$PWD/build/librednoise.so"

# macOS
export REDNOISE_LIBRARY="$PWD/build/librednoise.dylib"

# Windows (cmd)
set REDNOISE_LIBRARY=%CD%\build\rednoise.dll
```

## Install

```
pip install ./bindings/python
```

Pillow is optional and only needed for `Image.to_pillow()` or saving non-PNG
formats:

```
pip install "./bindings/python[pillow]"
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
