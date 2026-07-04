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

cmake_minimum_required(VERSION 3.24) # FetchContent FIND_PACKAGE_ARGS (find-first-then-fetch)
project(RedNoise VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_APP "Build the RedNoise application (requires SDL3)" ON)
option(BUILD_TESTS "Build the unit tests" ON)
option(BUILD_HEADLESS "Build the headless PPM renderer (no SDL3)" ON)
option(BUILD_GPU "Build the OpenCL GPU path tracer (requires an OpenCL SDK)" OFF)
option(BUILD_LIB "Build the installable rednoise library + C API" ON)

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
            -Wno-unused-parameter -Wno-unused-variable -Wno-ignored-attributes
            -Wno-unknown-pragmas) # the library builds without OpenMP; #pragma omp is a no-op there
endif ()

# --- Dependencies: find-first, then fetch -----------------------------------
# The zero-effort default: for each dependency that can be built from source we
# first look for a system package (instant if you already have it) and only fetch
# the latest from upstream when it is missing. No version is pinned. Knobs:
#   -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER  force-fetch the newest, ignore system
#   -DFETCH_DEPENDENCIES=OFF                     never fetch (system/vendored only, offline)
option(FETCH_DEPENDENCIES "Allow fetching missing deps from upstream (OFF = system + vendored only)" ON)

# --- glm (header-only vector maths) ---
add_library(glm_headers INTERFACE)
if (FETCH_DEPENDENCIES)
    include(FetchContent)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG master # latest upstream, used only if no system glm is found
            GIT_SHALLOW TRUE
            FIND_PACKAGE_ARGS NAMES glm)
    FetchContent_MakeAvailable(glm)
    if (TARGET glm::glm)
        target_link_libraries(glm_headers INTERFACE glm::glm)
    elseif (TARGET glm)
        target_link_libraries(glm_headers INTERFACE glm)
    endif ()
    message(STATUS "glm: system package if present, else fetched from upstream")
else ()
    target_include_directories(glm_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    message(STATUS "glm: using vendored third_party/glm (FETCH_DEPENDENCIES=OFF)")
endif ()

# glm's include path as a plain build-time include (a genex resolved per config).
# The installable library uses this instead of linking glm_headers, so glm - a
# build-only header dependency - never leaks into the library's export set.
if (TARGET glm::glm)
    set(RN_GLM_INCLUDE "$<TARGET_PROPERTY:glm::glm,INTERFACE_INCLUDE_DIRECTORIES>")
elseif (TARGET glm)
    set(RN_GLM_INCLUDE "$<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>")
else ()
    set(RN_GLM_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
endif ()
# Each of our targets links glm_headers explicitly below. (Deliberately not
# link_libraries() at directory scope: that would leak onto fetched dependency
# targets like the OpenCL ICD loader and break their install/export sets.)

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
        src/Transform.cpp
        src/Scene.cpp
        src/Photon.cpp
        src/Materials.cpp
        src/Radiosity.cpp
        src/BDPT.cpp
        src/Metropolis.cpp
        src/Ocean.cpp
        src/Meshing.cpp
        src/Octree.cpp
        src/KdTree.cpp
        src/Clouds.cpp
        src/Voxel.cpp
        src/Nurbs.cpp
        src/SceneGraph.cpp
        src/OceanFFT.cpp
        src/ColourUtil.cpp
        src/Lines.cpp
        src/Mipmap.cpp
        src/IrradianceCache.cpp
        framework/Canvas.cpp
        framework/CanvasPoint.cpp
        framework/CanvasTriangle.cpp
        framework/Colour.cpp
        framework/ModelTriangle.cpp
        framework/TextureMap.cpp
        framework/TexturePoint.cpp
        framework/Utils.cpp)

# --- Installable library + C ABI --------------------------------------------
# A self-contained library exposing the stable C API (include/rednoise/rednoise.h).
# glm is compiled in (build-only include) and OpenMP is intentionally not linked,
# so the export set has no external dependencies - downstream links only
# `rednoise::rednoise` and needs nothing else. Installs a CMake package config so
# consumers can `find_package(rednoise)`.
if (BUILD_LIB)
    add_library(rednoise ${ENGINE_SOURCES} src/capi.cpp)
    add_library(rednoise::rednoise ALIAS rednoise)
    target_include_directories(rednoise
            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE framework src "${RN_GLM_INCLUDE}")
    set_target_properties(rednoise PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS rednoise EXPORT rednoiseTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT rednoiseTargets NAMESPACE rednoise::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rednoise)
    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rednoiseConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/rednoiseConfig.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rednoise)
    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/rednoiseConfigVersion.cmake
            VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/rednoiseConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/rednoiseConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rednoise)

    # A pure-C example that links the library - proves the ABI.
    add_executable(c_consumer examples/c_consumer.c)
    target_link_libraries(c_consumer PRIVATE rednoise)
    set_target_properties(c_consumer PROPERTIES LINKER_LANGUAGE CXX)
endif ()

