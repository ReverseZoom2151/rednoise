#pragma once

#include "Camera.h"
#include "Light.h"
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

// The ray tracer shades each hit over all `lights` (point/directional/spot, with
// soft shadows for area lights), adding specular and an ambient floor; mirror
// and glass materials reflect / refract recursively. An empty `lights` uses a
// single default soft area light.
void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     ShadingModel shading = ShadingModel::Phong, const std::vector<Light> &lights = {});
