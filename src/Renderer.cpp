#include "Renderer.h"

#include "BVH.h"
#include "Drawing.h"
#include "Geometry.h"
#include "Noise.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <utility>
#include <vector>

// Signed area of triangle (a, b, p) x2 - the edge function used for barycentric tests.
static float edgeFunction(const CanvasPoint &a, const CanvasPoint &b, float px, float py) {
	return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

// Clip a triangle against the near plane in camera space, then project the
// result to canvas points. Returns 0, 1, or 2 sub-triangles (a triangle
// straddling the plane becomes a quad, fan-split into two). This replaces the
// old "drop the whole triangle if any vertex is behind the camera".
static std::vector<std::array<CanvasPoint, 3>> clipAndProject(const ModelTriangle &tri, const Camera &camera) {
	const float nearPlane = 0.1f;
	glm::vec3 cam[3];
	for (int i = 0; i < 3; i++)
		cam[i] = camera.toCameraSpace(tri.vertices[i]);
	auto inFront = [&](const glm::vec3 &v) { return -v.z > nearPlane; };

	std::vector<glm::vec3> poly; // Sutherland-Hodgman against the near plane
	for (int i = 0; i < 3; i++) {
		const glm::vec3 &a = cam[i];
		const glm::vec3 &b = cam[(i + 1) % 3];
		bool ai = inFront(a), bi = inFront(b);
		if (ai)
			poly.push_back(a);
		if (ai != bi) {
			float t = (-nearPlane - a.z) / (b.z - a.z);
			poly.push_back(a + t * (b - a));
		}
	}

	std::vector<std::array<CanvasPoint, 3>> result;
	if (poly.size() < 3)
		return result;
	std::vector<CanvasPoint> proj;
	proj.reserve(poly.size());
	for (const glm::vec3 &v : poly)
		proj.push_back(camera.projectCameraPoint(v));
	for (size_t i = 1; i + 1 < proj.size(); i++)
		result.push_back({proj[0], proj[i], proj[i + 1]});
	return result;
}

// Scanline fill of a projected triangle with an inverse-depth z-buffer.
static void fillTriangle(const CanvasPoint p[3], uint32_t colour, std::vector<float> &depthBuffer, int W, int H,
                         Canvas &canvas) {
	float area = edgeFunction(p[0], p[1], p[2].x, p[2].y);
	if (std::abs(area) < 1e-6f)
		return;
	int minX = std::max(0, static_cast<int>(std::floor(std::min({p[0].x, p[1].x, p[2].x}))));
	int maxX = std::min(W - 1, static_cast<int>(std::ceil(std::max({p[0].x, p[1].x, p[2].x}))));
	int minY = std::max(0, static_cast<int>(std::floor(std::min({p[0].y, p[1].y, p[2].y}))));
	int maxY = std::min(H - 1, static_cast<int>(std::ceil(std::max({p[0].y, p[1].y, p[2].y}))));
	float inv0 = 1.0f / p[0].depth;
	float inv1 = 1.0f / p[1].depth;
	float inv2 = 1.0f / p[2].depth;
	for (int y = minY; y <= maxY; y++) {
		for (int x = minX; x <= maxX; x++) {
			float px = x + 0.5f;
			float py = y + 0.5f;
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

void renderWireframe(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas) {
	canvas.clearPixels();
	for (const ModelTriangle &tri : model) {
		for (const std::array<CanvasPoint, 3> &p : clipAndProject(tri, camera)) {
			drawLine(p[0], p[1], tri.colour, canvas);
			drawLine(p[1], p[2], tri.colour, canvas);
			drawLine(p[2], p[0], tri.colour, canvas);
		}
	}
}

void renderRasterised(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                      bool backfaceCull) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	std::vector<float> depthBuffer(static_cast<size_t>(W) * H, 0.0f); // inverse depth, 0 = far

	for (const ModelTriangle &tri : model) {
		for (const std::array<CanvasPoint, 3> &p : clipAndProject(tri, camera)) {
			// Backface culling: a back-facing triangle projects with negative
			// screen-space winding. Optional (needs consistent winding).
			if (backfaceCull && edgeFunction(p[0], p[1], p[2].x, p[2].y) < 0.0f)
				continue;
			fillTriangle(p.data(), tri.colour.toUint32(), depthBuffer, W, H, canvas);
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

// A simple procedural sky: rays that escape the scene sample this instead of black.
static glm::vec3 environment(const glm::vec3 &direction) {
	float t = 0.5f * (direction.y + 1.0f);
	return glm::mix(glm::vec3(200.0f, 210.0f, 235.0f), glm::vec3(70.0f, 120.0f, 205.0f), t);
}

// Procedural wood grain from fBm-perturbed rings (Material::Procedural).
static glm::vec3 proceduralColour(const glm::vec3 &p) {
	glm::vec3 q = p * 3.0f;
	float n = fractalNoise(q, 4);
	float rings = 0.5f + 0.5f * std::sin((q.x + q.y) * 3.0f + n * 8.0f);
	return glm::mix(glm::vec3(60.0f, 30.0f, 12.0f), glm::vec3(205.0f, 150.0f, 90.0f), rings);
}

// Perturb a normal by the gradient of a noise field (Material::Bump).
static glm::vec3 bumpNormal(const glm::vec3 &normal, const glm::vec3 &p) {
	glm::vec3 q = p * 6.0f;
	float e = 0.15f;
	glm::vec3 gradient(perlin(q.x + e, q.y, q.z) - perlin(q.x - e, q.y, q.z),
	                   perlin(q.x, q.y + e, q.z) - perlin(q.x, q.y - e, q.z),
	                   perlin(q.x, q.y, q.z + e) - perlin(q.x, q.y, q.z - e));
	return glm::normalize(normal + 2.0f * gradient);
}

// Nearest-neighbour texel lookup at normalised (u, v); v is flipped for image origin.
static glm::vec3 sampleTexture(const TextureMap &tex, float u, float v) {
	int tw = static_cast<int>(tex.width);
	int th = static_cast<int>(tex.height);
	int tx = std::min(std::max(static_cast<int>(u * (tw - 1)), 0), tw - 1);
	int ty = std::min(std::max(static_cast<int>((1.0f - v) * (th - 1)), 0), th - 1);
	uint32_t p = tex.pixels[static_cast<size_t>(ty) * tw + tx];
	return glm::vec3((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
}

// Offset a texture coordinate along the view direction (in the triangle's tangent
// frame) by a height read from the texture, giving simple parallax mapping.
static glm::vec2 parallaxUV(const ModelTriangle &tri, glm::vec2 uv, const glm::vec3 &viewDir) {
	glm::vec3 e1 = tri.vertices[1] - tri.vertices[0];
	glm::vec3 e2 = tri.vertices[2] - tri.vertices[0];
	glm::vec2 d1(tri.texturePoints[1].x - tri.texturePoints[0].x, tri.texturePoints[1].y - tri.texturePoints[0].y);
	glm::vec2 d2(tri.texturePoints[2].x - tri.texturePoints[0].x, tri.texturePoints[2].y - tri.texturePoints[0].y);
	float det = d1.x * d2.y - d2.x * d1.y;
	if (std::abs(det) < 1e-8f)
		return uv;
	float f = 1.0f / det;
	glm::vec3 T = glm::normalize(f * (d2.y * e1 - d1.y * e2));
	glm::vec3 B = glm::normalize(f * (-d2.x * e1 + d1.x * e2));
	glm::vec3 vTS(glm::dot(viewDir, T), glm::dot(viewDir, B), glm::dot(viewDir, tri.normal));
	if (std::abs(vTS.z) < 1e-4f)
		return uv;
	float height = sampleTexture(*tri.texture, uv.x, uv.y).x / 255.0f; // red channel as height
	glm::vec2 offset = glm::vec2(vTS.x, vTS.y) / vTS.z * (height * 0.05f);
	return uv - offset;
}

static glm::vec3 shadeDiffuse(const RayTriangleIntersection &hit, const glm::vec3 &rayDirection, const glm::vec3 &light,
                              const BVH &bvh, ShadingModel shading) {
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;
	glm::vec3 viewDir = -rayDirection;

	// One shadow test at the hit point, shared by all shading models.
	glm::vec3 toLight = light - point;
	float lightDistance = glm::length(toLight);
	glm::vec3 lightDir = toLight / lightDistance;
	bool inShadow = bvh.occluded(point, lightDir, lightDistance, static_cast<int>(hit.triangleIndex));

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
		normal = faceViewer(normal, rayDirection);
		if (tri.material == Material::Bump)
			normal = bumpNormal(normal, point);
		shade = lightPoint(point, normal, viewDir, light, inShadow);
	}

	glm::vec3 base;
	if (tri.texture) {
		float tu = w0 * tri.texturePoints[0].x + w1 * tri.texturePoints[1].x + w2 * tri.texturePoints[2].x;
		float tv = w0 * tri.texturePoints[0].y + w1 * tri.texturePoints[1].y + w2 * tri.texturePoints[2].y;
		if (tri.material == Material::Parallax) {
			glm::vec2 uv = parallaxUV(tri, glm::vec2(tu, tv), viewDir);
			tu = uv.x;
			tv = uv.y;
		}
		base = sampleTexture(*tri.texture, tu, tv);
	} else if (tri.material == Material::Procedural) {
		base = proceduralColour(point);
	} else {
		base = glm::vec3(tri.colour.red, tri.colour.green, tri.colour.blue);
	}
	return base * shade.brightness + glm::vec3(255.0f) * shade.specular;
}

// Recursively trace a ray. Diffuse surfaces are shaded directly; mirrors reflect
// and glass reflects + refracts (Fresnel-weighted), bounded by `depth`.
static glm::vec3 traceRay(const glm::vec3 &origin, const glm::vec3 &direction, const BVH &bvh, const glm::vec3 &light,
                          ShadingModel shading, int depth) {
	RayTriangleIntersection hit = bvh.intersect(origin, direction);
	if (!hit.hit)
		return environment(direction); // sky
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;

	if (depth > 0 && tri.material == Material::Mirror) {
		glm::vec3 n = faceViewer(tri.normal, direction);
		glm::vec3 reflected = glm::reflect(direction, n);
		return traceRay(point + 1e-4f * reflected, reflected, bvh, light, shading, depth - 1);
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
		glm::vec3 reflectionColour = traceRay(point + 1e-4f * reflected, reflected, bvh, light, shading, depth - 1);
		glm::vec3 refracted = glm::refract(direction, n, eta);
		if (glm::length(refracted) < 1e-6f)
			return reflectionColour; // total internal reflection
		// Schlick's Fresnel approximation.
		float r0 = (etai - etat) / (etai + etat);
		r0 *= r0;
		float fresnel = r0 + (1.0f - r0) * std::pow(1.0f - cosi, 5.0f);
		glm::vec3 refractionColour = traceRay(point + 1e-4f * refracted, refracted, bvh, light, shading, depth - 1);
		return fresnel * reflectionColour + (1.0f - fresnel) * refractionColour;
	}

	return shadeDiffuse(hit, direction, light, bvh, shading);
}

void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     ShadingModel shading, const glm::vec3 &light) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	float f = camera.focalLength * camera.scale;
	const int maxDepth = 4;
	BVH bvh(model); // build the acceleration structure once per frame

	// Each pixel is independent, so the scanline loop parallelises cleanly.
#pragma omp parallel for schedule(dynamic, 4)
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			// Ray through this pixel: invert the projection to get a camera-space
			// direction, then rotate into world space.
			glm::vec3 dirCamera((x - W / 2.0f) / f, -(y - H / 2.0f) / f, -1.0f);
			glm::vec3 direction = glm::normalize(camera.orientation * dirCamera);
			glm::vec3 colour = traceRay(camera.position, direction, bvh, light, shading, maxDepth);
			int r = std::min(255, static_cast<int>(colour.r));
			int g = std::min(255, static_cast<int>(colour.g));
			int b = std::min(255, static_cast<int>(colour.b));
			canvas.setPixelColour(x, y, (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
		}
	}
}
