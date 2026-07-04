# RedNoise — CMake build
#
# Generate a build:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
# Compile:
#   cmake --build build --config Release
# Run (from the repo root, so that assets/ resolves):
#   ./build/RedNoise            # or build/Release/RedNoise.exe on MSVC
#
# Headless render + tests need no SDL3:
#   cmake -B build -S . -DBUILD_APP=OFF
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#   ./build/render_headless     # writes render-*.ppm

cmake_minimum_required(VERSION 3.20)
project(RedNoise CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_APP "Build the RedNoise application (requires SDL3)" ON)
option(BUILD_TESTS "Build the unit tests" ON)
option(BUILD_HEADLESS "Build the headless PPM renderer (no SDL3)" ON)

# Optional: multithread the ray tracer. Correctness does not depend on it, so a
# missing OpenMP just falls back to single-threaded.
find_package(OpenMP QUIET)
function(link_openmp target)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
    endif ()
endfunction()

# Warning flags shared by every target we compile (glm is header-only).
if (MSVC)
    add_compile_options(/W3 /Zc:wchar_t)
else ()
    add_compile_options(-Wall -Wextra -Wcast-align -Werror=return-type
            -Wno-unused-parameter -Wno-unused-variable -Wno-ignored-attributes)
endif ()

# The SDL-free "engine": geometry, camera, drawing and rendering into a Canvas.
# Shared by the app, the headless renderer and the tests.
set(ENGINE_SOURCES
        src/Interpolation.cpp
        src/ObjLoader.cpp
        src/Camera.cpp
        src/Geometry.cpp
        src/Drawing.cpp
        src/Renderer.cpp
        src/Noise.cpp
        src/BVH.cpp
        framework/Canvas.cpp
        framework/CanvasPoint.cpp
        framework/CanvasTriangle.cpp
        framework/Colour.cpp
        framework/ModelTriangle.cpp
        framework/TextureMap.cpp
        framework/TexturePoint.cpp
        framework/Utils.cpp)

# --- Application (needs SDL3) ------------------------------------------------
if (BUILD_APP)
    find_package(SDL3 REQUIRED CONFIG)
    add_executable(RedNoise src/RedNoise.cpp framework/DrawingWindow.cpp ${ENGINE_SOURCES})
    target_include_directories(RedNoise PRIVATE framework third_party src)
    target_link_libraries(RedNoise PRIVATE SDL3::SDL3)
    link_openmp(RedNoise)
    if (NOT MSVC)
        target_compile_options(RedNoise PRIVATE
                "$<$<CONFIG:Release>:-O3;-march=native;-mtune=native>"
                "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-mtune=native>"
                "$<$<CONFIG:Debug>:-O2;-fno-omit-frame-pointer;-g>")
    endif ()
endif ()

# --- Headless PPM renderer (no SDL3) ----------------------------------------
if (BUILD_HEADLESS)
    add_executable(render_headless tools/render_headless.cpp ${ENGINE_SOURCES})
    target_include_directories(render_headless PRIVATE framework third_party src)
    link_openmp(render_headless)

    # Fractal-terrain generator (only needs the noise module).
    add_executable(gen_terrain tools/gen_terrain.cpp src/Noise.cpp)
    target_include_directories(gen_terrain PRIVATE src third_party)

    # Orbiting-camera animation to a PPM frame sequence.
    add_executable(animate tools/animate.cpp ${ENGINE_SOURCES})
    target_include_directories(animate PRIVATE framework third_party src)
    link_openmp(animate)
endif ()

# --- Unit tests (no SDL3) ---------------------------------------------------
if (BUILD_TESTS)
    enable_testing()
    add_executable(rednoise_tests tests/tests.cpp ${ENGINE_SOURCES})
    target_include_directories(rednoise_tests PRIVATE framework third_party src)
    link_openmp(rednoise_tests)
    add_test(NAME unit COMMAND rednoise_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()
