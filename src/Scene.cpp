#include "Scene.h"

#include <cmath>
#include <limits>

Scene::Scene(const std::vector<ModelTriangle> &triangles, const std::vector<Sphere> &spheres)
    : spheres_(spheres), bvh_(triangles) {}

// Nearest positive root of the ray-sphere quadratic (direction assumed unit
// length). Returns a huge value if there is no hit in front.
static float sphereHit(const glm::vec3 &origin, const glm::vec3 &direction, const Sphere &s) {
	glm::vec3 oc = origin - s.center;
	float b = glm::dot(oc, direction);
	float c = glm::dot(oc, oc) - s.radius * s.radius;
	float disc = b * b - c;
	if (disc < 0.0f)
		return std::numeric_limits<float>::infinity();
	float sq = std::sqrt(disc);
	float t = -b - sq;
	if (t <= 1e-5f)
		t = -b + sq;
	return (t > 1e-5f) ? t : std::numeric_limits<float>::infinity();
}

RayTriangleIntersection Scene::intersect(const glm::vec3 &origin, const glm::vec3 &direction, int ignoreIndex) const {
	RayTriangleIntersection closest = bvh_.intersect(origin, direction, ignoreIndex);
	for (const Sphere &s : spheres_) {
		float t = sphereHit(origin, direction, s);
		if (t < closest.distanceFromCamera) {
			glm::vec3 point = origin + t * direction;
			glm::vec3 normal = glm::normalize(point - s.center);
			ModelTriangle tri;
			tri.colour = s.colour;
			tri.material = s.material;
			tri.normal = normal;
			tri.vertexNormals = {normal, normal, normal};
			closest.hit = true;
			closest.distanceFromCamera = t;
			closest.intersectionPoint = point;
			closest.intersectedTriangle = tri;
			closest.triangleIndex = std::numeric_limits<size_t>::max();
			closest.u = 0.0f;
			closest.v = 0.0f;
		}
	}
	return closest;
}

bool Scene::occluded(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance, int ignoreIndex) const {
	if (bvh_.occluded(origin, direction, maxDistance, ignoreIndex))
		return true;
	for (const Sphere &s : spheres_) {
		float t = sphereHit(origin, direction, s);
		if (t < maxDistance)
			return true;
	}
	return false;
}
