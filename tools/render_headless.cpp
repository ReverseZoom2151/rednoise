// Headless renderer: draw the model to PPM files with no window/SDL, so the
// output can be inspected locally and produced in CI.
//
// Usage: render_headless [objPath] [outPrefix]
//   defaults: assets/cornell-box.obj  render
// Produces <outPrefix>-wireframe.ppm, -rasterised.ppm, -raytraced.ppm.

#include "Camera.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include <Canvas.h>
#include <ModelTriangle.h>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
	const int width = 320;
	const int height = 240;
	std::string objPath = (argc > 1) ? argv[1] : "assets/cornell-box.obj";
	std::string outPrefix = (argc > 2) ? argv[2] : "render";

	std::vector<ModelTriangle> model = loadOBJ(objPath, 0.35f);
	if (model.empty()) {
		std::cerr << "No triangles loaded from " << objPath << "\n";
		return 1;
	}

	Camera camera(width, height, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
	camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	Canvas canvas(width, height);

	renderWireframe(model, camera, canvas);
	canvas.savePPM(outPrefix + "-wireframe.ppm");
	renderRasterised(model, camera, canvas);
	canvas.savePPM(outPrefix + "-rasterised.ppm");
	renderRaytraced(model, camera, canvas);
	canvas.savePPM(outPrefix + "-raytraced.ppm");

	// Optional third argument = path-tracer samples per pixel (slower).
	if (argc > 3) {
		int samples = std::atoi(argv[3]);
		renderPathTraced(model, camera, canvas, samples);
		canvas.savePPM(outPrefix + "-pathtraced.ppm");
	}

	std::cout << "Rendered " << model.size() << " triangles to " << outPrefix << "-{wireframe,rasterised,raytraced}.ppm"
	          << std::endl;
	return 0;
}
