#pragma once

#include "Camera.h"
#include <DrawingWindow.h>
#include <ModelTriangle.h>
#include <vector>

// Three ways to draw the same model. Each clears the window and draws into it.
// NOTE: the pixel output of these is compile-verified but not yet visually
// confirmed (no SDL3 runtime available at authoring time) - see ROADMAP.md.
void renderWireframe(const std::vector<ModelTriangle> &model, const Camera &camera, DrawingWindow &window);
void renderRasterised(const std::vector<ModelTriangle> &model, const Camera &camera, DrawingWindow &window);
void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, DrawingWindow &window);
