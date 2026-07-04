/* Minimal C program using the rednoise C ABI: load the Cornell box, ray-trace
 * it, and write the result as a PPM. Proves the library is consumable from pure
 * C (no C++), which is the same surface Rust / Python / Go bindings target.
 *
 *   c_consumer assets/cornell-box.obj out.ppm
 */
#include <rednoise/rednoise.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	const char *obj = argc > 1 ? argv[1] : "assets/cornell-box.obj";
	const char *out = argc > 2 ? argv[2] : "capi.ppm";
	const int width = 320, height = 240;

	printf("rednoise %s\n", rn_version());
	rn_scene *scene = rn_scene_load_obj(obj, 0.35f);
	if (!scene) {
		fprintf(stderr, "failed to load %s\n", obj);
		return 1;
	}
	printf("loaded %d triangles\n", rn_scene_triangle_count(scene));

	unsigned char *rgba = (unsigned char *)malloc((size_t)width * height * 4);
	if (!rgba || !rn_render(scene, RN_RAYTRACED, width, height, 4.0f, 8, rgba)) {
		fprintf(stderr, "render failed\n");
		return 1;
	}

	FILE *f = fopen(out, "wb");
	if (!f) {
		fprintf(stderr, "cannot open %s\n", out);
		return 1;
	}
	fprintf(f, "P6\n%d %d\n255\n", width, height);
	for (int i = 0; i < width * height; i++)
		fwrite(&rgba[i * 4], 1, 3, f); /* RGB, drop alpha */
	fclose(f);

	free(rgba);
	rn_scene_free(scene);
	printf("wrote %s\n", out);
	return 0;
}
