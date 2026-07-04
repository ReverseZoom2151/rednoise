#pragma once

#include <glm/glm.hpp>
#include <string>
#include <array>
#include "Colour.h"
#include "TexturePoint.h"

// Surface response used by the ray tracer.
enum class Material { Diffuse, Mirror, Glass };

struct ModelTriangle {
	std::array<glm::vec3, 3> vertices{};
	std::array<TexturePoint, 3> texturePoints{};
	std::array<glm::vec3, 3> vertexNormals{};
	Colour colour{};
	glm::vec3 normal{};
	Material material = Material::Diffuse;

	ModelTriangle();
	ModelTriangle(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, Colour trigColour);
	friend std::ostream &operator<<(std::ostream &os, const ModelTriangle &triangle);
};
