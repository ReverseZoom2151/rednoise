//! Safe Rust bindings for the [RedNoise] CPU software renderer.
//!
//! RedNoise is a C++ software renderer exposing a stable C ABI. This crate
//! wraps that ABI in a small, safe API: load a Wavefront OBJ scene, render it
//! with one of several modes, and save the result to a PNG.
//!
//! [RedNoise]: https://github.com/ReverseZoom2151/rednoise
//!
//! # Example
//!
//! ```no_run
//! use rednoise::{Scene, Mode};
//!
//! # fn main() -> Result<(), rednoise::Error> {
//! let scene = Scene::load_obj("assets/cornell-box.obj", 0.35)?;
//! println!("loaded {} triangles", scene.triangle_count());
//!
//! let image = scene.render(Mode::Pathtraced, 400, 300, 4.0, 64)?;
//! image.save_png("cornell.png")?;
//!
//! println!("rendered with RedNoise {}", rednoise::version());
//! # Ok(())
//! # }
//! ```
//!
//! The crate builds the C++ engine from source at build time (via the `cc`
//! crate), so it needs only a C++23 compiler - no prebuilt library and no
//! network access. See the crate README for details.

use std::ffi::{CStr, CString};
use std::fmt;

/// Raw FFI declarations mirroring `include/rednoise/rednoise.h`.
mod ffi {
    use std::os::raw::{c_char, c_float, c_int};

    /// Opaque scene handle. Never constructed on the Rust side.
    #[repr(C)]
    pub struct rn_scene {
        _private: [u8; 0],
    }

    extern "C" {
        pub fn rn_scene_load_obj(path: *const c_char, scale: c_float) -> *mut rn_scene;
        pub fn rn_scene_free(scene: *mut rn_scene);
        pub fn rn_scene_triangle_count(scene: *const rn_scene) -> c_int;
        pub fn rn_render(
            scene: *const rn_scene,
            mode: c_int,
            width: c_int,
            height: c_int,
            cam_z: c_float,
            samples: c_int,
            rgba: *mut u8,
        ) -> c_int;
        pub fn rn_save_png(
            path: *const c_char,
            width: c_int,
            height: c_int,
            rgba: *const u8,
        ) -> c_int;
        pub fn rn_version() -> *const c_char;
    }
}

/// Which renderer to run.
///
/// The discriminants match the C `rn_render_mode` enum exactly.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Mode {
    /// Wireframe.
    Wireframe = 0,
    /// Filled rasteriser.
    Rasterised = 1,
    /// Ray tracer.
    Raytraced = 2,
    /// Path tracer (uses the `samples` parameter).
    Pathtraced = 3,
}

/// Errors returned by this crate.
#[derive(Debug)]
pub enum Error {
    /// `rn_scene_load_obj` returned NULL (missing/invalid OBJ, etc.).
    LoadFailed,
    /// `rn_render` returned 0 (invalid arguments).
    RenderFailed,
    /// `rn_save_png` returned 0 (could not write the file).
    SaveFailed,
    /// A supplied path contained an interior NUL byte and could not be passed
    /// to C.
    NulByte,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let msg = match self {
            Error::LoadFailed => "failed to load OBJ scene",
            Error::RenderFailed => "render failed (invalid arguments)",
            Error::SaveFailed => "failed to save PNG",
            Error::NulByte => "path contained an interior NUL byte",
        };
        f.write_str(msg)
    }
}

impl std::error::Error for Error {}

/// A loaded scene.
///
/// Owns the underlying C `rn_scene` and frees it on drop. The handle is a raw
/// pointer into non-thread-safe C++ state, so `Scene` is intentionally neither
/// `Send` nor `Sync`.
pub struct Scene(*mut ffi::rn_scene);

impl Scene {
    /// Load a Wavefront OBJ (and its `.mtl`) as a scene, scaling vertices by
    /// `scale`.
    pub fn load_obj(path: &str, scale: f32) -> Result<Scene, Error> {
        let c_path = CString::new(path).map_err(|_| Error::NulByte)?;
        // Safety: `c_path` is a valid NUL-terminated string for the duration of
        // the call.
        let ptr = unsafe { ffi::rn_scene_load_obj(c_path.as_ptr(), scale) };
        if ptr.is_null() {
            Err(Error::LoadFailed)
        } else {
            Ok(Scene(ptr))
        }
    }

    /// Number of triangles in the scene.
    pub fn triangle_count(&self) -> u32 {
        // Safety: `self.0` is a valid, non-null scene pointer.
        let count = unsafe { ffi::rn_scene_triangle_count(self.0) };
        count.max(0) as u32
    }

    /// Render the scene into a fresh [`Image`].
    ///
    /// The camera sits at `(0, 0, cam_z)` aimed at the origin. `samples` is the
    /// per-pixel sample count used by [`Mode::Pathtraced`] (ignored otherwise).
    pub fn render(
        &self,
        mode: Mode,
        width: u32,
        height: u32,
        cam_z: f32,
        samples: u32,
    ) -> Result<Image, Error> {
        let mut rgba = vec![0u8; width as usize * height as usize * 4];
        // Safety: `rgba` has exactly width*height*4 bytes as the ABI requires,
        // and `self.0` is a valid scene pointer.
        let ok = unsafe {
            ffi::rn_render(
                self.0,
                mode as i32,
                width as i32,
                height as i32,
                cam_z,
                samples as i32,
                rgba.as_mut_ptr(),
            )
        };
        if ok == 1 {
            Ok(Image {
                width,
                height,
                rgba,
            })
        } else {
            Err(Error::RenderFailed)
        }
    }
}

impl Drop for Scene {
    fn drop(&mut self) {
        // Safety: called exactly once, with the pointer this `Scene` owns.
        // `rn_scene_free` ignores NULL, but our pointer is always non-null.
        unsafe { ffi::rn_scene_free(self.0) };
    }
}

/// An RGBA8 image produced by [`Scene::render`].
pub struct Image {
    /// Image width in pixels.
    pub width: u32,
    /// Image height in pixels.
    pub height: u32,
    /// Row-major RGBA8 pixels, `width * height * 4` bytes.
    pub rgba: Vec<u8>,
}

impl Image {
    /// Write this image to `path` as a PNG.
    pub fn save_png(&self, path: &str) -> Result<(), Error> {
        let c_path = CString::new(path).map_err(|_| Error::NulByte)?;
        // Safety: `c_path` is a valid NUL-terminated string and `rgba` holds
        // width*height*4 bytes, matching the dimensions we pass.
        let ok = unsafe {
            ffi::rn_save_png(
                c_path.as_ptr(),
                self.width as i32,
                self.height as i32,
                self.rgba.as_ptr(),
            )
        };
        if ok == 1 {
            Ok(())
        } else {
            Err(Error::SaveFailed)
        }
    }
}

/// Library version, e.g. `"0.1.0"`.
pub fn version() -> String {
    // Safety: `rn_version` returns a pointer to a static NUL-terminated string
    // with 'static lifetime; we only borrow it for the duration of this call.
    let ptr = unsafe { ffi::rn_version() };
    if ptr.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned()
}
