"""Python bindings for the RedNoise CPU software renderer.

This is a pure-Python binding built on :mod:`ctypes` over the stable C ABI
declared in ``include/rednoise/rednoise.h``. There is no compiled extension;
the binding loads the shared library (``librednoise.so`` on Linux,
``librednoise.dylib`` on macOS, ``rednoise.dll`` on Windows) at runtime.

The shared library is *not* bundled with this package. Build it from the C++
project::

    cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF
    cmake --build build

Then either place the library on a path the loader searches, or point the
``REDNOISE_LIBRARY`` environment variable at the built file.

Typical usage::

    import rednoise

    scene = rednoise.Scene.load_obj("assets/cornell-box.obj")
    print(scene.triangle_count, rednoise.version())
    image = scene.render(mode="pathtraced", width=400, height=300, samples=64)
    image.save("cornell.png")
    scene.close()
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
from enum import IntEnum
from typing import Optional, Tuple, Union

__all__ = ["RenderMode", "Scene", "Image", "version"]

__version__ = "0.1.0"


# ---------------------------------------------------------------------------
# Render modes
# ---------------------------------------------------------------------------
class RenderMode(IntEnum):
    """Which renderer to run, mirroring the C ``rn_render_mode`` enum."""

    WIREFRAME = 0
    RASTERISED = 1
    RAYTRACED = 2
    PATHTRACED = 3

    @classmethod
    def coerce(cls, value: Union["RenderMode", int, str]) -> "RenderMode":
        """Coerce an int, name string, or :class:`RenderMode` into a member.

        Strings are matched case-insensitively, so ``"pathtraced"``,
        ``"PATHTRACED"`` and ``"path_traced"`` all resolve to
        :attr:`RenderMode.PATHTRACED`.
        """
        if isinstance(value, cls):
            return value
        if isinstance(value, bool):
            # bool is a subclass of int; reject it to avoid surprises.
            raise TypeError("render mode must not be a bool")
        if isinstance(value, int):
            return cls(value)
        if isinstance(value, str):
            key = value.strip().upper().replace("-", "").replace("_", "").replace(" ", "")
            for member in cls:
                if member.name.replace("_", "") == key:
                    return member
            valid = ", ".join(m.name.lower() for m in cls)
            raise ValueError(f"unknown render mode {value!r}; expected one of: {valid}")
        raise TypeError(f"render mode must be RenderMode, int or str, not {type(value).__name__}")


# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------
def _candidate_paths() -> list:
    """Return candidate local build locations for the shared library."""
    here = os.path.dirname(os.path.abspath(__file__))
    # Walk up from this package to plausible project roots.
    roots = [
        here,  # next to the package itself
        os.path.dirname(here),  # bindings/python
        os.path.dirname(os.path.dirname(here)),  # bindings
        os.path.dirname(os.path.dirname(os.path.dirname(here))),  # project root
        os.getcwd(),  # wherever the process was launched from
    ]
    names = [
        "librednoise.so",
        "librednoise.dylib",
        "rednoise.dll",
        "librednoise.dll",
    ]
    subdirs = ["", "build", os.path.join("build", "Release"), os.path.join("build", "Debug")]

    seen = set()
    candidates = []
    for root in roots:
        for sub in subdirs:
            for name in names:
                path = os.path.normpath(os.path.join(root, sub, name))
                if path not in seen:
                    seen.add(path)
                    candidates.append(path)
    return candidates


def _find_library_path() -> Optional[str]:
    """Locate the shared library path, or return ``None`` if not found."""
    # 1. Explicit override via environment variable.
    env = os.environ.get("REDNOISE_LIBRARY")
    if env:
        if os.path.isfile(env):
            return env
        raise OSError(
            f"REDNOISE_LIBRARY is set to {env!r} but that file does not exist."
        )

    # 2. System search path.
    found = ctypes.util.find_library("rednoise")
    if found:
        return found

    # 3. Common local build locations.
    for path in _candidate_paths():
        if os.path.isfile(path):
            return path

    return None


_LIB = None  # cached CDLL handle


def _load_library() -> ctypes.CDLL:
    """Load (once) and return the RedNoise shared library.

    Loading is lazy so that ``import rednoise`` never fails merely because the
    library has not been built yet. The first call that needs native code
    triggers the load and raises a helpful error if the library is missing.
    """
    global _LIB
    if _LIB is not None:
        return _LIB

    path = _find_library_path()
    if path is None:
        raise OSError(
            "Could not locate the RedNoise shared library "
            "(librednoise.so / librednoise.dylib / rednoise.dll).\n"
            "Build it with:\n"
            "    cmake -B build -S . -DBUILD_LIB=ON -DBUILD_APP=OFF\n"
            "    cmake --build build\n"
            "then set the REDNOISE_LIBRARY environment variable to the built "
            "library file, or place it on the loader search path."
        )

    try:
        lib = ctypes.CDLL(path)
    except OSError as exc:  # pragma: no cover - platform dependent
        raise OSError(f"Failed to load RedNoise library at {path!r}: {exc}") from exc

    _configure_signatures(lib)
    _LIB = lib
    return lib


def _configure_signatures(lib: ctypes.CDLL) -> None:
    """Declare argtypes/restype for every exported function."""
    # rn_scene *rn_scene_load_obj(const char *path, float scale);
    lib.rn_scene_load_obj.argtypes = [ctypes.c_char_p, ctypes.c_float]
    lib.rn_scene_load_obj.restype = ctypes.c_void_p

    # void rn_scene_free(rn_scene *scene);
    lib.rn_scene_free.argtypes = [ctypes.c_void_p]
    lib.rn_scene_free.restype = None

    # int rn_scene_triangle_count(const rn_scene *scene);
    lib.rn_scene_triangle_count.argtypes = [ctypes.c_void_p]
    lib.rn_scene_triangle_count.restype = ctypes.c_int

    # int rn_render(const rn_scene *, rn_render_mode, int, int, float, int, unsigned char *);
    lib.rn_render.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.c_int,
        ctypes.c_char_p,
    ]
    lib.rn_render.restype = ctypes.c_int

    # int rn_save_png(const char *path, int width, int height, const unsigned char *rgba);
    lib.rn_save_png.argtypes = [
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_char_p,
    ]
    lib.rn_save_png.restype = ctypes.c_int

    # const char *rn_version(void);
    lib.rn_version.argtypes = []
    lib.rn_version.restype = ctypes.c_char_p


def _encode_path(path: Union[str, os.PathLike]) -> bytes:
    """Encode a filesystem path for the C ABI (``const char *``)."""
    return os.fsencode(os.fspath(path))


# ---------------------------------------------------------------------------
# Image
# ---------------------------------------------------------------------------
class Image:
    """An RGBA8 image returned by :meth:`Scene.render`.

    Holds the raw pixel bytes together with the image dimensions. Pixels are
    laid out row-major as ``width * height`` RGBA quadruplets.
    """

    __slots__ = ("_rgba", "_width", "_height")

    def __init__(self, rgba: bytes, width: int, height: int):
        expected = width * height * 4
        if len(rgba) != expected:
            raise ValueError(
                f"rgba buffer has {len(rgba)} bytes, expected {expected} "
                f"for a {width}x{height} RGBA8 image"
            )
        self._rgba = bytes(rgba)
        self._width = int(width)
        self._height = int(height)

    @property
    def rgba(self) -> bytes:
        """The raw RGBA8 pixel bytes (``width * height * 4`` bytes)."""
        return self._rgba

    @property
    def width(self) -> int:
        """Image width in pixels."""
        return self._width

    @property
    def height(self) -> int:
        """Image height in pixels."""
        return self._height

    @property
    def size(self) -> Tuple[int, int]:
        """The ``(width, height)`` image dimensions."""
        return (self._width, self._height)

    def save(self, path: Union[str, os.PathLike]) -> None:
        """Write the image to ``path`` as a PNG via the native ``rn_save_png``.

        Only the ``.png`` format is supported by the native writer. If Pillow
        is installed and a non-PNG extension is requested, the save is delegated
        to :meth:`to_pillow` instead.
        """
        path = os.fspath(path)
        ext = os.path.splitext(path)[1].lower()
        if ext and ext != ".png":
            # Fall back to Pillow for other formats if it is available.
            try:
                self.to_pillow().save(path)
                return
            except ImportError:
                raise ValueError(
                    f"native saving only supports .png; got {ext!r}. "
                    "Install Pillow (pip install rednoise[pillow]) to save other formats."
                )
        lib = _load_library()
        ok = lib.rn_save_png(_encode_path(path), self._width, self._height, self._rgba)
        if ok != 1:
            raise RuntimeError(f"rn_save_png failed to write {path!r}")

    def to_pillow(self):
        """Build and return a :class:`PIL.Image.Image` (requires Pillow).

        Pillow is imported lazily and is an optional dependency; install it via
        ``pip install rednoise[pillow]``.
        """
        try:
            from PIL import Image as PILImage
        except ImportError as exc:  # pragma: no cover - optional dependency
            raise ImportError(
                "Pillow is required for to_pillow(); install it with "
                "'pip install rednoise[pillow]'"
            ) from exc
        return PILImage.frombytes("RGBA", (self._width, self._height), self._rgba)

    def __repr__(self) -> str:
        return f"<rednoise.Image {self._width}x{self._height} RGBA8>"


# ---------------------------------------------------------------------------
# Scene
# ---------------------------------------------------------------------------
class Scene:
    """A loaded RedNoise scene backed by a native ``rn_scene`` handle.

    Create one with :meth:`load_obj`. The underlying native scene is released
    exactly once via :meth:`close`, ``__del__``, or context-manager exit.
    """

    __slots__ = ("_handle",)

    def __init__(self, handle: int):
        # handle is a non-NULL c_void_p value (an int address).
        if not handle:
            raise ValueError("Scene requires a non-NULL native handle")
        self._handle = handle

    @classmethod
    def load_obj(cls, path: Union[str, os.PathLike], scale: float = 0.35) -> "Scene":
        """Load a Wavefront OBJ (and its .mtl) as a scene.

        :param path: Path to the ``.obj`` file.
        :param scale: Uniform scale applied to vertices (default ``0.35``).
        :raises FileNotFoundError: if the OBJ file does not exist.
        :raises RuntimeError: if the native loader returns NULL.
        """
        path = os.fspath(path)
        if not os.path.isfile(path):
            raise FileNotFoundError(f"OBJ file not found: {path!r}")
        lib = _load_library()
        handle = lib.rn_scene_load_obj(_encode_path(path), ctypes.c_float(scale))
        if not handle:
            raise RuntimeError(f"rn_scene_load_obj failed to load {path!r}")
        return cls(handle)

    @property
    def triangle_count(self) -> int:
        """Number of triangles in the scene."""
        self._check_open()
        lib = _load_library()
        return int(lib.rn_scene_triangle_count(self._handle))

    def render(
        self,
        mode: Union[RenderMode, int, str] = "raytraced",
        width: int = 640,
        height: int = 480,
        cam_z: float = 4.0,
        samples: int = 64,
    ) -> Image:
        """Render the scene and return an :class:`Image`.

        :param mode: A :class:`RenderMode`, its integer value, or a name string
            such as ``"wireframe"``, ``"rasterised"``, ``"raytraced"`` or
            ``"pathtraced"``.
        :param width: Output width in pixels.
        :param height: Output height in pixels.
        :param cam_z: Camera Z position; the camera at ``(0, 0, cam_z)`` looks
            at the origin.
        :param samples: Per-pixel sample count (only used by ``PATHTRACED``).
        :raises RuntimeError: if the native renderer reports failure.
        """
        self._check_open()
        render_mode = RenderMode.coerce(mode)
        width = int(width)
        height = int(height)
        if width <= 0 or height <= 0:
            raise ValueError(f"width and height must be positive, got {width}x{height}")

        buf = ctypes.create_string_buffer(width * height * 4)
        lib = _load_library()
        ok = lib.rn_render(
            self._handle,
            int(render_mode),
            width,
            height,
            ctypes.c_float(cam_z),
            int(samples),
            buf,
        )
        if ok != 1:
            raise RuntimeError(
                f"rn_render failed (mode={render_mode.name}, {width}x{height})"
            )
        return Image(buf.raw, width, height)

    def close(self) -> None:
        """Release the native scene. Safe to call multiple times."""
        handle = self._handle
        if handle:
            self._handle = 0
            # Guard against interpreter-shutdown ordering issues.
            if _LIB is not None:
                _LIB.rn_scene_free(handle)

    def _check_open(self) -> None:
        if not self._handle:
            raise RuntimeError("operation on a closed Scene")

    def __enter__(self) -> "Scene":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __del__(self):
        # Best-effort cleanup; swallow everything during interpreter shutdown.
        try:
            self.close()
        except Exception:
            pass

    def __repr__(self) -> str:
        if not self._handle:
            return "<rednoise.Scene closed>"
        return f"<rednoise.Scene handle=0x{self._handle:x}>"


# ---------------------------------------------------------------------------
# Module-level helpers
# ---------------------------------------------------------------------------
def version() -> str:
    """Return the native library version string, e.g. ``"0.1.0"``."""
    lib = _load_library()
    raw = lib.rn_version()
    if raw is None:
        return ""
    return raw.decode("utf-8", "replace")
