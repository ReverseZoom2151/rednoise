//! Render the Cornell box with the path tracer and save it to a PNG.
//!
//! Build the native C++ library first, then run this example from the crate
//! directory (`bindings/rust`), pointing the linker at the build output:
//!
//! ```sh
//! REDNOISE_LIB_DIR=<repo>/build cargo run --example render
//! ```
//!
//! The OBJ path below is relative to the current working directory, so run it
//! from the repository root (or adjust the path).

use rednoise::{Mode, Scene};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let scene = Scene::load_obj("assets/cornell-box.obj", 0.35)?;

    println!("triangles: {}", scene.triangle_count());
    println!("rednoise version: {}", rednoise::version());

    let image = scene.render(Mode::Pathtraced, 400, 300, 4.0, 64)?;
    image.save_png("cornell.png")?;

    println!(
        "saved cornell.png ({}x{}, {} bytes)",
        image.width,
        image.height,
        image.rgba.len()
    );

    Ok(())
}
