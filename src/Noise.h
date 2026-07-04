#pragma once

#include <glm/glm.hpp>

// Classic Perlin noise. `perlin` returns roughly [-1, 1]; `fractalNoise` sums
// octaves (fBm) and returns roughly [0, 1]. SDL-free and unit-testable.
float perlin(float x, float y, float z);
float fractalNoise(const glm::vec3 &p, int octaves);

// Analytical-derivative variants. These return the noise value together with its
// exact gradient (computed analytically from the quintic fade and the trilinear
// interpolation, not by finite differences).
//
// `perlinNoiseD` returns the Perlin value in .x and the gradient
// (d/dx, d/dy, d/dz) in .y, .z, .w. The value matches `perlin` at every point.
//
// `fractalNoiseD` returns the fBm value in .x (matching `fractalNoise`, i.e.
// mapped to roughly [0, 1]) and the analytical gradient of that value in
// .y, .z, .w, accumulating each octave's gradient scaled by its frequency
// (chain rule) and amplitude.
glm::vec4 perlinNoiseD(const glm::vec3 &p);
glm::vec4 fractalNoiseD(const glm::vec3 &p, int octaves);
