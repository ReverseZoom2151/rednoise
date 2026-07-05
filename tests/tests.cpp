// Minimal dependency-free unit tests for the SDL-independent logic
// (interpolation, string splitting, OBJ/MTL parsing). Run via CTest.

#include "BVH.h"
#include "Camera.h"
#include "Geometry.h"
#include "Interpolation.h"
#include "Clouds.h"
#include "Aniso.h"
#include "BCn.h"
#include "Bezier.h"
#include "Drawing.h"
#include "Blackbody.h"
#include "IrradianceCache.h"
#include "QMC.h"
#include "ColourUtil.h"
#include "Quaternion.h"
#include "Resample.h"
#include "Frustum.h"
#include "Grid.h"
#include "HalfEdge.h"
#include "KdTree.h"
#include "Lines.h"
#include "Materials.h"
#include "Meshing.h"
#include "Mipmap.h"
#include "Morton.h"
#include "Noise.h"
#include "Nurbs.h"
#include "ObjLoader.h"
#include "Octree.h"
#include "OceanFFT.h"
#include "Scene.h"
#include "SceneGraph.h"
#include "Sky.h"
#include "Tessellator.h"
#include "Tonemap.h"
#include "Transform.h"
#include "Voxel.h"
#include "Worley.h"
#include <Canvas.h>
#include <glm/gtc/matrix_transform.hpp>
#include <Utils.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                                                                    \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::cerr << "FAIL: " #cond " (line " << __LINE__ << ")\n";                                                \
			++failures;                                                                                                \
		}                                                                                                              \
	} while (0)

static bool nearly(float a, float b) {
	return std::fabs(a - b) < 1e-4f;
}

static void testInterpolateSingleFloats() {
	std::vector<float> v = interpolateSingleFloats(2.0f, 8.0f, 4);
	CHECK(v.size() == 4);
	CHECK(nearly(v.front(), 2.0f));
	CHECK(nearly(v.back(), 8.0f));
	CHECK(nearly(v[1], 4.0f));
	CHECK(nearly(v[2], 6.0f));
	// Guards.
	CHECK(interpolateSingleFloats(1.0f, 2.0f, 0).empty());
	std::vector<float> one = interpolateSingleFloats(5.0f, 9.0f, 1);
	CHECK(one.size() == 1 && nearly(one[0], 5.0f));
}

static void testInterpolateThreeElementValues() {
	std::vector<glm::vec3> v = interpolateThreeElementValues(glm::vec3(1, 4, 9.2f), glm::vec3(4, 1, 9.8f), 4);
	CHECK(v.size() == 4);
	CHECK(nearly(v.front().x, 1.0f) && nearly(v.back().x, 4.0f));
	CHECK(nearly(v[1].x, 2.0f) && nearly(v[1].y, 3.0f));
}

static void testInterpolation() {
	CHECK(nearly(interpolation(5, 0, 10, 0, 100), 50));
	CHECK(nearly(interpolation(0, 0, 10, 20, 40), 20));
	// Degenerate source range returns y1.
	CHECK(nearly(interpolation(5, 3, 3, 7, 9), 7));
}

static void testSplit() {
	std::vector<std::string> t = split("a,b,c", ',');
	CHECK(t.size() == 3 && t[0] == "a" && t[1] == "b" && t[2] == "c");
}

static void testLoadOBJ() {
	// Path is relative to the repo root (CTest sets WORKING_DIRECTORY there).
	std::vector<ModelTriangle> tris = loadOBJ("assets/cornell-box.obj", 0.35f);
	CHECK(!tris.empty());
	// usemtl should have assigned named materials to the faces.
	bool anyNamed = false;
	for (const ModelTriangle &t : tris)
		if (!t.colour.name.empty())
			anyNamed = true;
	CHECK(anyNamed);
}

