#pragma once

#include "BVH.h"
#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <glm/glm.hpp>
#include <vector>

// An analytic sphere primitive (exact, no tessellation).
struct Sphere {
	glm::vec3 center{};
	float radius = 1.0f;
	Colour colour{};
	Material material = Material::Diffuse;
	float roughness = 0.2f; // for Material::Metal
};

// A scene of triangle meshes (accelerated by a BVH) plus analytic spheres. Ray
// queries return the closest hit across both; a sphere hit is reported as a
// RayTriangleIntersection whose synthetic triangle carries the sphere's normal,
// colour and material (index SIZE_MAX so it never aliases a real triangle).
class Scene {
public:
	Scene(const std::vector<ModelTriangle> &triangles, const std::vector<Sphere> &spheres);

	RayTriangleIntersection intersect(const glm::vec3 &origin, const glm::vec3 &direction, int ignoreIndex = -1) const;
	bool occluded(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance, int ignoreIndex) const;

private:
	const std::vector<Sphere> &spheres_;
	BVH bvh_;
};
