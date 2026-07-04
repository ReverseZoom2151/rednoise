//! Build script: compiles the RedNoise C++ engine from source (vendored under
//! `vendor/`) into a static library and links it into the crate.
//!
//! This makes the crate self-contained: `cargo build` needs only a C++23
//! compiler (found by the `cc` crate) - no prebuilt `rednoise` library and no
//! network access at build time.
//!
//! The vendored sources live under `vendor/` and are produced by
//! `vendor.sh` (run automatically by the publish workflow). See the crate
//! README for details.
//!
//! An optional fast path: set `REDNOISE_LIB_DIR` to a directory containing a
//! prebuilt `rednoise` dynamic library and the script links that instead of
//! compiling from source. The default, and the crates.io path, is from-source.

use std::env;
use std::path::{Path, PathBuf};

/// Engine translation units, relative to `vendor/`. Kept in sync with the
/// C ABI engine (the app entry point `RedNoise.cpp` and the SDL-based
/// `DrawingWindow.cpp` are intentionally excluded - they are not part of the
/// library surface).
const SOURCES: &[&str] = &[
    // framework/
    "framework/Canvas.cpp",
    "framework/CanvasPoint.cpp",
    "framework/CanvasTriangle.cpp",
    "framework/Colour.cpp",
    "framework/ModelTriangle.cpp",
    "framework/TextureMap.cpp",
    "framework/TexturePoint.cpp",
    "framework/Utils.cpp",
    // src/
    "src/BDPT.cpp",
    "src/BVH.cpp",
    "src/Camera.cpp",
    "src/capi.cpp",
    "src/Clouds.cpp",
    "src/ColourUtil.cpp",
    "src/Drawing.cpp",
    "src/Geometry.cpp",
    "src/Interpolation.cpp",
    "src/IrradianceCache.cpp",
    "src/KdTree.cpp",
    "src/Lines.cpp",
    "src/Materials.cpp",
    "src/Meshing.cpp",
    "src/Metropolis.cpp",
    "src/Mipmap.cpp",
    "src/Noise.cpp",
    "src/Nurbs.cpp",
    "src/ObjLoader.cpp",
    "src/Ocean.cpp",
    "src/OceanFFT.cpp",
    "src/Octree.cpp",
    "src/Photon.cpp",
    "src/Radiosity.cpp",
    "src/Renderer.cpp",
    "src/Scene.cpp",
    "src/SceneGraph.cpp",
    "src/Transform.cpp",
    "src/Voxel.cpp",
];

fn main() {
    // Optional fast path: link a prebuilt dynamic library instead of building
    // from source. Off by default; the from-source path is what works on
    // crates.io.
    println!("cargo:rerun-if-env-changed=REDNOISE_LIB_DIR");
    if let Ok(lib_dir) = env::var("REDNOISE_LIB_DIR") {
        link_prebuilt(&lib_dir);
        return;
    }

    build_from_source();
}

/// Compile the vendored C++ engine into a static library and link it.
fn build_from_source() {
    let manifest_dir =
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR is always set by cargo");
    let vendor = PathBuf::from(&manifest_dir).join("vendor");

    if !vendor.join("src").join("capi.cpp").exists() {
        panic!(
            "vendored engine sources not found at {}. Run bindings/rust/vendor.sh \
             to populate vendor/ before building from a source checkout.",
            vendor.display()
        );
    }

    // Rebuild if any vendored source changes.
    println!("cargo:rerun-if-changed={}", vendor.display());
    println!("cargo:rerun-if-changed=build.rs");

    let mut build = cc::Build::new();
    build
        .cpp(true)
        // C++23. `cc` translates this to the right flag per compiler
        // (`-std=c++23` for gcc/clang, `/std:c++23` for MSVC - MSVC also
        // accepts `/std:c++latest` for older toolchains).
        .std("c++23")
        // Include roots. framework/ and src/ are needed for the angle-bracket
        // and quoted engine includes; include/ carries the public C ABI header
        // (`rednoise/rednoise.h`); third_party/ has glm. Note the stb header is
        // pulled in via a path relative to the .cpp files
        // (`../third_party/stb/...`), which resolves inside vendor/ because
        // third_party/ sits beside framework/ and src/.
        .include(vendor.join("framework"))
        .include(vendor.join("src"))
        .include(vendor.join("include"))
        .include(vendor.join("third_party"));

    for src in SOURCES {
        build.file(vendor.join(src));
    }

    // OpenMP is OPTIONAL. The engine builds and runs single-threaded without
    // it; the `#pragma omp` directives are simply ignored. We therefore do NOT
    // require or link an OpenMP runtime here. Silence the "unknown pragma"
    // warning on gcc/clang so the build stays quiet.
    let compiler = build.get_compiler();
    if !compiler.is_like_msvc() {
        build.flag_if_supported("-Wno-unknown-pragmas");
    }

    build.compile("rednoise_engine");

    // Link the C++ standard library so the final Rust binary resolves C++
    // symbols. `cc` links the runtime for the object files it builds, but the
    // Rust link step still needs the standard library named explicitly on some
    // platforms.
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" | "android" | "freebsd" | "dragonfly" | "netbsd" | "openbsd" => {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        }
        "macos" | "ios" => {
            println!("cargo:rustc-link-lib=dylib=c++");
        }
        _ => {
            // MSVC (Windows) pulls in the C++ runtime automatically.
        }
    }
}

/// Optional fast path: link a prebuilt `rednoise` dynamic library from
/// `lib_dir` (the legacy REDNOISE_LIB_DIR behaviour).
fn link_prebuilt(lib_dir: &str) {
    let dir = Path::new(lib_dir);
    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=dylib=rednoise");

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" => {
            println!("cargo:rustc-link-lib=dylib=stdc++");
            println!("cargo:rustc-link-lib=dylib=gomp");
        }
        "macos" => {
            println!("cargo:rustc-link-lib=dylib=c++");
        }
        _ => {}
    }
}
