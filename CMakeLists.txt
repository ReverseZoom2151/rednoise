# RedNoise — CMake build
#
# Generate a build:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
# Compile:
#   cmake --build build --config Release
# Run (from the repo root, so that assets/ resolves):
#   ./build/RedNoise            # or build/Release/RedNoise.exe on MSVC
#
# Tests only (no SDL3 needed):
#   cmake -B build -S . -DBUILD_APP=OFF
#   cmake --build build --target rednoise_tests
#   ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.20)
project(RedNoise CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_APP "Build the RedNoise application (requires SDL3)" ON)
option(BUILD_TESTS "Build the unit tests" ON)

# Warning flags shared by every target we compile (glm is header-only).
if (MSVC)
    add_compile_options(/W3 /Zc:wchar_t)
else ()
    add_compile_options(-Wall -Wextra -Wcast-align -Werror=return-type
            -Wno-unused-parameter -Wno-unused-variable -Wno-ignored-attributes)
endif ()

file(GLOB FRAMEWORK_SOURCES CONFIGURE_DEPENDS framework/*.cpp)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# --- Application (needs SDL3) ------------------------------------------------
if (BUILD_APP)
    find_package(SDL3 REQUIRED CONFIG)
    add_executable(RedNoise ${APP_SOURCES} ${FRAMEWORK_SOURCES})
    target_include_directories(RedNoise PRIVATE framework third_party src)
    target_link_libraries(RedNoise PRIVATE SDL3::SDL3)
    if (NOT MSVC)
        target_compile_options(RedNoise PRIVATE
                "$<$<CONFIG:Release>:-O3;-march=native;-mtune=native>"
                "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-mtune=native>"
                "$<$<CONFIG:Debug>:-O2;-fno-omit-frame-pointer;-g>")
    endif ()
endif ()

# --- Unit tests (SDL-free) ---------------------------------------------------
if (BUILD_TESTS)
    enable_testing()
    add_executable(rednoise_tests
            tests/tests.cpp
            src/Interpolation.cpp
            src/ObjLoader.cpp
            framework/Colour.cpp
            framework/ModelTriangle.cpp
            framework/TexturePoint.cpp
            framework/Utils.cpp)
    target_include_directories(rednoise_tests PRIVATE framework third_party src)
    add_test(NAME unit COMMAND rednoise_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()
