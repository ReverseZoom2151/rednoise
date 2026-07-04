#include "Renderer.h"

#include "Drawing.h"
#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

// Project a triangle's three vertices. Returns false if any vertex is level with
// or behind the camera (no near-plane clipping yet - that whole triangle is
// simply dropped).
static bool projectTriangle(const ModelTriangle &tri, const Camera &camera, CanvasPoint out[3]) {
	for (int i = 0; i < 3; i++) {
		out[i] = camera.projectVertex(tri.vertices[i]);
		if (out[i].depth <= 0.0f)
			return false;
	}
	return true;
}

// Signed area of triangle (a, b, p) x2 - the edge function used for barycentric tests.
static float edgeFunction(const CanvasPoint &a, const CanvasPoint &b, float px, float py) {
	return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

void renderWireframe(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas) {
	canvas.clearPixels();
	for (const ModelTriangle &tri : model) {
		CanvasPoint p[3];
		if (!projectTriangle(tri, camera, p))
			continue;
		drawLine(p[0], p[1], tri.colour, canvas);
		drawLine(p[1], p[2], tri.colour, canvas);
		drawLine(p[2], p[0], tri.colour, canvas);
	}
}

void renderRasterised(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	// Inverse-depth buffer: 0 is infinitely far, larger is nearer.
	std::vector<float> depthBuffer(static_cast<size_t>(W) * H, 0.0f);

	for (const ModelTriangle &tri : model) {
		CanvasPoint p[3];
		if (!projectTriangle(tri, camera, p))
			continue;
		float area = edgeFunction(p[0], p[1], p[2].x, p[2].y);
		if (std::abs(area) < 1e-6f)
			continue;

		int minX = std::max(0, static_cast<int>(std::floor(std::min({p[0].x, p[1].x, p[2].x}))));
		int maxX = std::min(W - 1, static_cast<int>(std::ceil(std::max({p[0].x, p[1].x, p[2].x}))));
		int minY = std::max(0, static_cast<int>(std::floor(std::min({p[0].y, p[1].y, p[2].y}))));
		int maxY = std::min(H - 1, static_cast<int>(std::ceil(std::max({p[0].y, p[1].y, p[2].y}))));

		float inv0 = 1.0f / p[0].depth;
		float inv1 = 1.0f / p[1].depth;
		float inv2 = 1.0f / p[2].depth;
		uint32_t colour = tri.colour.toUint32();

		for (int y = minY; y <= maxY; y++) {
			for (int x = minX; x <= maxX; x++) {
				float px = x + 0.5f;
				float py = y + 0.5f;
				// Barycentric weights (divide by signed area so winding does not matter).
				float w0 = edgeFunction(p[1], p[2], px, py) / area;
				float w1 = edgeFunction(p[2], p[0], px, py) / area;
				float w2 = edgeFunction(p[0], p[1], px, py) / area;
				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
					continue;
				float invDepth = w0 * inv0 + w1 * inv1 + w2 * inv2;
				size_t idx = static_cast<size_t>(y) * W + x;
				if (invDepth > depthBuffer[idx]) {
					depthBuffer[idx] = invDepth;
					canvas.setPixelColour(x, y, colour);
				}
			}
		}
	}
}

// Shade a hit: proximity + angle-of-incidence diffuse, an ambient floor, and a
// hard shadow ray toward the light.
static uint32_t shadeHit(const RayTriangleIntersection &hit, const glm::vec3 &rayDirection, const glm::vec3 &light,
                         const std::vector<ModelTriangle> &model) {
	const float lightIntensity = 40.0f;
	const float ambient = 0.2f;

	glm::vec3 point = hit.intersectionPoint;
	glm::vec3 normal = triangleNormal(hit.intersectedTriangle);
	// Orient the normal toward the viewer (rayDirection points camera -> surface).
	if (glm::dot(normal, rayDirection) > 0.0f)
		normal = -normal;

	glm::vec3 toLight = light - point;
	float distance = glm::length(toLight);
	glm::vec3 lightDir = toLight / distance;

	// Hard shadow: something between the surface and the light occludes it.
	RayTriangleIntersection shadow =
	    getClosestIntersection(point, lightDir, model, static_cast<int>(hit.triangleIndex));
	bool inShadow = shadow.hit && shadow.distanceFromCamera < distance;

	float proximity = lightIntensity / (4.0f * 3.14159265f * distance * distance);
	float incidence = std::max(0.0f, glm::dot(normal, lightDir));
	float diffuse = inShadow ? 0.0f : proximity * incidence;

	// Specular highlight (Blinn/Phong): reflect the light about the normal and
	// compare with the view direction. Added as a white highlight.
	float specular = 0.0f;
	if (!inShadow && incidence > 0.0f) {
		glm::vec3 viewDir = -rayDirection;
		glm::vec3 reflectDir = 2.0f * glm::dot(normal, lightDir) * normal - lightDir;
		specular = std::pow(std::max(0.0f, glm::dot(reflectDir, viewDir)), 64.0f);
	}

	float brightness = std::min(1.0f, std::max(diffuse, ambient));
	const Colour &c = hit.intersectedTriangle.colour;
	int r = std::min(255, static_cast<int>(c.red * brightness + 255.0f * specular));
	int g = std::min(255, static_cast<int>(c.green * brightness + 255.0f * specular));
	int b = std::min(255, static_cast<int>(c.blue * brightness + 255.0f * specular));
	return (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     const glm::vec3 &light) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	float f = camera.focalLength * camera.scale;

	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			// Ray through this pixel: invert the projection to get a camera-space
			// direction, then rotate into world space.
			glm::vec3 dirCamera((x - W / 2.0f) / f, -(y - H / 2.0f) / f, -1.0f);
			glm::vec3 direction = glm::normalize(camera.orientation * dirCamera);
			RayTriangleIntersection hit = getClosestIntersection(camera.position, direction, model);
			if (hit.hit)
				canvas.setPixelColour(x, y, shadeHit(hit, direction, light, model));
		}
	}
}
