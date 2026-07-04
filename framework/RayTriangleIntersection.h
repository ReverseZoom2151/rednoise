#pragma once

#include "ModelTriangle.h"
#include <glm/glm.hpp>
#include <limits>

// Result of casting a ray against the scene: the closest triangle it hit (if any),
// where it hit, and how far along the ray that was.
struct RayTriangleIntersection {
	glm::vec3 intersectionPoint{};
	float distanceFromCamera = std::numeric_limits<float>::infinity();
	ModelTriangle intersectedTriangle{};
	size_t triangleIndex = 0;
	bool hit = false;
};