# --- Application (needs SDL3) ------------------------------------------------
if (BUILD_APP)
    if (FETCH_DEPENDENCIES)
        include(FetchContent)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        # System SDL3 (vcpkg / package manager) if present - instant; else fetch
        # and build the latest from source.
        FetchContent_Declare(SDL3
                GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                GIT_TAG main # latest upstream SDL3, used only if no system SDL3 is found
                GIT_SHALLOW TRUE
                FIND_PACKAGE_ARGS CONFIG)
        FetchContent_MakeAvailable(SDL3)
        message(STATUS "SDL3: system package if present, else fetched + built from source")
    else ()
        find_package(SDL3 REQUIRED CONFIG)
    endif ()
    add_executable(RedNoise src/RedNoise.cpp framework/DrawingWindow.cpp ${ENGINE_SOURCES})
    target_include_directories(RedNoise PRIVATE framework src)
    target_link_libraries(RedNoise PRIVATE SDL3::SDL3 glm_headers)
    link_openmp(RedNoise)
    # Put the SDL3 runtime library next to the exe so it runs out of the box.
    # (A system package usually deploys it; a from-source fetch does not.)
    if (WIN32)
        add_custom_command(TARGET RedNoise POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:RedNoise>")
    endif ()
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
    target_include_directories(render_headless PRIVATE framework src)
    target_link_libraries(render_headless PRIVATE glm_headers)
    link_openmp(render_headless)

    # Fractal-terrain generator (only needs the noise module).
    add_executable(gen_terrain tools/gen_terrain.cpp src/Noise.cpp)
    target_include_directories(gen_terrain PRIVATE src)
    target_link_libraries(gen_terrain PRIVATE glm_headers)

    # Orbiting-camera animation to a PPM frame sequence.
    add_executable(animate tools/animate.cpp ${ENGINE_SOURCES})
    target_include_directories(animate PRIVATE framework src)
    target_link_libraries(animate PRIVATE glm_headers)
    link_openmp(animate)

    # Animated Gerstner-wave ocean to a PPM frame sequence.
    add_executable(render_ocean tools/render_ocean.cpp ${ENGINE_SOURCES})
    target_include_directories(render_ocean PRIVATE framework src)
    target_link_libraries(render_ocean PRIVATE glm_headers)
    link_openmp(render_ocean)
endif ()

# --- GPU path tracer (OpenCL) -----------------------------------------------
# Vendor-neutral: standard OpenCL via find_package, runs on any conformant
# device. The kernel gpu/pathtracer.cl is loaded at runtime; run from the repo
# root. If no OpenCL SDK is found we warn and skip rather than fail the build.
# OPENCL_TARGET_VERSION is the API level the code is COMPILED to use: 120 is the
# maximum-compatibility floor (runs on any conformant runtime); set it to 300 to
# require an OpenCL 3.0 runtime. It is independent of the SDK version found -
# find_package always picks up the newest headers/loader installed.
set(OPENCL_TARGET_VERSION 120 CACHE STRING "OpenCL API level to target (120 = broadest, 300 = require 3.0)")
if (BUILD_GPU)
    set(_ocl_ok FALSE)
    # System OpenCL SDK (a vendor SDK / CUDA) if present - instant; otherwise
    # fetch and build the latest Khronos headers + ICD loader. The loader always
    # dispatches at runtime to the installed vendor driver - that runtime is the
    # one piece that cannot be fetched (it ships with the GPU driver).
    find_package(OpenCL)
    if (OpenCL_FOUND)
        message(STATUS "OpenCL: using system SDK ${OpenCL_VERSION_STRING}")
        set(_ocl_libs OpenCL::OpenCL)
        set(_ocl_ok TRUE)
    elseif (FETCH_DEPENDENCIES)
        include(FetchContent)
        FetchContent_Declare(OpenCLHeaders
                GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
                GIT_TAG main GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(OpenCLHeaders)
        set(OPENCL_ICD_LOADER_HEADERS_DIR ${openclheaders_SOURCE_DIR} CACHE PATH "" FORCE)
        FetchContent_Declare(OpenCLICDLoader
                GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
                GIT_TAG main GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(OpenCLICDLoader)
        set(_ocl_libs OpenCL OpenCL::Headers)
        set(_ocl_ok TRUE)
        message(STATUS "OpenCL: no system SDK; fetched latest Khronos headers + ICD loader")
    endif ()
    if (_ocl_ok)
        message(STATUS "gpu_pathtracer: compiling to target OpenCL API ${OPENCL_TARGET_VERSION}")
        add_executable(gpu_pathtracer gpu/gpu_pathtracer.cpp ${ENGINE_SOURCES})
        target_include_directories(gpu_pathtracer PRIVATE framework src)
        target_compile_definitions(gpu_pathtracer PRIVATE CL_TARGET_OPENCL_VERSION=${OPENCL_TARGET_VERSION})
        target_link_libraries(gpu_pathtracer PRIVATE ${_ocl_libs} glm_headers)
    else ()
        message(WARNING "BUILD_GPU=ON but no OpenCL SDK found; skipping gpu_pathtracer. "
                "Enable FETCH_DEPENDENCIES, or install any vendor's OpenCL SDK.")
    endif ()
endif ()

# --- Unit tests (no SDL3) ---------------------------------------------------
if (BUILD_TESTS)
    enable_testing()
    add_executable(rednoise_tests tests/tests.cpp ${ENGINE_SOURCES})
    target_include_directories(rednoise_tests PRIVATE framework src)
    target_link_libraries(rednoise_tests PRIVATE glm_headers)
    link_openmp(rednoise_tests)
    add_test(NAME unit COMMAND rednoise_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()
