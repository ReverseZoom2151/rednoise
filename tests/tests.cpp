// Minimal dependency-free unit tests for the SDL-independent logic
// (interpolation, string splitting, OBJ/MTL parsing). Run via CTest.

#include "Interpolation.h"
#include "ObjLoader.h"
#include <Utils.h>
#include <cmath>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                                                                    \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::cerr << "FAIL: " #cond " (line " << __LINE__ << ")\n";                                                \
			++failures;                                                                                                \
		}                                                                                                              \
	} while (0)

static bool nearly(float a, float b) {
	return std::fabs(a - b) < 1e-4f;
}

static void testInterpolateSingleFloats() {
	std::vector<float> v = interpolateSingleFloats(2.0f, 8.0f, 4);
	CHECK(v.size() == 4);
	CHECK(nearly(v.front(), 2.0f));
	CHECK(nearly(v.back(), 8.0f));
	CHECK(nearly(v[1], 4.0f));
	CHECK(nearly(v[2], 6.0f));
	// Guards.
	CHECK(interpolateSingleFloats(1.0f, 2.0f, 0).empty());
	std::vector<float> one = interpolateSingleFloats(5.0f, 9.0f, 1);
	CHECK(one.size() == 1 && nearly(one[0], 5.0f));
}

static void testInterpolateThreeElementValues() {
	std::vector<glm::vec3> v = interpolateThreeElementValues(glm::vec3(1, 4, 9.2f), glm::vec3(4, 1, 9.8f), 4);
	CHECK(v.size() == 4);
	CHECK(nearly(v.front().x, 1.0f) && nearly(v.back().x, 4.0f));
	CHECK(nearly(v[1].x, 2.0f) && nearly(v[1].y, 3.0f));
}

static void testInterpolation() {
	CHECK(nearly(interpolation(5, 0, 10, 0, 100), 50));
	CHECK(nearly(interpolation(0, 0, 10, 20, 40), 20));
	// Degenerate source range returns y1.
	CHECK(nearly(interpolation(5, 3, 3, 7, 9), 7));
}

static void testSplit() {
	std::vector<std::string> t = split("a,b,c", ',');
	CHECK(t.size() == 3 && t[0] == "a" && t[1] == "b" && t[2] == "c");
}

static void testLoadOBJ() {
	// Path is relative to the repo root (CTest sets WORKING_DIRECTORY there).
	std::vector<ModelTriangle> tris = loadOBJ("assets/cornell-box.obj", 0.35f);
	CHECK(!tris.empty());
	// usemtl should have assigned named materials to the faces.
	bool anyNamed = false;
	for (const ModelTriangle &t : tris)
		if (!t.colour.name.empty())
			anyNamed = true;
	CHECK(anyNamed);
}

int main() {
	testInterpolateSingleFloats();
	testInterpolateThreeElementValues();
	testInterpolation();
	testSplit();
	testLoadOBJ();

	if (failures == 0) {
		std::cout << "All tests passed\n";
		return 0;
	}
	std::cerr << failures << " test(s) failed\n";
	return 1;
}
