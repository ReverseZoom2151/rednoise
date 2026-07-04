//! Render the Cornell box with the path tracer and save it to a PNG.
//!
//! The crate compiles the C++ engine from source, so no prebuilt library is
//! needed. Run this example from the repository root so the asset path
//! resolves:
//!
//! ```sh
//! cargo run --manifest-path bindings/rust/Cargo.toml --example render
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
