// Generate a fractal terrain mesh (Perlin heightfield) as a Wavefront OBJ.
// Usage: gen_terrain [outPath]   (default assets/terrain.obj)

#include "Noise.h"
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <string>

int main(int argc, char **argv) {
	const int N = 60;             // grid resolution
	const float span = 6.0f;      // world extent in x and z
	const float amplitude = 1.4f; // height scale
	const float frequency = 0.6f; // noise frequency
	std::string out = (argc > 1) ? argv[1] : "assets/terrain.obj";

	std::ofstream f(out);
	f << "mtllib cornell-box.mtl\no terrain\nusemtl Green\n";

	auto height = [&](float x, float z) {
		return amplitude * (fractalNoise(glm::vec3(x * frequency, 0.0f, z * frequency), 5) - 0.5f);
	};
	for (int i = 0; i <= N; i++) {
		for (int j = 0; j <= N; j++) {
			float x = (i / static_cast<float>(N) - 0.5f) * span;
			float z = (j / static_cast<float>(N) - 0.5f) * span;
			f << "v " << x << " " << height(x, z) << " " << z << "\n";
		}
	}
	auto idx = [&](int i, int j) { return i * (N + 1) + j + 1; }; // 1-based
	for (int i = 0; i < N; i++) {
		for (int j = 0; j < N; j++) {
			int a = idx(i, j), b = idx(i + 1, j), c = idx(i + 1, j + 1), d = idx(i, j + 1);
			f << "f " << a << " " << b << " " << c << "\n";
			f << "f " << a << " " << c << " " << d << "\n";
		}
	}
	std::printf("Wrote %s (%d triangles)\n", out.c_str(), N * N * 2);
	return 0;
}
