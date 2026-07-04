// OpenCL path-tracing kernel: one work-item per pixel, brute-force triangle
// intersection, diffuse bounces with next-event estimation to a ceiling area
// light. Triangles are laid out as 16 floats each: v0(3) v1(3) v2(3) colour(3)
// pad(4). Radiance accumulates into `accum` across frames for progressive
// (real-time) refinement; the tone-mapped result is written to `outImg`.

#define PI 3.14159265f

uint xorshift(uint *s) {
	uint x = *s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return x;
}
float randf(uint *s) { return (xorshift(s) >> 8) * (1.0f / 16777216.0f); }

bool hitTri(float3 o, float3 d, float3 v0, float3 v1, float3 v2, float *t) {
	float3 e1 = v1 - v0, e2 = v2 - v0;
	float3 p = cross(d, e2);
	float det = dot(e1, p);
	if (fabs(det) < 1e-7f)
		return false;
	float inv = 1.0f / det;
	float3 tv = o - v0;
	float u = dot(tv, p) * inv;
	if (u < 0.0f || u > 1.0f)
		return false;
	float3 q = cross(tv, e1);
	float v = dot(d, q) * inv;
	if (v < 0.0f || u + v > 1.0f)
		return false;
	float tt = dot(e2, q) * inv;
	if (tt <= 1e-4f)
		return false;
	*t = tt;
	return true;
}

float3 triV(__global const float *tris, int i, int k) {
	return (float3)(tris[i * 16 + k * 3 + 0], tris[i * 16 + k * 3 + 1], tris[i * 16 + k * 3 + 2]);
}

float3 cosineHemi(float3 n, float u1, float u2) {
	float r = sqrt(u1);
	float th = 2.0f * PI * u2;
	float x = r * cos(th), y = r * sin(th), z = sqrt(fmax(0.0f, 1.0f - u1));
	float3 up = fabs(n.z) < 0.99f ? (float3)(0, 0, 1) : (float3)(1, 0, 0);
	float3 t = normalize(cross(up, n));
	float3 b = cross(n, t);
	return normalize(t * x + b * y + n * z);
}

__kernel void trace(__global const float *tris, const int numTris, const float4 camPos, const float4 camRight,
                    const float4 camUp, const float4 camFwd, const float f, const int W, const int H,
                    const int samples, const uint frameSeed, __global float *accum, const int accumFrames,
                    __global uchar *outImg) {
	int x = get_global_id(0), y = get_global_id(1);
	if (x >= W || y >= H)
		return;
	uint seed = (uint)(x * 1973 + y * 9277 + frameSeed * 26699) | 1u;

	const float3 Lc = (float3)(0, 0.33f, 0);
	const float lh = 0.18f;
	const float3 Ln = (float3)(0, -1, 0);
	const float larea = (2 * lh) * (2 * lh);
	const float3 Le = (float3)(30, 30, 30);

	float3 total = (float3)(0, 0, 0);
	for (int s = 0; s < samples; s++) {
		float px = x + randf(&seed), py = y + randf(&seed);
		float3 dc = (float3)((px - W * 0.5f) / f, -(py - H * 0.5f) / f, -1.0f);
		float3 dir = normalize(camRight.xyz * dc.x + camUp.xyz * dc.y + camFwd.xyz * dc.z);
		float3 o = camPos.xyz;
		float3 beta = (float3)(1, 1, 1), L = (float3)(0, 0, 0);
		for (int depth = 0; depth < 4; depth++) {
			float bt = 1e30f;
			int hi = -1;
			for (int i = 0; i < numTris; i++) {
				float t;
				if (hitTri(o, dir, triV(tris, i, 0), triV(tris, i, 1), triV(tris, i, 2), &t) && t < bt) {
					bt = t;
					hi = i;
				}
			}
			if (hi < 0)
				break;
			float3 v0 = triV(tris, hi, 0), v1 = triV(tris, hi, 1), v2 = triV(tris, hi, 2);
			float3 albedo = (float3)(tris[hi * 16 + 9], tris[hi * 16 + 10], tris[hi * 16 + 11]) / 255.0f;
			float3 p = o + dir * bt;
			float3 n = normalize(cross(v1 - v0, v2 - v0));
			if (dot(n, dir) > 0)
				n = -n;
			// Next-event estimation to the ceiling light.
			float3 lp = Lc + (float3)((randf(&seed) * 2 - 1) * lh, 0, (randf(&seed) * 2 - 1) * lh);
			float3 toL = lp - p;
			float dist = length(toL);
			toL /= dist;
			float cS = dot(n, toL), cL = dot(Ln, -toL);
			if (cS > 0 && cL > 0) {
				bool occ = false;
				float3 so = p + n * 1e-3f;
				for (int i = 0; i < numTris && !occ; i++) {
					float t;
					if (hitTri(so, toL, triV(tris, i, 0), triV(tris, i, 1), triV(tris, i, 2), &t) && t < dist - 2e-3f)
						occ = true;
				}
				if (!occ) {
					float G = cS * cL / (dist * dist);
					L += beta * (albedo / PI) * G * Le * larea;
				}
			}
			float3 nd = cosineHemi(n, randf(&seed), randf(&seed));
			beta *= albedo;
			o = p + n * 1e-3f;
			dir = nd;
		}
		total += L;
	}

	int idx = y * W + x;
	accum[idx * 3 + 0] += total.x;
	accum[idx * 3 + 1] += total.y;
	accum[idx * 3 + 2] += total.z;
	float invN = 1.0f / ((float)samples * (float)accumFrames);
	float3 c = (float3)(accum[idx * 3 + 0], accum[idx * 3 + 1], accum[idx * 3 + 2]) * invN;
	c = 255.0f * (c / (c + (float3)(1, 1, 1))); // Reinhard
	outImg[idx * 4 + 0] = (uchar)min(255.0f, c.x);
	outImg[idx * 4 + 1] = (uchar)min(255.0f, c.y);
	outImg[idx * 4 + 2] = (uchar)min(255.0f, c.z);
	outImg[idx * 4 + 3] = 255;
}
