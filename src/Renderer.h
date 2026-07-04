#pragma once

#include "Camera.h"
#include "Light.h"
#include "Scene.h"
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
                     ShadingModel shading = ShadingModel::Phong, const std::vector<Light> &lights = {},
                     const std::vector<Sphere> &spheres = {});

// Monte-Carlo path tracer: `samples` jittered paths per pixel give global
// illumination (colour bleeding), soft shadows, and anti-aliasing together.
// aperture > 0 enables depth of field focused at `focusDistance`; a non-zero
// cameraMotion adds motion blur along that vector.
void renderPathTraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas, int samples = 32,
                      const std::vector<Light> &lights = {}, float aperture = 0.0f, float focusDistance = 4.0f,
                      const glm::vec3 &cameraMotion = glm::vec3(0.0f), const std::vector<Sphere> &spheres = {});

// Post-filters over a rendered canvas.
void toneMap(Canvas &canvas, float exposure = 1.0f, float gamma = 2.2f);
void applyFXAA(Canvas &canvas);
