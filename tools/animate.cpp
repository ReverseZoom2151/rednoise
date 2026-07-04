// Render an orbiting camera animation to numbered PPM frames, ready to be
// assembled into a video or GIF (e.g. `ffmpeg -i frame-%03d.ppm out.mp4`).
//
// Usage: animate <objPath> <framePrefix> [frames]

#include "Camera.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include <Canvas.h>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <string>
#include <vector>

int main(int argc, char **argv) {
	if (argc < 3) {
		std::printf("usage: animate <obj> <framePrefix> [frames]\n");
		return 1;
	}
	std::string objPath = argv[1];
	std::string prefix = argv[2];
	int frames = (argc > 3) ? std::atoi(argv[3]) : 36;

	std::vector<ModelTriangle> model = loadOBJ(objPath, 0.35f);
	if (model.empty()) {
		std::printf("no triangles loaded from %s\n", objPath.c_str());
		return 1;
	}

	const int W = 320, H = 240;
	Canvas canvas(W, H);
	for (int i = 0; i < frames; i++) {
		Camera camera(W, H, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
		camera.orbitY(2.0f * 3.14159265f * i / frames, glm::vec3(0.0f)); // one full turn
		renderRasterised(model, camera, canvas);
		char name[512];
		std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), i);
		canvas.savePPM(name);
	}
	std::printf("wrote %d frames: %s-000.ppm ...\nassemble with e.g.  ffmpeg -framerate 24 -i %s-%%03d.ppm out.mp4\n",
	            frames, prefix.c_str(), prefix.c_str());
	return 0;
}