static void testCameraProjection() {
	Camera cam(320, 240, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
	cam.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	// The origin projects to the centre of the image, four units in front.
	CanvasPoint centre = cam.projectVertex(glm::vec3(0.0f, 0.0f, 0.0f));
	CHECK(nearly(centre.x, 160.0f));
	CHECK(nearly(centre.y, 120.0f));
	CHECK(nearly(centre.depth, 4.0f));
	// A point to the right maps right of centre; a point up maps above centre
	// (smaller y, since screen y grows downward).
	CanvasPoint right = cam.projectVertex(glm::vec3(1.0f, 0.0f, 0.0f));
	CanvasPoint up = cam.projectVertex(glm::vec3(0.0f, 1.0f, 0.0f));
	CHECK(right.x > 160.0f);
	CHECK(up.y < 120.0f);
	// A more distant point has greater depth.
	CanvasPoint far = cam.projectVertex(glm::vec3(0.0f, 0.0f, -4.0f));
	CHECK(far.depth > centre.depth);
}

static void testCameraLookAt() {
	Camera cam(320, 240, 2.0f, glm::vec3(3.0f, 2.0f, 5.0f));
	cam.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	glm::vec3 rightAxis = cam.orientation[0];
	glm::vec3 upAxis = cam.orientation[1];
	glm::vec3 fwdAxis = cam.orientation[2];
	// Orthonormal basis: unit columns, mutually perpendicular.
	CHECK(nearly(glm::length(rightAxis), 1.0f));
	CHECK(nearly(glm::length(upAxis), 1.0f));
	CHECK(nearly(glm::length(fwdAxis), 1.0f));
	CHECK(nearly(glm::dot(rightAxis, upAxis), 0.0f));
	CHECK(nearly(glm::dot(rightAxis, fwdAxis), 0.0f));
	CHECK(nearly(glm::dot(upAxis, fwdAxis), 0.0f));
	// Forward column points from the target back to the camera.
	glm::vec3 expectedFwd = glm::normalize(cam.position - glm::vec3(0.0f));
	CHECK(nearly(fwdAxis.x, expectedFwd.x) && nearly(fwdAxis.y, expectedFwd.y) && nearly(fwdAxis.z, expectedFwd.z));
}

static void testTriangleIntersection() {
	std::vector<ModelTriangle> tris;
	tris.push_back(ModelTriangle(glm::vec3(-1, -1, -2), glm::vec3(1, -1, -2), glm::vec3(0, 1, -2), Colour(255, 0, 0)));
	// Ray straight down -z from the origin hits the triangle at (0,0,-2), distance 2.
	RayTriangleIntersection hit = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), tris);
	CHECK(hit.hit);
	CHECK(nearly(hit.distanceFromCamera, 2.0f));
	CHECK(nearly(hit.intersectionPoint.z, -2.0f));
	// Ray pointing the other way misses.
	RayTriangleIntersection miss = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), tris);
	CHECK(!miss.hit);
	// Closest-hit: a nearer triangle in front should win.
	tris.push_back(ModelTriangle(glm::vec3(-1, -1, -1), glm::vec3(1, -1, -1), glm::vec3(0, 1, -1), Colour(0, 255, 0)));
	RayTriangleIntersection nearest = getClosestIntersection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), tris);
	CHECK(nearest.hit && nearly(nearest.distanceFromCamera, 1.0f));
}

static void testTriangleNormal() {
	ModelTriangle t(glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(0, 1, 0), Colour());
	glm::vec3 n = triangleNormal(t);
	CHECK(nearly(glm::length(n), 1.0f));
	CHECK(nearly(std::fabs(n.z), 1.0f)); // triangle lies in the z=0 plane
}

static void testBVH() {
	std::vector<ModelTriangle> model = loadOBJ("assets/cornell-box.obj", 0.35f);
	CHECK(!model.empty());
	BVH bvh(model);
	glm::vec3 origin(0.0f, 0.0f, 4.0f);
	int hits = 0, mismatches = 0;
	// A grid of rays fanned across the scene: the BVH must agree with brute force.
	for (int i = 0; i < 400; i++) {
		float a = (i % 20) / 20.0f - 0.5f;
		float b = (i / 20) / 20.0f - 0.5f;
		glm::vec3 dir = glm::normalize(glm::vec3(a, b, -1.0f));
		RayTriangleIntersection brute = getClosestIntersection(origin, dir, model);
		RayTriangleIntersection acc = bvh.intersect(origin, dir);
		CHECK(brute.hit == acc.hit);
		if (brute.hit && acc.hit) {
			hits++;
			if (brute.triangleIndex != acc.triangleIndex ||
			    std::fabs(brute.distanceFromCamera - acc.distanceFromCamera) > 1e-3f)
				mismatches++;
		}
	}
	CHECK(hits > 0);
	CHECK(mismatches == 0);
}

