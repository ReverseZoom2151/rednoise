# RedNoise Build Roadmap

Potential builds for the renderer, ordered roughly by dependency and difficulty.
Compiled from the Bristol computer-graphics coursework progression and a survey of
four reference implementations of this same Cornell-box renderer.

**Status legend:** [done] shipped · [next] recommended next step · [ext] extension

**Seen-in legend** (reference repos under `extras/`, evidence these are achievable):
`20` = COMS30020 (same framework as ours) · `15` = COMS30115 (briefs + graded
gallery) · `CG` = Computergraphics-master · `RT` = Raytracer_Bristol.

---

## What we already have

- [done] SDL3 window + pixel buffer, PPM/BMP screenshot, event loop.
- [done] Interpolation, gradients, DDA lines, stroked/filled/random triangles.
- [done] Affine texture-mapped triangles.
- [done] OBJ geometry + MTL material-colour loading (`mtllib`/`usemtl`).
- [done] Perspective camera: projection, lookAt, orbit, translate, pitch/yaw.
- [done] Three render modes: wireframe, z-buffered rasteriser, flat raytrace
  (ray-triangle closest-hit). The Cornell box renders.
- [done] SDL-free `Canvas` + headless PPM renderer (`render_headless`); CI
  renders the box on every push and uploads the images as artifacts.

The Cornell box now renders in 3D. Everything below builds on that toward a
fully-shaded ray tracer and beyond.

---

## Phase 1 - Core rasteriser (the Cornell box appears)

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Perspective projection | Project `ModelTriangle` vertices through a pinhole camera to canvas points (`u = -f*x/z + W/2`). First 3D image. | ● | 20, 15, CG |
| [done] Wireframe render mode | Draw projected triangle edges. Immediate visual payoff, minimal maths. | ● | 20, 15 |
| [done] Filled + depth (z-)buffer | Rasterise filled triangles with a per-pixel `1/z` buffer for correct occlusion. | ●● | 20, 15, CG |
| [done] Perspective-correct interpolation | Interpolate attributes in `1/z` space (needed before textures/lighting look right). | ●● | 15, CG |

## Phase 2 - Camera

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Translate + rotate | WASD/arrows move; X/Y/Z rotation matrices on position + orientation. | ● | 20, 15 |
| [done] lookAt | Build an orthonormal basis aimed at a target point. | ● | 20, 15, RT, CG |
| [done] Orbit | Rotate the camera around the scene each frame, re-aiming via lookAt. | ● | 20 |
| [done] Pitch / mouse-look | Up-down tilt and `SDL_GetRelativeMouseState` free-look. | ●● | 15 |

## Phase 3 - Core raytracer

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Ray-triangle intersection | Moller-Trumbore / matrix-inverse barycentric solve; closest-hit search. | ●● | 20, 15, CG, RT |
| [done] Hard shadows | Shadow ray to the light; occlusion darkens the pixel. | ●● | 20, 15, CG, RT |
| [done] Diffuse + ambient | Inverse-square proximity, angle-of-incidence Lambert, ambient floor. | ●● | 20, 15, CG, RT |
| [done] Render-mode toggle | Switch wireframe / rasterise / raytrace on number keys. | ● | 20 |

## Phase 4 - Shading models

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Specular highlights | Phong or Blinn-Phong `pow(R·V, n)` term. | ●● | 20, RT |
| [done] Per-vertex normals | Read `vn` from OBJ, else average adjacent face normals; enables smooth shading. | ●● | 20 |
| [done] Gouraud shading | Light per vertex, interpolate brightness across the triangle. | ●● | 20, 15 |
| [done] Phong shading | Interpolate the normal per pixel, light per pixel (default in ref). | ●●● | 20, 15 |

## Phase 5 - Materials and optics

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Mirror reflection | Recursive reflected ray for `Mirror` materials (our MTL already names them). | ●●● | 20, CG, RT |
| [done] Glass refraction | Snell's-law transmitted ray with a recursion-depth cap. | ●●● | 20, RT |
| [done] Fresnel blend | Mix reflection and refraction by view angle for realistic glass. | ●●● | 20 |
| [done] Raytraced texture sampling | Barycentric UV lookup at the hit point (extends our raster textures). | ●● | 20 |
| [done] Normal / bump mapping (procedural) | Perturb normals from a texture for surface detail. | ●●● | 15 |
| [done] Parallax mapping | Depth-parallax on textured surfaces. | ●●●● | 15 |
| [done] Environment map | Sky/background sampled by escaped rays. | ●●● | 15 |
| [done] Procedural textures (Perlin) | Noise-driven materials, e.g. wood grain, marble. | ●● | RT |

