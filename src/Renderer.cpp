#include "Renderer.h"

#include "Scene.h"
#include "Drawing.h"
#include "Geometry.h"
#include "Light.h"
#include "Noise.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <random>
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

// Fraction of a light visible from `point` (1 = lit, 0 = shadowed). Point and
// directional lights are binary; an area light (radius > 0) is sampled over a
// fixed disk for soft shadows (deterministic pattern, so no noise).
static float visibility(const Light &L, const glm::vec3 &point, const Scene &scene, int ignoreIndex) {
	if (L.type == LightType::Directional) {
		glm::vec3 dir = -glm::normalize(L.direction);
		return scene.occluded(point, dir, 1e6f, ignoreIndex) ? 0.0f : 1.0f;
	}
	if (L.radius <= 0.0f) {
		glm::vec3 to = L.position - point;
		float dist = glm::length(to);
		return scene.occluded(point, to / dist, dist, ignoreIndex) ? 0.0f : 1.0f;
	}
	static const glm::vec2 kSamples[9] = {{0, 0},       {1, 0},        {-1, 0},       {0, 1},        {0, -1},
	                                      {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f}};
	glm::vec3 axis = glm::normalize(point - L.position);
	glm::vec3 up = std::abs(axis.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	glm::vec3 tangent = glm::normalize(glm::cross(up, axis));
	glm::vec3 bitangent = glm::cross(axis, tangent);
	int unoccluded = 0;
	for (const glm::vec2 &s : kSamples) {
		glm::vec3 samplePos = L.position + (tangent * s.x + bitangent * s.y) * L.radius;
		glm::vec3 to = samplePos - point;
		float dist = glm::length(to);
		if (!scene.occluded(point, to / dist, dist, ignoreIndex))
			unoccluded++;
	}
	return unoccluded / 9.0f;
}

// Multi-light shading of a surface point: proximity/incidence diffuse + specular
// + soft-shadow visibility summed over every light, on an ambient floor. `base`
// is the surface's own colour (0..255).
static glm::vec3 shadeSurface(const glm::vec3 &point, const glm::vec3 &normal, const glm::vec3 &viewDir,
                              const std::vector<Light> &lights, const Scene &scene, int ignoreIndex,
                              const glm::vec3 &base) {
	const float ambient = 0.2f;
	glm::vec3 diffuseAccum(0.0f);
	float specAccum = 0.0f;
	for (const Light &L : lights) {
		glm::vec3 lightDir;
		float attenuation;
		if (L.type == LightType::Directional) {
			lightDir = -glm::normalize(L.direction);
			attenuation = L.intensity / 40.0f;
		} else {
			glm::vec3 to = L.position - point;
			float dist = glm::length(to);
			lightDir = to / dist;
			attenuation = L.intensity / (4.0f * 3.14159265f * dist * dist);
		}
		float cone = 1.0f;
		if (L.type == LightType::Spot)
			cone = (glm::dot(-lightDir, glm::normalize(L.direction)) > L.coneCos) ? 1.0f : 0.0f;
		float vis = visibility(L, point, scene, ignoreIndex);
		float incidence = std::max(0.0f, glm::dot(normal, lightDir));
		float brightness = vis * cone * attenuation * incidence;
		diffuseAccum += L.colour * brightness;
		if (brightness > 0.0f) {
			glm::vec3 reflectDir = 2.0f * glm::dot(normal, lightDir) * normal - lightDir;
			specAccum += vis * cone * std::pow(std::max(0.0f, glm::dot(reflectDir, viewDir)), 64.0f);
		}
	}
	glm::vec3 lit = glm::min(glm::vec3(1.0f), diffuseAccum + glm::vec3(ambient));
	return base * lit + glm::vec3(255.0f) * std::min(1.0f, specAccum);
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

// The surface's own colour (0..255) at a hit: sampled texture, procedural, or flat.
static glm::vec3 surfaceBaseColour(const RayTriangleIntersection &hit, const glm::vec3 &viewDir) {
	const ModelTriangle &tri = hit.intersectedTriangle;
	float w0 = 1.0f - hit.u - hit.v;
	float w1 = hit.u;
	float w2 = hit.v;
	if (tri.texture) {
		float tu = w0 * tri.texturePoints[0].x + w1 * tri.texturePoints[1].x + w2 * tri.texturePoints[2].x;
		float tv = w0 * tri.texturePoints[0].y + w1 * tri.texturePoints[1].y + w2 * tri.texturePoints[2].y;
		if (tri.material == Material::Parallax) {
			glm::vec2 uv = parallaxUV(tri, glm::vec2(tu, tv), viewDir);
			tu = uv.x;
			tv = uv.y;
		}
		return sampleTexture(*tri.texture, tu, tv);
	}
	if (tri.material == Material::Procedural)
		return proceduralColour(hit.intersectionPoint);
	return glm::vec3(tri.colour.red, tri.colour.green, tri.colour.blue);
}

static glm::vec3 shadeDiffuse(const RayTriangleIntersection &hit, const glm::vec3 &rayDirection,
                              const std::vector<Light> &lights, const Scene &scene, ShadingModel shading) {
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;
	glm::vec3 viewDir = -rayDirection;
	int ignore = static_cast<int>(hit.triangleIndex);

	// Barycentric weights (vertex 0 gets 1-u-v).
	float w0 = 1.0f - hit.u - hit.v;
	float w1 = hit.u;
	float w2 = hit.v;

	glm::vec3 base = surfaceBaseColour(hit, viewDir);

	if (shading == ShadingModel::Gouraud) {
		// Shade each vertex, then interpolate the resulting colour.
		glm::vec3 c0 = shadeSurface(tri.vertices[0], faceViewer(tri.vertexNormals[0], rayDirection), viewDir, lights,
		                            scene, ignore, base);
		glm::vec3 c1 = shadeSurface(tri.vertices[1], faceViewer(tri.vertexNormals[1], rayDirection), viewDir, lights,
		                            scene, ignore, base);
		glm::vec3 c2 = shadeSurface(tri.vertices[2], faceViewer(tri.vertexNormals[2], rayDirection), viewDir, lights,
		                            scene, ignore, base);
		return w0 * c0 + w1 * c1 + w2 * c2;
	}

	glm::vec3 normal =
	    (shading == ShadingModel::Phong)
	        ? glm::normalize(w0 * tri.vertexNormals[0] + w1 * tri.vertexNormals[1] + w2 * tri.vertexNormals[2])
	        : tri.normal;
	normal = faceViewer(normal, rayDirection);
	if (tri.material == Material::Bump)
		normal = bumpNormal(normal, point);
	return shadeSurface(point, normal, viewDir, lights, scene, ignore, base);
}

// Recursively trace a ray. Diffuse surfaces are shaded directly; mirrors reflect
// and glass reflects + refracts (Fresnel-weighted), bounded by `depth`.
static glm::vec3 traceRay(const glm::vec3 &origin, const glm::vec3 &direction, const Scene &scene,
                          const std::vector<Light> &lights, ShadingModel shading, int depth) {
	RayTriangleIntersection hit = scene.intersect(origin, direction);
	if (!hit.hit)
		return environment(direction); // sky
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;

	if (depth > 0 && tri.material == Material::Mirror) {
		glm::vec3 n = faceViewer(tri.normal, direction);
		glm::vec3 reflected = glm::reflect(direction, n);
		return traceRay(point + 1e-4f * reflected, reflected, scene, lights, shading, depth - 1);
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
		glm::vec3 reflectionColour = traceRay(point + 1e-4f * reflected, reflected, scene, lights, shading, depth - 1);
		glm::vec3 refracted = glm::refract(direction, n, eta);
		if (glm::length(refracted) < 1e-6f)
			return reflectionColour; // total internal reflection
		// Schlick's Fresnel approximation.
		float r0 = (etai - etat) / (etai + etat);
		r0 *= r0;
		float fresnel = r0 + (1.0f - r0) * std::pow(1.0f - cosi, 5.0f);
		glm::vec3 refractionColour = traceRay(point + 1e-4f * refracted, refracted, scene, lights, shading, depth - 1);
		return fresnel * reflectionColour + (1.0f - fresnel) * refractionColour;
	}

	return shadeDiffuse(hit, direction, lights, scene, shading);
}

// --- Monte-Carlo path tracer ------------------------------------------------

// Cosine-weighted hemisphere direction around a normal.
static glm::vec3 cosineHemisphere(const glm::vec3 &n, std::mt19937 &rng) {
	std::uniform_real_distribution<float> U(0.0f, 1.0f);
	float u1 = U(rng), u2 = U(rng);
	float r = std::sqrt(u1);
	float theta = 2.0f * 3.14159265f * u2;
	float x = r * std::cos(theta), y = r * std::sin(theta), z = std::sqrt(std::max(0.0f, 1.0f - u1));
	glm::vec3 up = std::abs(n.z) < 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
	glm::vec3 t = glm::normalize(glm::cross(up, n));
	glm::vec3 b = glm::cross(n, t);
	return glm::normalize(t * x + b * y + n * z);
}

// Direct lighting (0..1) at a diffuse point, summed over the lights.
static glm::vec3 directLight(const glm::vec3 &point, const glm::vec3 &normal, const glm::vec3 &albedo,
                             const std::vector<Light> &lights, const Scene &scene, int ignore) {
	glm::vec3 sum(0.0f);
	for (const Light &L : lights) {
		glm::vec3 lightDir;
		float attenuation;
		if (L.type == LightType::Directional) {
			lightDir = -glm::normalize(L.direction);
			attenuation = L.intensity / 40.0f;
		} else {
			glm::vec3 to = L.position - point;
			float dist = glm::length(to);
			lightDir = to / dist;
			attenuation = L.intensity / (4.0f * 3.14159265f * dist * dist);
		}
		float cone = 1.0f;
		if (L.type == LightType::Spot)
			cone = (glm::dot(-lightDir, glm::normalize(L.direction)) > L.coneCos) ? 1.0f : 0.0f;
		float vis = visibility(L, point, scene, ignore);
		float incidence = std::max(0.0f, glm::dot(normal, lightDir));
		sum += L.colour * (vis * cone * attenuation * incidence);
	}
	return albedo * glm::min(sum, glm::vec3(1.0f));
}

// Trace one path: direct light at each diffuse bounce plus a recursive
// cosine-weighted indirect bounce (this is what produces colour bleeding /
// global illumination). Mirror reflects; glass reflects or refracts
// stochastically by Fresnel.
static glm::vec3 pathTrace(const glm::vec3 &origin, const glm::vec3 &direction, const Scene &scene,
                           const std::vector<Light> &lights, int depth, std::mt19937 &rng) {
	RayTriangleIntersection hit = scene.intersect(origin, direction);
	if (!hit.hit)
		return environment(direction) / 255.0f;
	const ModelTriangle &tri = hit.intersectedTriangle;
	glm::vec3 point = hit.intersectionPoint;
	int ignore = static_cast<int>(hit.triangleIndex);

	if (tri.material == Material::Mirror && depth > 0) {
		glm::vec3 n = faceViewer(tri.normal, direction);
		glm::vec3 reflected = glm::reflect(direction, n);
		return pathTrace(point + 1e-4f * reflected, reflected, scene, lights, depth - 1, rng);
	}
	if (tri.material == Material::Glass && depth > 0) {
		const float ior = 1.5f;
		glm::vec3 n = tri.normal;
		float cosi = glm::dot(direction, n);
		float etai = 1.0f, etat = ior;
		if (cosi < 0.0f) {
			cosi = -cosi;
		} else {
			std::swap(etai, etat);
			n = -n;
		}
		float r0 = (etai - etat) / (etai + etat);
		r0 *= r0;
		float fresnel = r0 + (1.0f - r0) * std::pow(1.0f - cosi, 5.0f);
		glm::vec3 refracted = glm::refract(direction, n, etai / etat);
		std::uniform_real_distribution<float> U(0.0f, 1.0f);
		glm::vec3 dir = (glm::length(refracted) < 1e-6f || U(rng) < fresnel) ? glm::reflect(direction, n) : refracted;
		return pathTrace(point + 1e-4f * dir, dir, scene, lights, depth - 1, rng);
	}

	glm::vec3 n = faceViewer(tri.normal, direction);
	glm::vec3 albedo = surfaceBaseColour(hit, -direction) / 255.0f;
	glm::vec3 direct = directLight(point, n, albedo, lights, scene, ignore);
	if (depth <= 0)
		return direct;
	glm::vec3 bounce = cosineHemisphere(n, rng);
	glm::vec3 indirect = albedo * pathTrace(point + 1e-4f * bounce, bounce, scene, lights, depth - 1, rng);
	return direct + indirect;
}

void renderRaytraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas,
                     ShadingModel shading, const std::vector<Light> &lights, const std::vector<Sphere> &spheres) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	float f = camera.focalLength * camera.scale;
	const int maxDepth = 4;
	Scene scene(model, spheres); // triangles (BVH) + analytic spheres

	// Default to one soft (area) light so shadows have a penumbra out of the box.
	std::vector<Light> used = lights;
	if (used.empty()) {
		Light d;
		d.radius = 0.15f;
		used.push_back(d);
	}

	// Each pixel is independent, so the scanline loop parallelises cleanly.
#pragma omp parallel for schedule(dynamic, 4)
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			// Ray through this pixel: invert the projection to get a camera-space
			// direction, then rotate into world space.
			glm::vec3 dirCamera((x - W / 2.0f) / f, -(y - H / 2.0f) / f, -1.0f);
			glm::vec3 direction = glm::normalize(camera.orientation * dirCamera);
			glm::vec3 colour = traceRay(camera.position, direction, scene, used, shading, maxDepth);
			int r = std::min(255, static_cast<int>(colour.r));
			int g = std::min(255, static_cast<int>(colour.g));
			int b = std::min(255, static_cast<int>(colour.b));
			canvas.setPixelColour(x, y, (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
		}
	}
}

