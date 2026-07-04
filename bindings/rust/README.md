# rednoise (Rust bindings)

Safe Rust bindings for the RedNoise CPU software renderer. They wrap the stable
C ABI declared in `include/rednoise/rednoise.h` and link the native `rednoise`
library.

## Prerequisites

Build the native C++ library first, from the repository root:

```sh
cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF
cmake --build build
```

This produces the `rednoise` shared/static library under `build/`.

## Building the crate

Point the build script at the directory that holds the built library via
`REDNOISE_LIB_DIR`, then build as usual:

```sh
REDNOISE_LIB_DIR=<repo>/build cargo build
```

If `REDNOISE_LIB_DIR` is unset, the build script defaults to the repo's `build/`
directory (two levels up from `bindings/rust`) and prints a warning.

The engine is C++, so linking also pulls in the C++ runtime. The build script
handles this per platform: `stdc++` (plus `gomp` for OpenMP) on Linux, `c++` on
macOS, and the toolchain default on Windows.

## Usage

```rust
use rednoise::{Scene, Mode};

fn main() -> Result<(), rednoise::Error> {
    let scene = Scene::load_obj("assets/cornell-box.obj", 0.35)?;
    println!("triangles: {}", scene.triangle_count());

    let image = scene.render(Mode::Pathtraced, 400, 300, 4.0, 64)?;
    image.save_png("cornell.png")?;

    println!("rednoise {}", rednoise::version());
    Ok(())
}
```

## Example

Run the bundled example from the repository root (so the asset path resolves):

```sh
REDNOISE_LIB_DIR=<repo>/build cargo run --manifest-path bindings/rust/Cargo.toml --example render
```

At runtime the dynamic loader must also be able to find the shared library. On
Linux set `LD_LIBRARY_PATH=<repo>/build`; on macOS set
`DYLD_LIBRARY_PATH=<repo>/build`.

## API

- `Scene::load_obj(path, scale) -> Result<Scene, Error>`
- `Scene::triangle_count() -> u32`
- `Scene::render(mode, width, height, cam_z, samples) -> Result<Image, Error>`
- `Image { width, height, rgba }` with `Image::save_png(path)`
- `Mode`: `Wireframe`, `Rasterised`, `Raytraced`, `Pathtraced`
- `version() -> String`

## License

MIT.
