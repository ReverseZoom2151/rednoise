# RedNoise — CMake build
#
# Generate a build:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
# Compile:
#   cmake --build build --config Release
# Run (from the repo root, so that assets/ resolves):
#   ./build/RedNoise            # or build/Release/RedNoise.exe on MSVC
#
# You only need to re-generate if you edit this file; otherwise just recompile.

cmake_minimum_required(VERSION 3.12)
project(RedNoise CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)

# The application (src/) and the "sdw" teaching framework (framework/).
file(GLOB FRAMEWORK_SOURCES CONFIGURE_DEPENDS framework/*.cpp)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(RedNoise ${APP_SOURCES} ${FRAMEWORK_SOURCES})
target_include_directories(RedNoise PRIVATE framework third_party src)

if (MSVC)
    target_compile_options(RedNoise PRIVATE /W3 /Zc:wchar_t)
else ()
    target_compile_options(RedNoise PRIVATE
            -Wall -Wextra -Wcast-align -Werror=return-type
            -Wno-unused-parameter -Wno-unused-variable -Wno-ignored-attributes)
    target_compile_options(RedNoise PRIVATE
            "$<$<CONFIG:Release>:-O3;-march=native;-mtune=native>"
            "$<$<CONFIG:RelWithDebInfo>:-O3;-march=native;-mtune=native>"
            "$<$<CONFIG:Debug>:-O2;-fno-omit-frame-pointer;-g>")
endif ()

target_link_libraries(RedNoise PRIVATE SDL3::SDL3)
