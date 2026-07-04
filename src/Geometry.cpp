#include "Geometry.h"

glm::vec3 triangleNormal(const ModelTriangle &triangle) {
	glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
	glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
	return glm::normalize(glm::cross(e0, e1));
}

RayTriangleIntersection getClosestIntersection(const glm::vec3 &origin, const glm::vec3 &direction,
                                               const std::vector<ModelTriangle> &triangles, int ignoreIndex) {
	RayTriangleIntersection closest; // hit = false, distance = +inf
	for (size_t i = 0; i < triangles.size(); i++) {
		if (static_cast<int>(i) == ignoreIndex)
			continue;
		const ModelTriangle &triangle = triangles[i];
		glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
		glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
		glm::vec3 spVector = origin - triangle.vertices[0];
		glm::mat3 deMatrix(-direction, e0, e1);
		float det = glm::determinant(deMatrix);
		if (std::abs(det) < 1e-8f)
			continue; // ray parallel to the triangle plane
		glm::vec3 possibleSolution = glm::inverse(deMatrix) * spVector;
		float t = possibleSolution.x; // distance along the ray
		float u = possibleSolution.y; // barycentric
		float v = possibleSolution.z; // barycentric
		bool inside = (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
		if (inside && t > 1e-5f && t < closest.distanceFromCamera) {
			closest.hit = true;
			closest.distanceFromCamera = t;
			closest.intersectionPoint = origin + t * direction;
			closest.intersectedTriangle = triangle;
			closest.triangleIndex = i;
		}
	}
	return closest;
}
