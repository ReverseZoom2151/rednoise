// Render an orbiting camera animation to numbered PPM frames, ready to be
// assembled into a video or GIF (e.g. `ffmpeg -i frame-%03d.ppm out.mp4`).
//
// Usage: animate <objPath> <framePrefix> [frames] [easing]
//   easing = linear | smooth | reciprocal   (default reciprocal)
//
// `reciprocal` is a harmonic ease-in-out: per-frame angular velocity is weighted
// by the reciprocal of the distance from the mid-frame (fast in the middle, slow
// at the ends), the hand-rolled easing the reference COMS30020 assignment used.

#include "Camera.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include <Canvas.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Eased parameter in [0,1] for each of `frames` steps under the chosen mode.
static std::vector<float> easedParameters(int frames, const std::string &mode) {
	std::vector<float> t(frames, 0.0f);
	if (frames <= 1)
		return t;
	if (mode == "reciprocal") {
		// Velocity weight peaks at the middle frame (reciprocal of distance to it),
		// so the orbit accelerates then decelerates. Cumulative + normalised.
		std::vector<float> weight(frames);
		float mid = (frames - 1) / 2.0f;
		for (int i = 0; i < frames; i++)
			weight[i] = 1.0f / (1.0f + std::abs(i - mid));
		float acc = 0.0f, total = 0.0f;
		for (float w : weight)
			total += w;
		for (int i = 0; i < frames; i++) {
			t[i] = acc / total;
			acc += weight[i];
		}
	} else {
		for (int i = 0; i < frames; i++) {
			float lin = static_cast<float>(i) / frames;
			t[i] = (mode == "smooth") ? lin * lin * (3.0f - 2.0f * lin) : lin; // smoothstep or linear
		}
	}
	return t;
}

int main(int argc, char **argv) {
	if (argc < 3) {
		std::printf("usage: animate <obj> <framePrefix> [frames] [linear|smooth|reciprocal]\n");
		return 1;
	}
	std::string objPath = argv[1];
	std::string prefix = argv[2];
	int frames = (argc > 3) ? std::atoi(argv[3]) : 36;
	std::string easing = (argc > 4) ? argv[4] : "reciprocal";

	std::vector<ModelTriangle> model = loadOBJ(objPath, 0.35f);
	if (model.empty()) {
		std::printf("no triangles loaded from %s\n", objPath.c_str());
		return 1;
	}

	std::vector<float> t = easedParameters(frames, easing);
	const int W = 320, H = 240;
	Canvas canvas(W, H);
	for (int i = 0; i < frames; i++) {
		Camera camera(W, H, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
		camera.orbitY(2.0f * 3.14159265f * t[i], glm::vec3(0.0f)); // one eased full turn
		renderRasterised(model, camera, canvas);
		char name[512];
		std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), i);
		canvas.savePPM(name);
	}
	std::printf("wrote %d frames (%s easing): %s-000.ppm ...\n"
	            "assemble with e.g.  ffmpeg -framerate 24 -i %s-%%03d.ppm out.mp4\n",
	            frames, easing.c_str(), prefix.c_str(), prefix.c_str());
	return 0;
}
