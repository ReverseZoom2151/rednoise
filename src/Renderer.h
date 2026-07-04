#pragma once

#include "Camera.h"
#include <Canvas.h>
#include <ModelTriangle.h>
#include <glm/glm.hpp>
#include <vector>

// Flat uses the face normal; Gouraud lights each vertex and interpolates the
// result; Phong interpolates the normal per pixel.
enum class ShadingModel { Flat, Gouraud, Phong };

// Three ways to draw the same model. Each clears the canvas and draws into it.
// Both raster paths clip triangles against the near plane; the rasteriser can
// optionally cull back-facing triangles.
void renderWireframe(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas);
void renderRasterised(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                      bool backfaceCull = false);

// The ray tracer shades each hit with proximity + angle-of-incidence diffuse
// light, a specular highlight, an ambient floor, and hard shadows toward
// `light`. Mirror and glass materials reflect / refract recursively.
void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     ShadingModel shading = ShadingModel::Phong, const glm::vec3 &light = glm::vec3(0.0f, 0.95f, 0.0f));
