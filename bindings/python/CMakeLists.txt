# CMake driver for the RedNoise Python wheel (scikit-build-core).
#
# This does NOT build a Python extension module. The binding is pure ctypes; all
# we need is the native shared library. So we pull in the parent C++ project and
# build ONLY the installable `rednoise` library (its stable C ABI), then install
# the resulting .so / .dll / .dylib INTO the importable `rednoise/` package dir
# so the wheel carries it and __init__.py finds it next to the package.

cmake_minimum_required(VERSION 3.24)
project(rednoise_python LANGUAGES C CXX)

# Build only the library from the parent project. Force these into the cache
# BEFORE add_subdirectory so the parent's option() calls pick them up. We keep
# the app (SDL3), tests, headless tools, GPU and Vulkan paths off, and disable
# fetching so the build is offline (glm is vendored in third_party/).
set(BUILD_APP OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_HEADLESS OFF CACHE BOOL "" FORCE)
set(BUILD_GPU OFF CACHE BOOL "" FORCE)
set(BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(BUILD_LIB ON CACHE BOOL "" FORCE)
set(FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
# ctypes loads a SHARED library, so build rednoise as a .dll/.so/.dylib (with the
# C ABI exported - see WINDOWS_EXPORT_ALL_SYMBOLS on the target), not a static lib.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

# The C++ project root is two levels up (bindings/python -> repo root).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. rednoise_root)

# Install the built native library into the importable package directory so it
# ships inside the wheel right next to rednoise/__init__.py.
install(TARGETS rednoise
        RUNTIME DESTINATION rednoise
        LIBRARY DESTINATION rednoise
        ARCHIVE DESTINATION rednoise)
