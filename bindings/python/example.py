"""Runnable example for the RedNoise Python bindings.

This loads the Cornell box scene, path-traces it, saves a PNG, and prints the
triangle count and native library version.

Prerequisites:
  1. Build the shared library from the project root:
         cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF
         cmake --build build
  2. Point the binding at the built library (adjust the extension for your OS):
         export REDNOISE_LIBRARY="$PWD/build/librednoise.so"     # Linux
         export REDNOISE_LIBRARY="$PWD/build/librednoise.dylib"   # macOS
         set    REDNOISE_LIBRARY=%CD%\\build\\rednoise.dll          # Windows

Run from the repository root so the asset path resolves:
    python bindings/python/example.py

Or install the package first (pip install ./bindings/python) and run from
anywhere; this script locates assets relative to the repo layout.
"""

import os

import rednoise


def main() -> None:
    # Resolve assets/cornell-box.obj relative to this file's location in the
    # repo (bindings/python/example.py -> <repo>/assets/cornell-box.obj),
    # falling back to a path relative to the current working directory.
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(here))
    obj_path = os.path.join(repo_root, "assets", "cornell-box.obj")
    if not os.path.isfile(obj_path):
        obj_path = os.path.join("assets", "cornell-box.obj")

    print(f"RedNoise version: {rednoise.version()}")

    with rednoise.Scene.load_obj(obj_path) as scene:
        print(f"Triangle count: {scene.triangle_count}")
        image = scene.render(
            mode="pathtraced",
            width=400,
            height=300,
            samples=64,
        )
        out_path = "cornell.png"
        image.save(out_path)
        print(f"Saved {image.width}x{image.height} render to {out_path}")


if __name__ == "__main__":
    main()
