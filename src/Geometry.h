#pragma once

#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <glm/glm.hpp>
#include <vector>

// Geometric normal of a triangle (not normalised-dependent on winding order).
glm::vec3 triangleNormal(const ModelTriangle &triangle);

// Cast a ray (origin + t*direction, t > 0) against every triangle and return the
// closest valid hit. `intersection.hit` is false when nothing was hit. If
// `ignoreIndex` is non-negative, that triangle is skipped (useful for shadow
// rays that would otherwise self-intersect the originating surface).
RayTriangleIntersection getClosestIntersection(const glm::vec3 &origin, const glm::vec3 &direction,
                                               const std::vector<ModelTriangle> &triangles, int ignoreIndex = -1);