void renderPathTraced(const std::vector<ModelTriangle> &model, const Camera &camera, Canvas &canvas, int samples,
                      const std::vector<Light> &lights, float aperture, float focusDistance,
                      const glm::vec3 &cameraMotion, const std::vector<Sphere> &spheres) {
	canvas.clearPixels();
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	float f = camera.focalLength * camera.scale;
	const int maxDepth = 4;
	Scene scene(model, spheres);

	std::vector<Light> used = lights;
	if (used.empty()) {
		Light d;
		d.radius = 0.15f;
		used.push_back(d);
	}

#pragma omp parallel for schedule(dynamic, 4)
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			std::mt19937 rng(static_cast<unsigned>((y * W + x) * 9781 + 1)); // per-pixel seed = reproducible
			std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);
			std::uniform_real_distribution<float> U(0.0f, 1.0f);
			glm::vec3 sum(0.0f);
			for (int s = 0; s < samples; s++) {
				// Jitter within the pixel: supersampling anti-aliasing for free.
				float jx = jitter(rng), jy = jitter(rng);
				glm::vec3 dirCamera((x + 0.5f + jx - W / 2.0f) / f, -(y + 0.5f + jy - H / 2.0f) / f, -1.0f);
				glm::vec3 direction = glm::normalize(camera.orientation * dirCamera);
				// Motion blur: jitter the camera along its motion vector over the shutter.
				glm::vec3 camPos = camera.position + cameraMotion * U(rng);
				glm::vec3 origin = camPos;
				// Depth of field: sample a lens disk and re-aim at the focal point.
				if (aperture > 0.0f) {
					glm::vec3 focalPoint = camPos + direction * focusDistance;
					float ang = 2.0f * 3.14159265f * U(rng);
					float rad = aperture * std::sqrt(U(rng));
					glm::vec3 right = camera.orientation[0];
					glm::vec3 up = camera.orientation[1];
					origin = camPos + (right * std::cos(ang) + up * std::sin(ang)) * rad;
					direction = glm::normalize(focalPoint - origin);
				}
				sum += pathTrace(origin, direction, scene, used, maxDepth, rng);
			}
			glm::vec3 colour = sum / static_cast<float>(samples) * 255.0f;
			int r = std::min(255, static_cast<int>(colour.r));
			int g = std::min(255, static_cast<int>(colour.g));
			int b = std::min(255, static_cast<int>(colour.b));
			canvas.setPixelColour(x, y, (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
		}
	}
}

