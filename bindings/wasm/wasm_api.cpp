// Emscripten entry point for the RedNoise renderer.
//
// This exposes a tiny extern "C" surface that JavaScript can call through
// cwrap/ccall. Unlike the native C ABI in src/capi.cpp, loadOBJ() reads a file
// by path, and under Emscripten there is no host filesystem. We therefore write
// the incoming OBJ text into MEMFS (an in-memory filesystem Emscripten provides
// by default) and hand that path to loadOBJ().
//
// Memory for the output RGBA buffer is allocated and freed on the JS side via
// Module._malloc / Module._free; this function only fills it.

#include <emscripten/emscripten.h>

#include "Camera.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include <Canvas.h>
#include <ModelTriangle.h>

#include <cstdint>
#include <fstream>
#include <glm/glm.hpp>
#include <vector>

#include "rednoise/rednoise.h"

extern "C" {

// Render an OBJ (supplied as text) and copy the result into outRgba (an
// caller-provided buffer of width*height*4 bytes, RGBA8). Returns 1 on success,
// 0 on failure. mode maps to rn_render_mode: 0 wireframe, 1 rasterised,
// 2 raytraced, 3 pathtraced. samples is only used by the path tracer.
EMSCRIPTEN_KEEPALIVE
int rn_wasm_render(const char *objText, int objLen, int mode, int width, int height, int samples, float camZ,
                   unsigned char *outRgba) {
	if (!objText || objLen < 0 || !outRgba || width <= 0 || height <= 0)
		return 0;

	// Write the OBJ text to a MEMFS path so loadOBJ() (which reads a file path)
	// can parse it. loadOBJ resolves any mtllib reference relative to this file;
	// materials shipped in a separate .mtl are not available here, so faces fall
	// back to their default colour, which matches the headless C ABI behaviour.
	const char *objPath = "/tmp/scene.obj";
	{
		std::ofstream out(objPath, std::ios::binary | std::ios::trunc);
		if (!out)
			return 0;
		out.write(objText, static_cast<std::streamsize>(objLen));
		if (!out)
			return 0;
	}

	std::vector<ModelTriangle> model = loadOBJ(objPath, 1.0f);
	if (model.empty())
		return 0;

	// Camera at (0, 0, camZ) aimed at the origin, matching src/capi.cpp.
	Camera camera(width, height, 2.0f, glm::vec3(0.0f, 0.0f, camZ));
	camera.lookAt(glm::vec3(0.0f));

	Canvas canvas(width, height);
	switch (mode) {
	case RN_WIREFRAME:
		renderWireframe(model, camera, canvas);
		break;
	case RN_RASTERISED:
		renderRasterised(model, camera, canvas);
		break;
	case RN_RAYTRACED:
		renderRaytraced(model, camera, canvas);
		break;
	case RN_PATHTRACED:
		renderPathTraced(model, camera, canvas, samples);
		break;
	default:
		return 0;
	}

	// Canvas stores 0xAARRGGBB per pixel; unpack to RGBA8, matching capi.cpp.
	const int count = width * height;
	for (int i = 0; i < count; i++) {
		uint32_t p = canvas.pixels[static_cast<size_t>(i)];
		outRgba[i * 4 + 0] = static_cast<unsigned char>((p >> 16) & 0xFF);
		outRgba[i * 4 + 1] = static_cast<unsigned char>((p >> 8) & 0xFF);
		outRgba[i * 4 + 2] = static_cast<unsigned char>(p & 0xFF);
		outRgba[i * 4 + 3] = static_cast<unsigned char>((p >> 24) & 0xFF);
	}
	return 1;
}

// Library version string, e.g. "0.1.0".
EMSCRIPTEN_KEEPALIVE
const char *rn_wasm_version(void) {
	const char *v = rn_version();
	return v ? v : "0.1.2";
}

} // extern "C"
