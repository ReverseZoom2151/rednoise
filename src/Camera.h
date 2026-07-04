#pragma once

#include <CanvasPoint.h>
#include <glm/glm.hpp>

// A pinhole camera. Free of any SDL/window dependency so the projection maths
// can be unit-tested headlessly.
//
// Convention: the camera looks down its local -z axis. `orientation` is an
// orthonormal mat3 whose columns are the camera's right (x), up (y) and
// backward (z) axes in world space, so transpose(orientation) maps a
// world-space offset into camera space.
class Camera {
public:
	glm::vec3 position{0.0f, 0.0f, 4.0f};
	glm::mat3 orientation{1.0f};
	float focalLength = 2.0f;
	float scale = 150.0f; // world units -> pixels
	int imageWidth = 320;
	int imageHeight = 240;

	Camera() = default;
	Camera(int width, int height, float focal, const glm::vec3 &pos);

	// Transform a world-space vertex into camera space (camera at origin looking -z).
	glm::vec3 toCameraSpace(const glm::vec3 &vertex) const;

	// Project a world-space vertex onto the image plane. The returned CanvasPoint
	// carries a positive `depth` (distance in front of the camera); depth <= 0
	// means the vertex is level with or behind the camera.
	CanvasPoint projectVertex(const glm::vec3 &vertex) const;

	// Aim the camera at a target point, keeping a world-up of +y.
	void lookAt(const glm::vec3 &target);

	// Movement helpers.
	void translate(const glm::vec3 &delta);
	void rotateX(float radians); // pitch
	void rotateY(float radians); // yaw
	void orbitY(float radians, const glm::vec3 &target);
};