// Post-process the rendered canvas: Reinhard tone-map (compresses highlights)
// followed by gamma correction.
void toneMap(Canvas &canvas, float exposure, float gamma) {
	for (uint32_t &pixel : canvas.pixels) {
		glm::vec3 c(static_cast<float>((pixel >> 16) & 0xFF), static_cast<float>((pixel >> 8) & 0xFF),
		            static_cast<float>(pixel & 0xFF));
		c = c / 255.0f * exposure;
		c = c / (c + glm::vec3(1.0f));                     // Reinhard
		c = glm::pow(c, glm::vec3(1.0f / gamma)) * 255.0f; // gamma
		int r = std::min(255, static_cast<int>(c.r));
		int g = std::min(255, static_cast<int>(c.g));
		int b = std::min(255, static_cast<int>(c.b));
		pixel = (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
	}
}

// A lightweight FXAA: blur pixels along high-contrast luma edges to soften
// jaggies without a full extra render pass.
void applyFXAA(Canvas &canvas) {
	const float edgeThreshold = 0.12f;
	std::vector<uint32_t> src = canvas.pixels;
	int W = static_cast<int>(canvas.width);
	int H = static_cast<int>(canvas.height);
	auto at = [&](int x, int y) {
		uint32_t p = src[static_cast<size_t>(y) * W + x];
		return glm::vec3((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
	};
	auto luma = [](const glm::vec3 &c) { return (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f; };
	for (int y = 1; y < H - 1; y++) {
		for (int x = 1; x < W - 1; x++) {
			float lC = luma(at(x, y));
			float lN = luma(at(x, y - 1)), lS = luma(at(x, y + 1));
			float lW = luma(at(x - 1, y)), lE = luma(at(x + 1, y));
			float contrast = std::max({lC, lN, lS, lW, lE}) - std::min({lC, lN, lS, lW, lE});
			if (contrast < edgeThreshold)
				continue;
			glm::vec3 avg = (at(x, y - 1) + at(x, y + 1) + at(x - 1, y) + at(x + 1, y)) * 0.25f;
			float blend = std::min(0.5f, (contrast - edgeThreshold) * 2.0f);
			glm::vec3 c = glm::mix(at(x, y), avg, blend);
			int r = std::min(255, static_cast<int>(c.r));
			int g = std::min(255, static_cast<int>(c.g));
			int b = std::min(255, static_cast<int>(c.b));
			canvas.pixels[static_cast<size_t>(y) * W + x] =
			    (255u << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
		}
	}
}
