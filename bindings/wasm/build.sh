#!/usr/bin/env bash
# Build the RedNoise WebAssembly module with Emscripten.
#
# Requires the Emscripten SDK (emsdk) to be installed and activated so that
# `emcc` is on PATH. See https://emscripten.org/docs/getting_started/downloads.html
#
# Run from this directory (bindings/wasm):
#     bash build.sh
#
# Produces dist/rednoise.mjs (an ES6 module factory) and dist/rednoise.wasm.

set -euo pipefail

# Resolve paths relative to this script so it works from any CWD.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "${ROOT_DIR}"

mkdir -p "${SCRIPT_DIR}/dist"

# Engine sources (compiled together with the wasm entry point).
FRAMEWORK_SRCS=(
	framework/Canvas.cpp
	framework/CanvasPoint.cpp
	framework/CanvasTriangle.cpp
	framework/Colour.cpp
	framework/ModelTriangle.cpp
	framework/TextureMap.cpp
	framework/TexturePoint.cpp
	framework/Utils.cpp
)

ENGINE_SRCS=(
	src/BDPT.cpp
	src/BVH.cpp
	src/Camera.cpp
	src/capi.cpp
	src/Clouds.cpp
	src/ColourUtil.cpp
	src/Drawing.cpp
	src/Geometry.cpp
	src/Interpolation.cpp
	src/IrradianceCache.cpp
	src/KdTree.cpp
	src/Lines.cpp
	src/Materials.cpp
	src/Meshing.cpp
	src/Metropolis.cpp
	src/Mipmap.cpp
	src/Noise.cpp
	src/Nurbs.cpp
	src/ObjLoader.cpp
	src/Ocean.cpp
	src/OceanFFT.cpp
	src/Octree.cpp
	src/Photon.cpp
	src/Radiosity.cpp
	src/Renderer.cpp
	src/Scene.cpp
	src/SceneGraph.cpp
	src/Transform.cpp
	src/Voxel.cpp
)

emcc \
	-std=c++23 \
	-O3 \
	-I framework \
	-I src \
	-I include \
	-I third_party \
	"${SCRIPT_DIR}/wasm_api.cpp" \
	"${FRAMEWORK_SRCS[@]}" \
	"${ENGINE_SRCS[@]}" \
	-sMODULARIZE=1 \
	-sEXPORT_ES6=1 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sENVIRONMENT=web,node \
	-sEXPORTED_FUNCTIONS='["_rn_wasm_render","_rn_wasm_version","_malloc","_free"]' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","stringToUTF8","UTF8ToString","HEAPU8","lengthBytesUTF8"]' \
	-o "${SCRIPT_DIR}/dist/rednoise.mjs"

echo "Built ${SCRIPT_DIR}/dist/rednoise.mjs and dist/rednoise.wasm"
