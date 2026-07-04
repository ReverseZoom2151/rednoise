#pragma once

#include <glm/glm.hpp>

enum class LightType { Point, Directional, Spot };

// A light source. A point light with radius > 0 is an area light (soft shadows).
struct Light {
	LightType type = LightType::Point;
	glm::vec3 position{0.0f, 0.95f, 0.0f};
	glm::vec3 direction{0.0f, -1.0f, 0.0f}; // for Directional / Spot
	glm::vec3 colour{1.0f, 1.0f, 1.0f};     // 0..1 tint
	float intensity = 40.0f;
	float radius = 0.0f;  // area-light size; 0 = a hard point light
	float coneCos = 0.9f; // Spot cone, cosine of the half-angle
};
