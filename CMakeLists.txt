# RedNoise — CMake build
#
# Generate a build:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
# Compile:
#   cmake --build build --target RedNoise --config Release
# Run (from the repo root, so that assets/ resolves):
#   ./build/RedNoise            # or build/Release/RedNoise.exe on MSVC
#
# You only need to re-generate if you edit this file; otherwise just recompile.

cmake_minimum_required(VERSION 3.12)
project(RedNoise CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

# The "sdw" teaching framework (headers + implementations co-located).
file(GLOB FRAMEWORK_SOURCES CONFIGURE_DEPENDS framework/*.cpp)

add_executable(RedNoise
        src/RedNoise.cpp
        ${FRAMEWORK_SOURCES})

target_include_directories(RedNoise PRIVATE
        framework
        third_party
        ${SDL2_INCLUDE_DIRS})

if (MSVC)
    target_compile_options(RedNoise PRIVATE /W3 /Zc:wchar_t)
    set(DEBUG_OPTIONS /MTd)
    set(RELEASE_OPTIONS /MT /GF /Gy /O2 /fp:fast)
    if (NOT DEFINED SDL2_LIBRARIES)
        set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main)
    endif()
else ()
    target_compile_options(RedNoise PRIVATE
            -Wall
            -Wextra
            -Wcast-align
            -Werror=return-type
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-ignored-attributes)
    set(DEBUG_OPTIONS -O2 -fno-omit-frame-pointer -g)
    set(RELEASE_OPTIONS -O3 -march=native -mtune=native)
endif()

target_compile_options(RedNoise PRIVATE "$<$<CONFIG:RelWithDebInfo>:${RELEASE_OPTIONS}>")
target_compile_options(RedNoise PRIVATE "$<$<CONFIG:Release>:${RELEASE_OPTIONS}>")
target_compile_options(RedNoise PRIVATE "$<$<CONFIG:Debug>:${DEBUG_OPTIONS}>")

target_link_libraries(RedNoise PRIVATE ${SDL2_LIBRARIES})