## Phase 6 - Shadows and lights

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Soft shadows / area lights | Jitter many samples over a light's area, average occlusion. | ●●● | 20, CG, 15 |
| [done] Shadow-buffer (rasteriser) | Render depth from the light, reproject to shade (shadow mapping). | ●●● | CG |
| [done] Multiple + typed lights | Point (attenuation), directional, spotlight cone. | ●● | RT, 15 |
| [done] Volumetric / 3D light source (area light) | Light with spatial extent for richer soft shadows. | ●●●● | 15 |

## Phase 7 - Performance

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Backface culling | Skip triangles facing away from the camera. | ● | CG, 15 |
| [done] Near-plane clipping | Clip geometry crossing the camera plane (fixes projection blowups). | ●●● | 15, CG |
| [done] BVH / kd-tree / octree | Spatial acceleration so raytracing scales past brute force O(n). | ●●●● | 15 |
| [done] Multithreading (OpenMP) | Parallelise the pixel loop; near-linear speedup. | ●● | CG, 15 |
| GPU (OpenCL / compute) | Offload tracing to the GPU for real-time. | ●●●●● | 15 |

## Phase 8 - Image quality

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Supersampling AA (path tracer) | N sub-samples per pixel, averaged. | ●● | CG, RT, 15 |
| [done] FXAA (post-process) | Cheap luma-edge anti-aliasing on the final image. | ●●● | CG |
| [done] Depth of field | Sample a lens aperture for focus blur. | ●●● | 15 |
| [done] Motion blur | Integrate over shutter time for moving geometry. | ●●● | 15 |
| [done] Image post-filters (tone-map) | Tone-map, bloom, colour grade the framebuffer. | ●● | 15 |

## Phase 9 - Global illumination (the deep end)

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] Multi-bounce indirect / colour bleeding | Secondary diffuse bounces for the classic Cornell glow. | ●●●● | 15, CG |
| [done] Photon mapping | Emit photons, gather radiance; caustics and GI. | ●●●●● | CG, 15 |
| [done] Caustics (via photon mapping) | Focused light through glass/water onto surfaces. | ●●●●● | 15 |
| [done] Path tracing | Monte-Carlo GI; the reference standard for realism. | ●●●●● | 15 |

## Phase 10 - Content, output and polish

| Build | What it adds | Diff | Seen in |
| --- | --- | --- | --- |
| [done] More primitives (analytic spheres, planes, quadrics) | Spheres, planes, quadrics (ellipsoid/cone/cylinder) alongside triangles. | ●● | RT |
| [done] Extra meshes | Load spheres, bunny, logo, higher-poly Cornell scenes. | ● | 20 |
| [done] Object transforms / instancing | Translate/scale/rotate matrices per object. | ●● | RT |
| [done] Animation system | Scripted camera/light choreography over frames. | ●● | 20 |
| [done] PPM sequence + video/GIF | Dump numbered frames, assemble to video (ffmpeg) or GIF. | ● | 20, 15 |
| [done] Level of detail / hierarchical models | Swap mesh detail by distance. | ●●● | 15 |
| [done] Fractal terrain / procedural scenes | Mountains, clouds, water via fractals/noise. | ●●●● | 15 |

## Moonshots (graded 100% in the unit's gallery)

- Real-time GPU path tracer with reflection + refraction.
- [done] Physically-based ocean-water surface (Gerstner waves, glass material, photon caustics).
- [done] Physically-based rendering (PBR) material model (metallic/roughness).

---

## Phase 11 - Reference-parity extras

Gaps found by a full re-scan of the four reference repositories in `extras/`
that were not in the original plan.

| Feature | Source | Status |
| --- | --- | --- |
| Analytic planes + quadrics (ellipsoid/cylinder/cone) | Raytracer_Bristol | [done] |
| Bloom post-filter | COMS30115 gallery | [done] |
| [done] Full frustum clipping (all planes) | COMS30115 clipping lecture | done |
| [done] Volumetric 3D light source | COMS30115 gallery | done |
| [done] Photon-map final gather | COMS30115 gallery | done |
| [done] Named PBR material presets + camera roll | Raytracer_Bristol / COMS30020 | done |
| [done] Classic radiosity (patches + gathering) | COMS30115 lecture 14 | done |
| [done] Bidirectional path tracing | COMS30115 lecture 15 | done |
| [done] Metropolis light transport (PSSMLT) | COMS30115 lecture 15 | done |

---

## Suggested near-term path

1. Perspective projection + wireframe (Phase 1) - the box finally appears.
2. Filled + z-buffer, then flat diffuse - a solid shaded render.
3. Raytracer with hard shadows + diffuse/ambient (Phase 3).
4. Phong shading and mirror/glass materials (Phases 4-5) - our MTL already
   names `Mirror`/`Glass`, so the hooks exist.
5. Pick a headline extension: soft shadows, an acceleration structure, or GI.

Each phase is independently demoable and screenshot-able, which also feeds the
animation/video output in Phase 10.
