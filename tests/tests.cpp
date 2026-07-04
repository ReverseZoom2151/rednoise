// Minimal dependency-free unit tests for the SDL-independent logic
// (interpolation, string splitting, OBJ/MTL parsing). Run via CTest.

#include "Camera.h"
#include "Geometry.h"
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

static void testCameraProjection() {
	Camera cam(320, 240, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
	cam.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	// The origin projects to the centre of the image, four units in front.
	CanvasPoint centre = cam.projectVertex(glm::vec3(0.0f, 0.0f, 0.0f));
	CHECK(nearly(centre.x, 160.0f));
	CHECK(nearly(centre.y, 120.0f));
	CHECK(nearly(centre.depth, 4.0f));
	// A point to the right maps right of centre; a point up maps above centre
	// (smaller y, since screen y grows downward).
	CanvasPoint right = cam.projectVertex(glm::vec3(1.0f, 0.0f, 0.0f));
	CanvasPoint up = cam.projectVertex(glm::vec3(0.0f, 1.0f, 0.0f));
	CHECK(right.x > 160.0f);
	CHECK(up.y < 120.0f);
	// A more distant point has greater depth.
	CanvasPoint far = cam.projectVertex(glm::vec3(0.0f, 0.0f, -4.0f));
	CHECK(far.depth > centre.depth);
}

static void testCameraLookAt() {
	Camera cam(320, 240, 2.0f, glm::vec3(3.0f, 2.0f, 5.0f));
	cam.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	glm::vec3 rightAxis = cam.orientation[0];
	glm::vec3 upAxis = cam.orientation[1];
	glm::vec3 fwdAxis = cam.orientation[2];
	// Orthonormal basis: unit columns, mutually perpendicular.
	CHECK(nearly(glm::length(rightAxis), 1.0f));
	CHECK(nearly(glm::length(upAxis), 1.0f));
	CHECK(nearly(glm::length(fwdAxis), 1.0f));
	CHECK(nearly(glm::dot(rightAxis, upAxis), 0.0f));
	CHECK(nearly(glm::dot(rightAxis, fwdAxis), 0.0f));
	CHECK(nearly(glm::dot(upAxis, fwdAxis), 0.0f));
	// Forward column points from the target back to the camera.
	glm::vec3 expectedFwd = glm::normalize(cam.position - glm::vec3(0.0f));
	CHECK(nearly(fwdAxis.x, expectedFwd.x) && nearly(fwdAxis.y, expectedFwd.y) && nearly(fwdAxis.z, expectedFwd.z));
}

static void testTriangleIntersection() {
	std::vector<ModelTriangle> tris;
	tris.push_back(ModelTriangle(glm::vec3(-1, -1, -2), glm::vec3(1, -1, -2), glm::vec3(0, 1, -2), Colour(255, 0, 0)));
	// Ray straight down -z from the origin hits the triangle at (0,0,-2), distance 2.
	RayTriangleIntersection hit = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), tris);
	CHECK(hit.hit);
	CHECK(nearly(hit.distanceFromCamera, 2.0f));
	CHECK(nearly(hit.intersectionPoint.z, -2.0f));
	// Ray pointing the other way misses.
	RayTriangleIntersection miss = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), tris);
	CHECK(!miss.hit);
	// Closest-hit: a nearer triangle in front should win.
	tris.push_back(ModelTriangle(glm::vec3(-1, -1, -1), glm::vec3(1, -1, -1), glm::vec3(0, 1, -1), Colour(0, 255, 0)));
	RayTriangleIntersection nearest = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), tris);
	CHECK(nearest.hit && nearly(nearest.distanceFromCamera, 1.0f));
}

static void testTriangleNormal() {
	ModelTriangle t(glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(0, 1, 0), Colour());
	glm::vec3 n = triangleNormal(t);
	CHECK(nearly(glm::length(n), 1.0f));
	CHECK(nearly(std::fabs(n.z), 1.0f)); // triangle lies in the z=0 plane
}

int main() {
	testInterpolateSingleFloats();
	testInterpolateThreeElementValues();
	testInterpolation();
	testSplit();
	testLoadOBJ();
	testCameraProjection();
	testCameraLookAt();
	testTriangleIntersection();
	testTriangleNormal();

	if (failures == 0) {
		std::cout << "All tests passed\n";
		return 0;
	}
	std::cerr << failures << " test(s) failed\n";
	return 1;
}
