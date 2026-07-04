#pragma once

#include <glm/glm.hpp>

// Classic Perlin noise. `perlin` returns roughly [-1, 1]; `fractalNoise` sums
// octaves (fBm) and returns roughly [0, 1]. SDL-free and unit-testable.
float perlin(float x, float y, float z);
float fractalNoise(const glm::vec3 &p, int octaves);