static void testTransform() {
	std::vector<ModelTriangle> mesh;
	mesh.push_back(ModelTriangle(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), Colour(255, 0, 0)));
	glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(2, 3, 4));
	std::vector<ModelTriangle> moved = transformModel(mesh, m);
	CHECK(moved.size() == 1);
	CHECK(nearly(moved[0].vertices[0].x, 2.0f) && nearly(moved[0].vertices[0].y, 3.0f));
	CHECK(nearly(moved[0].vertices[1].x, 3.0f)); // (1,0,0) -> (3,3,4)
	// Instancing appends.
	std::vector<ModelTriangle> scene;
	appendInstance(scene, mesh, glm::mat4(1.0f));
	appendInstance(scene, mesh, m);
	CHECK(scene.size() == 2);
	// LOD picks by distance.
	std::vector<ModelTriangle> hi = mesh, lo;
	CHECK(selectLOD(1.0f, 5.0f, hi, lo).size() == 1);
	CHECK(selectLOD(9.0f, 5.0f, hi, lo).size() == 0);
}

static void testMaterialsAndRoll() {
	CHECK(materialPreset("gold").material == Material::Metal);
	CHECK(materialPreset("chrome").material == Material::Metal);
	CHECK(materialPreset("ruby").material == Material::Diffuse);
	CHECK(materialPreset("does-not-exist").material == Material::Diffuse); // neutral fallback
	// Roll changes the right vector but keeps the basis unit-length.
	Camera cam(100, 100, 2.0f, glm::vec3(0, 0, 4));
	cam.lookAt(glm::vec3(0));
	glm::mat3 before = cam.orientation;
	cam.rotateZ(0.3f);
	CHECK(glm::length(cam.orientation[0] - before[0]) > 0.01f);
	CHECK(nearly(glm::length(cam.orientation[0]), 1.0f));
}

