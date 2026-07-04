// rn: a single command-line front-end for the renderer.
//
//   rn render <obj> [-o out.png] [-m mode] [-s WxH] [--spp N] [--scale S] [--cam-z Z]
//   rn animate <obj> [-o prefix] [--frames N] [--ease linear|smooth|reciprocal] [-s WxH]
//   rn version
//   rn help
//
// Output format follows the -o extension: .png (default) or .ppm.

#include "Camera.h"
#include "ObjLoader.h"
#include "Radiosity.h"
#include "Renderer.h"
#include <Canvas.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage() {
	std::cout << "rn - RedNoise renderer CLI\n\n"
	             "Usage:\n"
	             "  rn render <obj> [options]     render a single image\n"
	             "  rn animate <obj> [options]    render an orbiting camera sequence\n"
	             "  rn version                    print the library version\n"
	             "  rn help                       show this help\n\n"
	             "render options:\n"
	             "  -o, --out <file>     output file, .png or .ppm (default out.png)\n"
	             "  -m, --mode <mode>    wireframe|rasterised|raytraced|pathtraced|photon|radiosity\n"
	             "                       (default raytraced)\n"
	             "  -s, --size <WxH>     image size (default 640x480)\n"
	             "      --spp <N>        samples/pixel for pathtraced (default 64)\n"
	             "      --scale <S>      OBJ load scale (default 0.35)\n"
	             "      --cam-z <Z>      camera distance on +Z (default 4.0)\n\n"
	             "animate options:\n"
	             "  -o, --out <prefix>   frame prefix (default frame), writes prefix-000.ppm ...\n"
	             "      --frames <N>     number of frames (default 36)\n"
	             "      --ease <mode>    linear|smooth|reciprocal (default reciprocal)\n"
	             "  -s, --size <WxH>     frame size (default 320x240)\n";
}

// Return the value following flag `name` (or its short alias `alt`), or fallback.
std::string opt(const std::vector<std::string> &a, const std::string &name, const std::string &alt,
                const std::string &fallback) {
	for (size_t i = 0; i + 1 < a.size(); i++)
		if (a[i] == name || (!alt.empty() && a[i] == alt))
			return a[i + 1];
	return fallback;
}

void parseSize(const std::string &s, int &w, int &h) {
	size_t x = s.find('x');
	if (x != std::string::npos) {
		w = std::atoi(s.substr(0, x).c_str());
		h = std::atoi(s.substr(x + 1).c_str());
	}
}

void save(const Canvas &c, const std::string &path) {
	if (path.size() >= 4 && path.substr(path.size() - 4) == ".ppm")
		c.savePPM(path);
	else
		c.savePNG(path);
}

int doRender(const std::vector<std::string> &a) {
	if (a.empty()) {
		std::cerr << "render: missing <obj>\n";
		return 1;
	}
	std::string obj = a[0];
	std::string out = opt(a, "--out", "-o", "out.png");
	std::string mode = opt(a, "--mode", "-m", "raytraced");
	float scale = std::atof(opt(a, "--scale", "", "0.35").c_str());
	float camZ = std::atof(opt(a, "--cam-z", "", "4.0").c_str());
	int spp = std::atoi(opt(a, "--spp", "", "64").c_str());
	int w = 640, h = 480;
	parseSize(opt(a, "--size", "-s", "640x480"), w, h);

	std::vector<ModelTriangle> model = loadOBJ(obj, scale);
	if (model.empty()) {
		std::cerr << "render: no triangles loaded from " << obj << "\n";
		return 1;
	}
	Camera camera(w, h, 2.0f, glm::vec3(0.0f, 0.0f, camZ));
	camera.lookAt(glm::vec3(0.0f));
	Canvas canvas(w, h);

	if (mode == "wireframe")
		renderWireframe(model, camera, canvas);
	else if (mode == "rasterised" || mode == "rasterized")
		renderRasterised(model, camera, canvas);
	else if (mode == "raytraced")
		renderRaytraced(model, camera, canvas);
	else if (mode == "pathtraced")
		renderPathTraced(model, camera, canvas, spp);
	else if (mode == "photon")
		renderPhotonMapped(model, camera, canvas, 200000);
	else if (mode == "radiosity")
		renderRadiosity(model, camera, canvas);
	else {
		std::cerr << "render: unknown mode '" << mode << "'\n";
		return 1;
	}
	save(canvas, out);
	std::cout << "wrote " << out << " (" << w << "x" << h << ", " << mode << ")\n";
	return 0;
}

std::vector<float> easedParameters(int frames, const std::string &mode) {
	std::vector<float> t(frames, 0.0f);
	if (frames <= 1)
		return t;
	if (mode == "reciprocal") {
		std::vector<float> weight(frames);
		float mid = (frames - 1) / 2.0f;
		for (int i = 0; i < frames; i++)
			weight[i] = 1.0f / (1.0f + std::abs(i - mid));
		float acc = 0.0f, total = 0.0f;
		for (float wgt : weight)
			total += wgt;
		for (int i = 0; i < frames; i++) {
			t[i] = acc / total;
			acc += weight[i];
		}
	} else {
		for (int i = 0; i < frames; i++) {
			float lin = static_cast<float>(i) / frames;
			t[i] = (mode == "smooth") ? lin * lin * (3.0f - 2.0f * lin) : lin;
		}
	}
	return t;
}

int doAnimate(const std::vector<std::string> &a) {
	if (a.empty()) {
		std::cerr << "animate: missing <obj>\n";
		return 1;
	}
	std::string obj = a[0];
	std::string prefix = opt(a, "--out", "-o", "frame");
	std::string ease = opt(a, "--ease", "", "reciprocal");
	int frames = std::atoi(opt(a, "--frames", "", "36").c_str());
	int w = 320, h = 240;
	parseSize(opt(a, "--size", "-s", "320x240"), w, h);

	std::vector<ModelTriangle> model = loadOBJ(obj, 0.35f);
	if (model.empty()) {
		std::cerr << "animate: no triangles loaded from " << obj << "\n";
		return 1;
	}
	std::vector<float> t = easedParameters(frames, ease);
	Canvas canvas(w, h);
	for (int i = 0; i < frames; i++) {
		Camera camera(w, h, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
		camera.orbitY(2.0f * 3.14159265f * t[i], glm::vec3(0.0f));
		renderRasterised(model, camera, canvas);
		char name[512];
		std::snprintf(name, sizeof(name), "%s-%03d.ppm", prefix.c_str(), i);
		canvas.savePPM(name);
	}
	std::cout << "wrote " << frames << " frames (" << ease << " easing): " << prefix << "-000.ppm ...\n";
	return 0;
}

} // namespace

int main(int argc, char **argv) {
	std::vector<std::string> args(argv + 1, argv + argc);
	if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
		usage();
		return args.empty() ? 1 : 0;
	}
	std::string cmd = args[0];
	std::vector<std::string> rest(args.begin() + 1, args.end());
	if (cmd == "render")
		return doRender(rest);
	if (cmd == "animate")
		return doAnimate(rest);
	if (cmd == "version") {
		std::cout << "rednoise 0.1.0\n";
		return 0;
	}
	std::cerr << "unknown command '" << cmd << "'. Try 'rn help'.\n";
	return 1;
}
