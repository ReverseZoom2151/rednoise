// Render the Gerstner-wave ocean to a PPM frame sequence: each frame advances
// the wave time, so the swell animates. The water is glass (Fresnel sky
// reflection + refraction) over a dark seabed, lit by a low sun.
//
//   render_ocean ocean 24        # 24 animated frames ocean-000.ppm ...
//   render_ocean ocean 1 90      # a single high-res frame (grid 90)

#include "Camera.h"
#include "Ocean.h"
#include "Renderer.h"
#include <Canvas.h>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>

int main(int argc, char **argv) {
	const char *prefix = argc > 1 ? argv[1] : "ocean";
	int frames = argc > 2 ? std::atoi(argv[2]) : 1;
	int gridN = argc > 3 ? std::atoi(argv[3]) : 60;

	Primitives prims;
	prims.planes.push_back({glm::vec3(0, -0.7f, 0), glm::vec3(0, 1, 0), Colour(20, 45, 70), Material::Diffuse, 0.2f});
	Camera camera(320, 200, 2.0f, glm::vec3(0, 0.5f, 3.3f));
	camera.lookAt(glm::vec3(0, -0.1f, -2.0f));
	std::vector<Light> sun;
	Light l;
	l.type = LightType::Directional;
	l.direction = glm::vec3(-0.4f, -0.7f, -0.6f);
	l.intensity = 48.0f;
	sun.push_back(l);

	for (int f = 0; f < frames; f++) {
		float t = f * 0.4f;
		std::vector<ModelTriangle> ocean = generateOcean(gridN, 7.0f, t);
		Canvas canvas(320, 200);
		renderRaytraced(ocean, camera, canvas, ShadingModel::Phong, sun, prims);
		char name[256];
		std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix, f);
		canvas.savePPM(name);
		std::printf("wrote %s (%zu triangles)\n", name, ocean.size());
	}
	return 0;
}
