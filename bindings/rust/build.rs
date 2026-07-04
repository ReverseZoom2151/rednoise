//! Build script: tells Cargo where to find and how to link the native
//! `rednoise` library (built from the C++ engine via CMake) plus the C++
//! runtime it depends on.

use std::env;
use std::path::PathBuf;

fn main() {
    // Directory that contains the built `rednoise` shared/static library.
    // Override it with the REDNOISE_LIB_DIR environment variable; otherwise we
    // default to the repo's `build/` directory, two levels up from this crate
    // (bindings/rust -> repo root -> build).
    let lib_dir = match env::var("REDNOISE_LIB_DIR") {
        Ok(dir) => PathBuf::from(dir),
        Err(_) => {
            let manifest_dir = env::var("CARGO_MANIFEST_DIR")
                .expect("CARGO_MANIFEST_DIR is always set by cargo");
            let default = PathBuf::from(manifest_dir)
                .join("..")
                .join("..")
                .join("build");
            println!(
                "cargo:warning=REDNOISE_LIB_DIR not set; defaulting to {}. \
                 Set REDNOISE_LIB_DIR to the directory containing the built \
                 `rednoise` library if this is wrong.",
                default.display()
            );
            default
        }
    };

    // Re-run this script if the override changes.
    println!("cargo:rerun-if-env-changed=REDNOISE_LIB_DIR");

    println!("cargo:rustc-link-search=native={}", lib_dir.display());

    // The RedNoise library itself.
    println!("cargo:rustc-link-lib=dylib=rednoise");

    // The engine is C++, so we must also link the C++ standard library and
    // (on Linux) the OpenMP runtime it uses for parallelism.
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" => {
            println!("cargo:rustc-link-lib=dylib=stdc++");
            // OpenMP runtime. May be optional depending on how the library was
            // built; emit it so a library compiled with OpenMP links cleanly.
            println!("cargo:rustc-link-lib=dylib=gomp");
        }
        "macos" => {
            println!("cargo:rustc-link-lib=dylib=c++");
        }
        _ => {
            // On Windows (MSVC) the C++ runtime is pulled in automatically by
            // the toolchain, so nothing extra is emitted here.
        }
    }
}
