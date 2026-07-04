#include "Renderer.h"

#include "Drawing.h"
#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <utility>
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

// Diffuse (with ambient floor) + specular for one surface point and normal.
struct Shade {
	float brightness;
	float specular;
};

static Shade lightPoint(const glm::vec3 &point, const glm::vec3 &normal, const glm::vec3 &viewDir,
                        const glm::vec3 &light, bool inShadow) {
	const float lightIntensity = 40.0f;
	const float ambient = 0.2f;
	glm::vec3 toLight = light - point;
	float distance = glm::length(toLight);
	glm::vec3 lightDir = toLight / distance;
	float proximity = lightIntensity / (4.0f * 3.14159265f * distance * distance);
	float incidence = std::max(0.0f, glm::dot(normal, lightDir));
	float diffuse = inShadow ? 0.0f : proximity * incidence;
	float specular = 0.0f;
	if (!inShadow && incidence > 0.0f) {
		glm::vec3 reflectDir = 2.0f * glm::dot(normal, lightDir) * normal - lightDir;
		specular = std::pow(std::max(0.0f, glm::dot(reflectDir, viewDir)), 64.0f);
	}
	return {std::min(1.0f, std::max(diffuse, ambient)), specular};
}

// Flip a normal to face the viewer (rayDirection points camera -> surface).
static glm::vec3 faceViewer(glm::vec3 normal, const glm::vec3 &rayDirection) {
	return (glm::dot(normal, rayDirection) > 0.0f) ? -normal : normal;
}

static glm::vec3 shadeDiffuse(const RayTriangleIntersection &hit, const glm::vec3 &rayDirection, const glm::vec3 &light,
                              const std::vector<ModelTriangle> &model, ShadingModel shading) {
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;
	glm::vec3 viewDir = -rayDirection;

	// One shadow test at the hit point, shared by all shading models.
	glm::vec3 toLight = light - point;
	float lightDistance = glm::length(toLight);
	glm::vec3 lightDir = toLight / lightDistance;
	RayTriangleIntersection shadow =
	    getClosestIntersection(point, lightDir, model, static_cast<int>(hit.triangleIndex));
	bool inShadow = shadow.hit && shadow.distanceFromCamera < lightDistance;

	// Barycentric weights (vertex 0 gets 1-u-v).
	float w0 = 1.0f - hit.u - hit.v;
	float w1 = hit.u;
	float w2 = hit.v;

	Shade shade;
	if (shading == ShadingModel::Gouraud) {
		Shade s0 =
		    lightPoint(tri.vertices[0], faceViewer(tri.vertexNormals[0], rayDirection), viewDir, light, inShadow);
		Shade s1 =
		    lightPoint(tri.vertices[1], faceViewer(tri.vertexNormals[1], rayDirection), viewDir, light, inShadow);
		Shade s2 =
		    lightPoint(tri.vertices[2], faceViewer(tri.vertexNormals[2], rayDirection), viewDir, light, inShadow);
		shade.brightness = w0 * s0.brightness + w1 * s1.brightness + w2 * s2.brightness;
		shade.specular = w0 * s0.specular + w1 * s1.specular + w2 * s2.specular;
	} else {
		glm::vec3 normal =
		    (shading == ShadingModel::Phong)
		        ? glm::normalize(w0 * tri.vertexNormals[0] + w1 * tri.vertexNormals[1] + w2 * tri.vertexNormals[2])
		        : tri.normal;
		shade = lightPoint(point, faceViewer(normal, rayDirection), viewDir, light, inShadow);
	}

	const Colour &c = tri.colour;
	return glm::vec3(c.red, c.green, c.blue) * shade.brightness + glm::vec3(255.0f) * shade.specular;
}

// Recursively trace a ray. Diffuse surfaces are shaded directly; mirrors reflect
// and glass reflects + refracts (Fresnel-weighted), bounded by `depth`.
static glm::vec3 traceRay(const glm::vec3 &origin, const glm::vec3 &direction, const std::vector<ModelTriangle> &model,
                          const glm::vec3 &light, ShadingModel shading, int depth) {
	RayTriangleIntersection hit = getClosestIntersection(origin, direction, model);
	if (!hit.hit)
		return glm::vec3(0.0f); // background
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;

	if (depth > 0 && tri.material == Material::Mirror) {
		glm::vec3 n = faceViewer(tri.normal, direction);
		glm::vec3 reflected = glm::reflect(direction, n);
		return traceRay(point + 1e-4f * reflected, reflected, model, light, shading, depth - 1);
	}

	if (depth > 0 && tri.material == Material::Glass) {
		const float ior = 1.5f;
		glm::vec3 n = tri.normal;
		float cosi = glm::dot(direction, n);
		float etai = 1.0f, etat = ior;
		if (cosi < 0.0f) {
			cosi = -cosi; // ray entering the surface
		} else {
			std::swap(etai, etat); // ray exiting: flip the normal to oppose it
			n = -n;
		}
		float eta = etai / etat;
		glm::vec3 reflected = glm::reflect(direction, n);
		glm::vec3 reflectionColour = traceRay(point + 1e-4f * reflected, reflected, model, light, shading, depth - 1);
		glm::vec3 refracted = glm::refract(direction, n, eta);
		if (glm::length(refracted) < 1e-6f)
			return reflectionColour; // total internal reflection
		// Schlick's Fresnel approximation.
		float r0 = (etai - etat) / (etai + etat);
		r0 *= r0;
		float fresnel = r0 + (1.0f - r0) * std::pow(1.0f - cosi, 5.0f);
		glm::vec3 refractionColour = traceRay(point + 1e-4f * refracted, refracted, model, light, shading, depth - 1);
		return fresnel * reflectionColour + (1.0f - fresnel) * refractionColour;
	}

	return shadeDiffuse(hit, direction, light, model, shading);
}

void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     ShadingModel shading, const glm::vec3 &light) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	float f = camera.focalLength * camera.scale;
	const int maxDepth = 4;

	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			// Ray through this pixel: invert the projection to get a camera-space
			// direction, then rotate into world space.
			glm::vec3 dirCamera((x - W / 2.0f) / f, -(y - H / 2.0f) / f, -1.0f);
			glm::vec3 direction = glm::normalize(camera.orientation * dirCamera);
			glm::vec3 colour = traceRay(camera.position, direction, model, light, shading, maxDepth);
			int r = std::min(255, static_cast<int>(colour.r));
			int g = std::min(255, static_cast<int>(colour.g));
			int b = std::min(255, static_cast<int>(colour.b));
			canvas.setPixelColour(x, y, (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
		}
	}
}
