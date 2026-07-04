#pragma once

#include "Camera.h"
#include <Canvas.h>
#include <ModelTriangle.h>
#include <glm/glm.hpp>
#include <vector>

// Three ways to draw the same model. Each clears the canvas and draws into it.
void renderWireframe(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas);
void renderRasterised(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas);

// The ray tracer shades each hit with proximity + angle-of-incidence diffuse
// light, an ambient floor, and hard shadows cast toward `light`.
void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     const glm::vec3 &light = glm::vec3(0.0f, 0.95f, 0.0f));
