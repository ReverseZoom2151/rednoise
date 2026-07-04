// Host program for the OpenCL path tracer: loads the Cornell box, uploads the
// triangles to the GPU, runs pathtracer.cl over `frames` progressive passes,
// times each pass, and writes the tone-mapped image to PPM. Reports the device
// and the achieved frames-per-second.
//
// Build (from repo root):
//   clang++ -std=c++23 gpu/gpu_pathtracer.cpp <engine .cpp files> \
//     -Iframework -Ithird_party -Isrc -I"<CUDA>/include" "<CUDA>/lib/x64/OpenCL.lib" -o gpu_pt
// Run:
//   ./gpu_pt assets/cornell-box.obj out.ppm 320 240 8 60

#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>

#include "Camera.h"
#include "ObjLoader.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void check(cl_int err, const char *what) {
	if (err != CL_SUCCESS) {
		std::fprintf(stderr, "OpenCL error %d at %s\n", err, what);
		std::exit(1);
	}
}

static std::string readFile(const char *path) {
	std::ifstream f(path);
	if (!f) {
		std::fprintf(stderr, "cannot open %s\n", path);
		std::exit(1);
	}
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

int main(int argc, char **argv) {
	const char *objPath = argc > 1 ? argv[1] : "assets/cornell-box.obj";
	const char *outPath = argc > 2 ? argv[2] : "gpu.ppm";
	int W = argc > 3 ? std::atoi(argv[3]) : 320;
	int H = argc > 4 ? std::atoi(argv[4]) : 240;
	int samples = argc > 5 ? std::atoi(argv[5]) : 8;
	int frames = argc > 6 ? std::atoi(argv[6]) : 60;

	// Load geometry and flatten to 16 floats per triangle.
	std::vector<ModelTriangle> model = loadOBJ(objPath, 0.35f);
	for (ModelTriangle &t : model)
		t.material = Material::Diffuse;
	std::vector<float> tris;
	tris.reserve(model.size() * 16);
	for (const ModelTriangle &t : model) {
		for (int v = 0; v < 3; v++) {
			tris.push_back(t.vertices[v].x);
			tris.push_back(t.vertices[v].y);
			tris.push_back(t.vertices[v].z);
		}
		tris.push_back(t.colour.red);
		tris.push_back(t.colour.green);
		tris.push_back(t.colour.blue);
		for (int p = 0; p < 4; p++)
			tris.push_back(0.0f);
	}
	int numTris = static_cast<int>(model.size());

	Camera cam(W, H, 2.0f, glm::vec3(0, 0, 4));
	cam.lookAt(glm::vec3(0));
	cl_float4 camPos = {{cam.position.x, cam.position.y, cam.position.z, 0}};
	cl_float4 camRight = {{cam.orientation[0].x, cam.orientation[0].y, cam.orientation[0].z, 0}};
	cl_float4 camUp = {{cam.orientation[1].x, cam.orientation[1].y, cam.orientation[1].z, 0}};
	cl_float4 camFwd = {{cam.orientation[2].x, cam.orientation[2].y, cam.orientation[2].z, 0}};
	float f = cam.focalLength * cam.scale;

	// OpenCL setup.
	cl_platform_id platform;
	check(clGetPlatformIDs(1, &platform, nullptr), "platform");
	cl_device_id device;
	check(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr), "device");
	char devName[256];
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(devName), devName, nullptr);
	cl_int err;
	cl_context ctx = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
	check(err, "context");
	cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, device, nullptr, &err);
	check(err, "queue");

	std::string src = readFile("gpu/pathtracer.cl");
	const char *srcPtr = src.c_str();
	size_t srcLen = src.size();
	cl_program prog = clCreateProgramWithSource(ctx, 1, &srcPtr, &srcLen, &err);
	check(err, "program");
	if (clBuildProgram(prog, 1, &device, "", nullptr, nullptr) != CL_SUCCESS) {
		size_t logSize;
		clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
		std::vector<char> log(logSize);
		clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
		std::fprintf(stderr, "build log:\n%s\n", log.data());
		std::exit(1);
	}
	cl_kernel kernel = clCreateKernel(prog, "trace", &err);
	check(err, "kernel");

	std::vector<float> accumZero(static_cast<size_t>(W) * H * 3, 0.0f);
	std::vector<uint8_t> outImg(static_cast<size_t>(W) * H * 4, 0);
	cl_mem trisBuf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, tris.size() * sizeof(float),
	                                tris.data(), &err);
	check(err, "trisBuf");
	cl_mem accumBuf = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumZero.size() * sizeof(float),
	                                 accumZero.data(), &err);
	check(err, "accumBuf");
	cl_mem outBuf = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, outImg.size(), nullptr, &err);
	check(err, "outBuf");

	clSetKernelArg(kernel, 0, sizeof(cl_mem), &trisBuf);
	clSetKernelArg(kernel, 1, sizeof(int), &numTris);
	clSetKernelArg(kernel, 2, sizeof(cl_float4), &camPos);
	clSetKernelArg(kernel, 3, sizeof(cl_float4), &camRight);
	clSetKernelArg(kernel, 4, sizeof(cl_float4), &camUp);
	clSetKernelArg(kernel, 5, sizeof(cl_float4), &camFwd);
	clSetKernelArg(kernel, 6, sizeof(float), &f);
	clSetKernelArg(kernel, 7, sizeof(int), &W);
	clSetKernelArg(kernel, 8, sizeof(int), &H);
	clSetKernelArg(kernel, 9, sizeof(int), &samples);
	clSetKernelArg(kernel, 11, sizeof(cl_mem), &accumBuf);
	clSetKernelArg(kernel, 13, sizeof(cl_mem), &outBuf);

	size_t global[2] = {static_cast<size_t>(W), static_cast<size_t>(H)};
	double totalMs = 0.0;
	for (int frame = 0; frame < frames; frame++) {
		cl_uint seed = static_cast<cl_uint>(frame + 1);
		int accumFrames = frame + 1;
		clSetKernelArg(kernel, 10, sizeof(cl_uint), &seed);
		clSetKernelArg(kernel, 12, sizeof(int), &accumFrames);
		auto t0 = std::chrono::high_resolution_clock::now();
		check(clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, global, nullptr, 0, nullptr, nullptr), "enqueue");
		clFinish(queue);
		auto t1 = std::chrono::high_resolution_clock::now();
		totalMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
	}

	check(clEnqueueReadBuffer(queue, outBuf, CL_TRUE, 0, outImg.size(), outImg.data(), 0, nullptr, nullptr), "read");

	std::ofstream out(outPath, std::ios::binary);
	out << "P6\n" << W << " " << H << "\n255\n";
	for (int i = 0; i < W * H; i++)
		out.write(reinterpret_cast<char *>(&outImg[i * 4]), 3);

	double avgMs = totalMs / frames;
	std::printf("device: %s\n", devName);
	std::printf("%dx%d, %d triangles, %d samples/frame, %d frames\n", W, H, numTris, samples, frames);
	std::printf("avg %.3f ms/frame  ->  %.1f fps  (progressive, %d total spp)\n", avgMs, 1000.0 / avgMs,
	            samples * frames);
	return 0;
}