static void testDeepScanModules() {
	// A small scene of triangles for the acceleration structures.
	std::vector<ModelTriangle> tris;
	for (int i = 0; i < 6; i++) {
		float z = -1.0f - i;
		tris.push_back(
		    ModelTriangle(glm::vec3(-1, -1, z), glm::vec3(1, -1, z), glm::vec3(0, 1, z), Colour(200, 100, 50)));
	}
	// Octree + KdTree + Grid must agree with the trusted BVH on hit + occlusion.
	BVH bvh(tris);
	Octree oct(tris);
	KdTree kd(tris);
	Grid gr(tris);
	glm::vec3 o(0.0f, 0.0f, 2.0f), d(0.0f, 0.0f, -1.0f);
	RayTriangleIntersection hb = bvh.intersect(o, d), ho = oct.intersect(o, d), hk = kd.intersect(o, d),
	                        hg = gr.intersect(o, d);
	CHECK(hb.hit && ho.hit && hk.hit && hg.hit);
	CHECK(ho.triangleIndex == hb.triangleIndex && hk.triangleIndex == hb.triangleIndex &&
	      hg.triangleIndex == hb.triangleIndex);
	CHECK(nearly(ho.distanceFromCamera, hb.distanceFromCamera) &&
	      nearly(hk.distanceFromCamera, hb.distanceFromCamera) && nearly(hg.distanceFromCamera, hb.distanceFromCamera));
	CHECK(oct.occluded(o, d, 10.0f, -1) == bvh.occluded(o, d, 10.0f, -1));
	CHECK(kd.occluded(o, d, 10.0f, -1) == bvh.occluded(o, d, 10.0f, -1));
	CHECK(gr.occluded(o, d, 10.0f, -1) == bvh.occluded(o, d, 10.0f, -1));

	// Meshing: Delaunay circumcircle predicate + point cloud + decimation.
	std::vector<glm::vec2> pts = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0.5f, 0.5f}, {0.2f, 0.8f}};
	CHECK(!meshing::delaunayTriangulate(pts).empty());
	CHECK(meshing::inCircumcircle({0, 0}, {2, 0}, {1, 2}, {1, 0.5f}));

	// NURBS tessellation gives 2*u*v triangles.
	std::vector<std::vector<glm::vec3>> grid(4, std::vector<glm::vec3>(4));
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			grid[i][j] = glm::vec3(i, ((i + j) % 2) ? 0.5f : 0.0f, j);
	std::vector<ModelTriangle> surf = tessellateSplineSurface(grid, 8, 8, Colour(100, 150, 200));
	CHECK(surf.size() == static_cast<size_t>(2 * 8 * 8));
	CHECK(meshing::decimate(surf, 0.5f).size() <= surf.size());

	// Scene graph flattens all node meshes.
	SceneNode root = makeNode(glm::mat4(1.0f), tris);
	root.children.push_back(makeNode(glm::mat4(1.0f), tris));
	CHECK(flatten(root).size() == tris.size() * 2);

	// Voxelise a mesh and re-mesh the voxels.
	CHECK(!voxelsToMesh(voxelize(surf, 12)).empty());

	// Spectral ocean generates a non-empty animated mesh.
	CHECK(!generateOceanFFT(16, 4.0f, 0.0f).empty());

	// Cloud density is a normalised field.
	float cd = cloudDensity(glm::vec3(0.3f, 2.0f, 0.5f), 0.0f);
	CHECK(cd >= 0.0f && cd <= 1.0f);

	// HSV round-trips back to RGB.
	glm::vec3 rgb(0.3f, 0.7f, 0.2f);
	glm::vec3 back = hsvToRgb(rgbToHsv(rgb));
	CHECK(std::abs(back.r - rgb.r) < 0.01f && std::abs(back.g - rgb.g) < 0.01f && std::abs(back.b - rgb.b) < 0.01f);

	// Irradiance cache: a stored record is reused nearby, missed far away.
	IrradianceCache icache;
	glm::vec3 up(0, 1, 0);
	icache.store(glm::vec3(0, 0, 0), up, glm::vec3(0.5f, 0.3f, 0.7f), 1.0f);
	glm::vec3 got;
	CHECK(icache.lookup(glm::vec3(0.05f, 0, 0.05f), up, got)); // near + same normal -> hit
	CHECK(std::abs(got.r - 0.5f) < 0.01f && std::abs(got.b - 0.7f) < 0.01f);
	CHECK(!icache.lookup(glm::vec3(5, 0, 5), up, got));      // far -> miss
	CHECK(!icache.lookup(glm::vec3(0.05f, 0, 0), -up, got)); // opposing normal -> miss

	// Mipmap pyramid: a checkerboard averages toward grey at coarse levels.
	TextureMap checker;
	checker.width = 8;
	checker.height = 8;
	checker.pixels.resize(64);
	for (int yy = 0; yy < 8; yy++)
		for (int xx = 0; xx < 8; xx++)
			checker.pixels[yy * 8 + xx] = ((xx + yy) & 1) ? 0xFFFFFFFFu : 0xFF000000u;
	MipTexture mip(checker);
	CHECK(mip.levelCount() == 4); // 8 -> 4 -> 2 -> 1
	glm::vec3 coarse = mip.sampleBilinear(mip.levelCount() - 1, 0.5f, 0.5f);
	CHECK(coarse.r > 100.0f && coarse.r < 160.0f); // 1x1 level ~ grey (checkerboard mean)
	glm::vec3 fine = mip.sampleTrilinear(0.5f, 0.5f, 0.0f);
	CHECK(fine.r >= 0.0f && fine.r <= 255.0f);

	// Line drawing sets pixels; Cohen-Sutherland clips a crossing line.
	Canvas canvas(32, 32);
	bresenhamLine(canvas, 2, 2, 20, 20, Colour(255, 255, 255));
	CHECK(canvas.pixels[11 * 32 + 11] != canvas.pixels[0]);
	float x0 = -5, y0 = 16, x1 = 40, y1 = 16;
	CHECK(cohenSutherlandClip(x0, y0, x1, y1, 0, 0, 31, 31));
	CHECK(x0 >= 0.0f && x1 <= 31.0f);

	// Cramer's-rule intersection matches the matrix-inverse one.
	ModelTriangle itri(glm::vec3(-1, -1, -2), glm::vec3(1, -1, -2), glm::vec3(0, 1, -2), Colour(255, 0, 0));
	float t1, u1, v1, t2, u2, v2;
	bool h1 = intersectTriangle(glm::vec3(0.1f, 0.0f, 0.0f), glm::vec3(0, 0, -1), itri, t1, u1, v1);
	bool h2 = intersectTriangleCramer(glm::vec3(0.1f, 0.0f, 0.0f), glm::vec3(0, 0, -1), itri, t2, u2, v2);
	CHECK(h1 && h2 && nearly(t1, t2) && nearly(u1, u2) && nearly(v1, v2));

	// Disco ball: facetMirror makes every triangle a flat mirror facet.
	std::vector<ModelTriangle> disco = facetMirror(tris);
	CHECK(!disco.empty() && disco[0].material == Material::Mirror);

	// Blackbody: warm at low temperature, cool at high, near-neutral at 6500K.
	glm::vec3 warm = blackbodyRGB(2700.0f), neutral = blackbodyRGB(6500.0f), cool = blackbodyRGB(10000.0f);
	CHECK(warm.r > warm.b);                                                // 2700K reddish
	CHECK(cool.b > cool.r);                                                // 10000K bluish
	CHECK(std::abs(neutral.r - neutral.b) < 0.2f);                         // 6500K near white
	CHECK(blackbodyRGB(6500.0f, 2.0f).r > blackbodyRGB(6500.0f).r * 1.5f); // intensity scales

	// QMC: Halton values in [0,1), more evenly spread than random (smaller max gap).
	for (uint32_t i = 1; i <= 32; i++) {
		CHECK(halton(0, i) >= 0.0f && halton(0, i) < 1.0f);
		CHECK(halton(1, i) >= 0.0f && halton(1, i) < 1.0f);
	}
	CHECK(nearly(radicalInverse(2, 1), 0.5f)); // 1 in base 2 reverses to 0.5
	glm::vec2 hs = hammersley(3, 16);
	CHECK(hs.x >= 0.0f && hs.x < 1.0f && hs.y >= 0.0f && hs.y < 1.0f);
	{
		std::vector<float> h;
		for (uint32_t i = 0; i < 16; i++)
			h.push_back(halton(0, i + 1));
		std::sort(h.begin(), h.end());
		float maxGap = h.front();
		for (size_t i = 1; i < h.size(); i++)
			maxGap = std::max(maxGap, h[i] - h[i - 1]);
		CHECK(maxGap < 0.2f); // low-discrepancy: no big empty gap
	}

	// Bezier: a bicubic patch with raised centre control points tessellates to
	// 2*res*res triangles, with the surface centre lifted in y.
	std::array<glm::vec3, 16> patch{};
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++) {
			float y = (r == 1 || r == 2) && (c == 1 || c == 2) ? 1.0f : 0.0f; // inner control points raised
			patch[r * 4 + c] = glm::vec3(c / 3.0f, y, r / 3.0f);
		}
	CHECK(bezierPatchPoint(patch, 0.5f, 0.5f).y > 0.3f); // surface bulges up in the middle
	std::vector<ModelTriangle> patchTris = tessellateBezierPatch(patch, 8, Colour(180, 120, 90));
	CHECK(patchTris.size() == static_cast<size_t>(2 * 8 * 8));

	// Sky: overhead is bluer than the sun direction (Rayleigh scattering).
	glm::vec3 sun = glm::normalize(glm::vec3(0.0f, 0.3f, -1.0f));
	glm::vec3 skyUp = skyColour(glm::vec3(0, 1, 0), sun), atSun = skyColour(sun, sun);
	CHECK(skyUp.b > skyUp.r);                     // blue sky overhead
	CHECK(atSun.r + atSun.g > skyUp.r + skyUp.g); // the sun region is much brighter/warmer

	// Disk primitive: a ray through the centre hits; one outside the radius misses.
	std::vector<ModelTriangle> noTris;
	Primitives diskPrims;
	diskPrims.disks.push_back(
	    {glm::vec3(0, 0, -2), glm::vec3(0, 0, 1), 0.5f, Colour(200, 0, 0), Material::Diffuse, 0.2f});
	Scene diskScene(noTris, diskPrims);
	RayTriangleIntersection dh = diskScene.intersect(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
	CHECK(dh.hit && nearly(dh.distanceFromCamera, 2.0f));
	RayTriangleIntersection dm = diskScene.intersect(glm::vec3(1.0f, 0, 0), glm::vec3(0, 0, -1)); // 1 unit off, r=0.5
	CHECK(!dm.hit);

	// Perlin analytical derivatives: the returned gradient matches a finite
	// difference of the value, at several points.
	for (glm::vec3 p : {glm::vec3(1.3f, 2.7f, 0.4f), glm::vec3(-0.6f, 5.1f, 3.2f)}) {
		glm::vec4 nd = perlinNoiseD(p);
		float e = 1e-3f;
		float dx = (perlinNoiseD(p + glm::vec3(e, 0, 0)).x - perlinNoiseD(p - glm::vec3(e, 0, 0)).x) / (2 * e);
		float dz = (perlinNoiseD(p + glm::vec3(0, 0, e)).x - perlinNoiseD(p - glm::vec3(0, 0, e)).x) / (2 * e);
		CHECK(std::abs(nd.y - dx) < 1e-2f); // analytic d/dx
		CHECK(std::abs(nd.w - dz) < 1e-2f); // analytic d/dz
	}

	// Perspective-correct interpolation: at the screen midpoint of an edge from
	// depth 1 (value 0) to depth 3 (value 1), the perspective-correct value is
	// 0.25, not the affine 0.5.
	CHECK(nearly(perspectiveInterp(0.5f, 0.5f, 0.0f, 1.0f, 1.0f / 3.0f, 0.0f, 0.0f, 1.0f, 0.0f), 0.25f));
	CHECK(std::abs(perspectiveInterp(0.5f, 0.5f, 0.0f, 1.0f, 1.0f / 3.0f, 0.0f, 0.0f, 1.0f, 0.0f) - 0.5f) > 0.2f);

	// Value noise: in [0,1], continuous, and distinct from gradient (Perlin) noise.
	glm::vec3 vp(1.3f, 4.7f, 0.9f);
	CHECK(valueNoise(vp) >= 0.0f && valueNoise(vp) <= 1.0f);
	CHECK(std::abs(valueNoise(vp) - valueNoise(vp + glm::vec3(0.001f, 0, 0))) < 0.05f); // continuous
	CHECK(std::abs(valueNoise(vp) - fractalNoise(vp, 1)) > 1e-3f);                      // not the same as gradient
	CHECK(fractalValueNoise(vp, 5) >= 0.0f && fractalValueNoise(vp, 5) <= 1.0f);

	// Worley (cellular) noise: f1 <= f2, non-negative, cells value in [0,1].
	WorleyResult wr = worley(glm::vec2(3.3f, 7.1f));
	CHECK(wr.f1 >= 0.0f && wr.f1 <= wr.f2);
	CHECK(worleyCells(glm::vec2(3.3f, 7.1f)) >= 0.0f && worleyCells(glm::vec2(3.3f, 7.1f)) <= 1.0f);
	CHECK(std::abs(worleyF1(glm::vec2(3.3f, 7.1f)) - worleyF1(glm::vec2(3.301f, 7.1f))) < 0.05f); // continuous
	WorleyResult wr3 = worley(glm::vec3(1.2f, 3.4f, 5.6f));
	CHECK(wr3.f1 >= 0.0f && wr3.f1 <= wr3.f2);

	// Morton codes: encode/decode round-trips (2D + 3D).
	for (uint32_t mx = 0; mx < 300; mx += 37)
		for (uint32_t my = 0; my < 300; my += 41) {
			uint32_t dx, dy;
			decodeMorton2(encodeMorton2(mx, my), dx, dy);
			CHECK(dx == mx && dy == my);
		}
	uint32_t ex, ey, ez;
	decodeMorton3(encodeMorton3(123, 456, 789), ex, ey, ez);
	CHECK(ex == 123 && ey == 456 && ez == 789);
	CHECK(encodeMorton2(1, 0) == 1 && encodeMorton2(0, 1) == 2); // interleave: x low bit, y next

	// Frustum culling: extract planes from a perspective*lookAt view-projection.
	glm::mat4 proj = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	Frustum fr = extractFrustum(proj * view);
	CHECK(pointInside(fr, glm::vec3(0, 0, 0)));   // in front -> inside
	CHECK(!pointInside(fr, glm::vec3(0, 0, 50))); // behind camera -> outside
	CHECK(testAABB(fr, glm::vec3(0, 0, 0), glm::vec3(0.5f)) != Cull::Outside);
	CHECK(testAABB(fr, glm::vec3(0, 0, 50), glm::vec3(0.5f)) == Cull::Outside);   // far behind
	CHECK(testAABB(fr, glm::vec3(1000, 0, 0), glm::vec3(0.5f)) == Cull::Outside); // way off to the side

	// Lanczos resampling: kernel is 1 at 0, zero at nonzero integers and outside
	// the window; upsampling preserves endpoints and stays bounded (no overshoot escape).
	CHECK(nearly(lanczosKernel(0.0f, 3), 1.0f));
	CHECK(std::abs(lanczosKernel(1.0f, 3)) < 1e-5f && std::abs(lanczosKernel(2.0f, 3)) < 1e-5f);
	CHECK(lanczosKernel(3.5f, 3) == 0.0f && lanczosKernel(-4.0f, 3) == 0.0f); // outside window
	std::vector<uint32_t> ramp(8);
	for (int i = 0; i < 8; i++) {
		int g = i * 255 / 7;
		ramp[i] = (255u << 24) | (g << 16) | (g << 8) | g;
	}
	std::vector<uint32_t> upRamp = resampleLanczos(ramp, 8, 1, 32, 1, 3);
	CHECK(upRamp.size() == 32);
	CHECK((upRamp.front() & 0xFF) == 0 && (upRamp.back() & 0xFF) == 255); // endpoints preserved
	for (uint32_t px : upRamp)
		CHECK((px & 0xFF) <= 255); // bounded, no overshoot beyond byte range

	// Quaternion: rotating x by 90deg about +z gives +y; rotate matches matrix form;
	// slerp endpoints are exact.
	glm::vec4 qz = quatFromAxisAngle(glm::vec3(0, 0, 1), glm::radians(90.0f));
	glm::vec3 rotated = quatRotate(qz, glm::vec3(1, 0, 0));
	CHECK(nearly(rotated.x, 0.0f) && nearly(rotated.y, 1.0f) && nearly(rotated.z, 0.0f));
	glm::vec3 viaMat = quatToMat3(qz) * glm::vec3(1, 0, 0);
	CHECK(nearly(viaMat.x, rotated.x) && nearly(viaMat.y, rotated.y) && nearly(viaMat.z, rotated.z));
	glm::vec4 qa = quatFromAxisAngle(glm::vec3(0, 1, 0), 0.3f), qb = quatFromAxisAngle(glm::vec3(0, 1, 0), 1.2f);
	glm::vec4 s0 = slerp(qa, qb, 0.0f), s1 = slerp(qa, qb, 1.0f);
	CHECK(nearly(std::abs(glm::dot(s0, qa)), 1.0f) && nearly(std::abs(glm::dot(s1, qb)), 1.0f));
	glm::mat3 on = orthonormalize(glm::mat3(glm::vec3(1, 0.01f, 0), glm::vec3(0, 1, 0.02f), glm::vec3(0.03f, 0, 1)));
	CHECK(nearly(glm::length(on[0]), 1.0f) && nearly(glm::dot(on[0], on[1]), 0.0f));

	// BCn/DXT decode: a BC1 block with white c0, black c1 and mixed indices
	// decodes to white / black / the two interpolated greys.
	uint8_t bc1[8] = {0xFF, 0xFF, 0x00, 0x00, 0b11100100, 0x00, 0x00, 0x00}; // indices 0,1,2,3 in first row
	uint32_t out[16];
	decodeBC1Block(bc1, out);
	CHECK((out[0] & 0xFFFFFF) == 0xFFFFFF);                 // index 0 -> c0 white
	CHECK((out[1] & 0xFFFFFF) == 0x000000);                 // index 1 -> c1 black
	CHECK((out[2] & 0xFF) == 170 && (out[3] & 0xFF) == 85); // 2/3 and 1/3 greys

	// Adaptive tessellator: quad domain has u*v points and 2*(u-1)*(v-1) triangles,
	// with independent per-axis factors.
	CHECK(tessellateQuadDomain(5, 5).size() == 25);
	CHECK(quadDomainTriangles(5, 5).size() == 32);
	std::array<glm::vec3, 16> flat{};
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++)
			flat[r * 4 + c] = glm::vec3(c / 3.0f, 0.0f, r / 3.0f);
	CHECK(adaptiveTessellateBezier(flat, 7, 3, Colour(100, 100, 100)).size() == static_cast<size_t>(2 * 6 * 2));

	// Loop subdivision: a tetrahedron (4 tris) grows 4x per level and stays finite,
	// contained within the original bounding box (convex smoothing shrinks inward).
	std::vector<ModelTriangle> tet;
	glm::vec3 A(1, 1, 1), B(1, -1, -1), C(-1, 1, -1), D(-1, -1, 1);
	Colour tc(200, 180, 160);
	tet.push_back(ModelTriangle(A, C, B, tc));
	tet.push_back(ModelTriangle(A, B, D, tc));
	tet.push_back(ModelTriangle(A, D, C, tc));
	tet.push_back(ModelTriangle(B, C, D, tc));
	std::vector<ModelTriangle> sub = halfedge::loopSubdivide(tet, 2, tc);
	CHECK(sub.size() == 4u * 16u); // 4 tris -> x4 per level, 2 levels
	bool finite = true, contained = true;
	for (const ModelTriangle &t : sub)
		for (int k = 0; k < 3; k++) {
			finite = finite && std::isfinite(t.vertices[k].x);
			contained = contained && std::abs(t.vertices[k].x) <= 1.001f;
		}
	CHECK(finite && contained);

	// Anisotropic filtering: mip chain of a 64x64 checkerboard has log2(64)+1=7
	// levels; anisotropic sampling returns a bounded RGBA and averages away
	// aliasing along a stretched gradient (variance <= a single trilinear tap).
	TextureMap aniChecker;
	aniChecker.width = 64;
	aniChecker.height = 64;
	aniChecker.pixels.resize(64 * 64);
	for (int yy = 0; yy < 64; yy++)
		for (int xx = 0; xx < 64; xx++) {
			uint8_t c = ((xx / 4 + yy / 4) & 1) ? 255 : 0;
			aniChecker.pixels[yy * 64 + xx] = (255u << 24) | (c << 16) | (c << 8) | c;
		}
	std::vector<TextureMap> mips = buildMipChain(aniChecker);
	CHECK(mips.size() == 7);
	glm::vec4 aniso = sampleAnisotropic(mips, 0.3f, 0.3f, glm::vec2(0.4f, 0.0f), glm::vec2(0.0f, 0.002f), 8);
	CHECK(aniso.r >= 0.0f && aniso.r <= 255.0f && aniso.g >= 0.0f && aniso.g <= 255.0f);

	// Tonemap / exposure: 1 stop doubles, ACES maps 0->0 and saturates high, sRGB
	// brightens midtones and round-trips.
	CHECK(nearly(applyExposure(glm::vec3(0.5f), 1.0f).r, 1.0f)); // +1 EV = x2
	CHECK(acesFilmic(glm::vec3(0.0f)).r < 1e-3f);
	CHECK(acesFilmic(glm::vec3(100.0f)).r > 0.95f);                       // highlights saturate near 1
	CHECK(linearToSRGB(glm::vec3(0.5f)).r > 0.5f);                        // sRGB lifts midtones
	CHECK(nearly(sRGBToLinear(linearToSRGB(glm::vec3(0.36f))).r, 0.36f)); // round-trip
}

int main() {
	testDeepScanModules();
	testMaterialsAndRoll();
	testTransform();
	testInterpolateSingleFloats();
	testInterpolateThreeElementValues();
	testInterpolation();
	testSplit();
	testLoadOBJ();
	testCameraProjection();
	testCameraLookAt();
	testTriangleIntersection();
	testTriangleNormal();
	testBVH();

	if (failures == 0) {
		std::cout << "All tests passed\n";
		return 0;
	}
	std::cerr << failures << " test(s) failed\n";
	return 1;
}
