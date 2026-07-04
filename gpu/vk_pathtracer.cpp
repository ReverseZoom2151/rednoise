// Headless Vulkan compute ray tracer.
//
// SDK-free: VK_NO_PROTOTYPES + a runtime-loaded loader (vulkan-1.dll /
// libvulkan.so.1), so it builds against only the Khronos Vulkan-Headers and runs
// against any installed Vulkan driver - no SDK or import library. The compute
// shader (gpu/pathtracer.comp) is precompiled to SPIR-V (gpu/spv_pathtracer.h),
// so the build needs no shader compiler either.
//
// Usage: vk_pathtracer <obj> [out.png] [width] [height]

// With a linked Vulkan SDK we use normal prototypes; without one we resolve
// entry points at runtime from the loader (see RN_VULKAN_LINKED in CMake).
#ifndef RN_VULKAN_LINKED
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "Camera.h"
#include "ObjLoader.h"
#include <Canvas.h>
#include <glm/glm.hpp>

#include "spv_pathtracer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

#ifndef RN_VULKAN_LINKED
// No SDK: open the platform Vulkan loader and return vkGetInstanceProcAddr.
PFN_vkGetInstanceProcAddr loadLoader() {
#ifdef _WIN32
	HMODULE lib = LoadLibraryA("vulkan-1.dll");
	if (!lib)
		return nullptr;
	return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
	    reinterpret_cast<void *>(GetProcAddress(lib, "vkGetInstanceProcAddr")));
#else
	void *lib = dlopen("libvulkan.so.1", RTLD_NOW);
	if (!lib)
		lib = dlopen("libvulkan.so", RTLD_NOW);
	if (!lib)
		return nullptr;
	return reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif
}
#endif

bool fail(const char *msg) {
	std::fprintf(stderr, "vulkan: %s\n", msg);
	return false;
}

// GPU-side triangle (std430: five vec4s = 80 bytes) and push constants.
struct GpuTri {
	float v0[4], v1[4], v2[4], n[4], col[4];
};
struct PushConstants {
	float camPos[4], camRight[4], camUp[4], camFwd[4], light[4];
	int32_t dims[4];
	float params[4];
};

void setVec(float *d, glm::vec3 v, float w = 0.0f) {
	d[0] = v.x;
	d[1] = v.y;
	d[2] = v.z;
	d[3] = w;
}

uint32_t findMemType(const VkPhysicalDeviceMemoryProperties &mp, uint32_t bits, VkMemoryPropertyFlags want) {
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
		if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
			return i;
	return ~0u;
}

} // namespace

int main(int argc, char **argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: vk_pathtracer <obj> [out.png] [width] [height]\n");
		return 1;
	}
	std::string objPath = argv[1];
	std::string outPath = argc > 2 ? argv[2] : "vk.png";
	int W = argc > 3 ? std::atoi(argv[3]) : 480;
	int H = argc > 4 ? std::atoi(argv[4]) : 360;

	std::vector<ModelTriangle> model = loadOBJ(objPath, 0.35f);
	if (model.empty())
		return fail("no triangles loaded"), 1;

	// Pack triangles for the GPU.
	std::vector<GpuTri> tris(model.size());
	for (size_t i = 0; i < model.size(); i++) {
		const ModelTriangle &t = model[i];
		setVec(tris[i].v0, t.vertices[0]);
		setVec(tris[i].v1, t.vertices[1]);
		setVec(tris[i].v2, t.vertices[2]);
		setVec(tris[i].n, t.normal);
		setVec(tris[i].col, glm::vec3(t.colour.red, t.colour.green, t.colour.blue) / 255.0f);
	}

#ifndef RN_VULKAN_LINKED
	PFN_vkGetInstanceProcAddr gipa = loadLoader();
	if (!gipa)
		return fail("could not load vulkan-1.dll / libvulkan.so.1 (is a driver installed?)"), 1;
	auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr, "vkCreateInstance"));
#endif
	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici{};
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS)
		return fail("vkCreateInstance failed"), 1;

#ifndef RN_VULKAN_LINKED
#define IFN(name) auto name = reinterpret_cast<PFN_##name>(gipa(instance, #name))
	IFN(vkEnumeratePhysicalDevices);
	IFN(vkGetPhysicalDeviceProperties);
	IFN(vkGetPhysicalDeviceQueueFamilyProperties);
	IFN(vkGetPhysicalDeviceMemoryProperties);
	IFN(vkCreateDevice);
	IFN(vkGetDeviceQueue);
	IFN(vkCreateBuffer);
	IFN(vkGetBufferMemoryRequirements);
	IFN(vkAllocateMemory);
	IFN(vkBindBufferMemory);
	IFN(vkMapMemory);
	IFN(vkCreateDescriptorSetLayout);
	IFN(vkCreateDescriptorPool);
	IFN(vkAllocateDescriptorSets);
	IFN(vkUpdateDescriptorSets);
	IFN(vkCreateShaderModule);
	IFN(vkCreatePipelineLayout);
	IFN(vkCreateComputePipelines);
	IFN(vkCreateCommandPool);
	IFN(vkAllocateCommandBuffers);
	IFN(vkBeginCommandBuffer);
	IFN(vkCmdBindPipeline);
	IFN(vkCmdBindDescriptorSets);
	IFN(vkCmdPushConstants);
	IFN(vkCmdDispatch);
	IFN(vkCmdPipelineBarrier);
	IFN(vkEndCommandBuffer);
	IFN(vkCreateFence);
	IFN(vkQueueSubmit);
	IFN(vkWaitForFences);
#undef IFN
#endif

	// Pick the first physical device and a compute-capable queue family.
	uint32_t nDev = 0;
	vkEnumeratePhysicalDevices(instance, &nDev, nullptr);
	if (nDev == 0)
		return fail("no Vulkan devices"), 1;
	std::vector<VkPhysicalDevice> devs(nDev);
	vkEnumeratePhysicalDevices(instance, &nDev, devs.data());
	VkPhysicalDevice phys = devs[0];
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(phys, &props);

	uint32_t nQ = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &nQ, nullptr);
	std::vector<VkQueueFamilyProperties> qf(nQ);
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &nQ, qf.data());
	uint32_t computeFamily = ~0u;
	for (uint32_t i = 0; i < nQ; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			computeFamily = i;
			break;
		}
	if (computeFamily == ~0u)
		return fail("no compute queue"), 1;

	VkPhysicalDeviceMemoryProperties memProps{};
	vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci{};
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = computeFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	VkDeviceCreateInfo dci{};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	VkDevice dev = VK_NULL_HANDLE;
	if (vkCreateDevice(phys, &dci, nullptr, &dev) != VK_SUCCESS)
		return fail("vkCreateDevice failed"), 1;
	VkQueue queue;
	vkGetDeviceQueue(dev, computeFamily, 0, &queue);

	// Host-visible storage buffers: triangles in, packed pixels out.
	const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	auto makeBuffer = [&](VkDeviceSize size, VkBuffer &buf, VkDeviceMemory &mem, void **mapped) {
		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCreateBuffer(dev, &bci, nullptr, &buf);
		VkMemoryRequirements mr{};
		vkGetBufferMemoryRequirements(dev, buf, &mr);
		VkMemoryAllocateInfo mai{};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = mr.size;
		mai.memoryTypeIndex = findMemType(memProps, mr.memoryTypeBits, hostFlags);
		vkAllocateMemory(dev, &mai, nullptr, &mem);
		vkBindBufferMemory(dev, buf, mem, 0);
		if (mapped)
			vkMapMemory(dev, mem, 0, size, 0, mapped);
	};

	VkDeviceSize triBytes = tris.size() * sizeof(GpuTri);
	VkDeviceSize pixBytes = VkDeviceSize(W) * H * sizeof(uint32_t);
	VkBuffer triBuf, pixBuf;
	VkDeviceMemory triMem, pixMem;
	void *triMap = nullptr;
	uint32_t *pixMap = nullptr;
	makeBuffer(triBytes, triBuf, triMem, &triMap);
	makeBuffer(pixBytes, pixBuf, pixMem, reinterpret_cast<void **>(&pixMap));
	std::memcpy(triMap, tris.data(), triBytes);

	// Descriptor set: two storage buffers.
	VkDescriptorSetLayoutBinding binds[2]{};
	for (int i = 0; i < 2; i++) {
		binds[i].binding = i;
		binds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binds[i].descriptorCount = 1;
		binds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	VkDescriptorSetLayoutCreateInfo dslci{};
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = binds;
	VkDescriptorSetLayout setLayout;
	vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &setLayout);

	VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
	VkDescriptorPoolCreateInfo dpci{};
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &poolSize;
	VkDescriptorPool pool;
	vkCreateDescriptorPool(dev, &dpci, nullptr, &pool);
	VkDescriptorSetAllocateInfo dsai{};
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = pool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &setLayout;
	VkDescriptorSet set;
	vkAllocateDescriptorSets(dev, &dsai, &set);

	VkDescriptorBufferInfo triInfo{triBuf, 0, triBytes}, pixInfo{pixBuf, 0, pixBytes};
	VkWriteDescriptorSet writes[2]{};
	for (int i = 0; i < 2; i++) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}
	writes[0].pBufferInfo = &triInfo;
	writes[1].pBufferInfo = &pixInfo;
	vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);

	// Compute pipeline from the precompiled SPIR-V.
	VkShaderModuleCreateInfo smci{};
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = sizeof(kPathtracerSpv);
	smci.pCode = kPathtracerSpv;
	VkShaderModule shader;
	vkCreateShaderModule(dev, &smci, nullptr, &shader);

	VkPushConstantRange pcRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)};
	VkPipelineLayoutCreateInfo plci{};
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &setLayout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcRange;
	VkPipelineLayout pipeLayout;
	vkCreatePipelineLayout(dev, &plci, nullptr, &pipeLayout);

	VkComputePipelineCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = shader;
	cpci.stage.pName = "main";
	cpci.layout = pipeLayout;
	VkPipeline pipeline;
	if (vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline) != VK_SUCCESS)
		return fail("vkCreateComputePipelines failed"), 1;

	// Push constants: camera (matching the CPU renderer) + a point light.
	Camera cam(W, H, 2.0f, glm::vec3(0.0f, 0.0f, 4.0f));
	cam.lookAt(glm::vec3(0.0f));
	PushConstants pc{};
	setVec(pc.camPos, cam.position);
	setVec(pc.camRight, cam.orientation[0]);
	setVec(pc.camUp, cam.orientation[1]);
	setVec(pc.camFwd, cam.orientation[2]);
	setVec(pc.light, glm::vec3(0.0f, 0.35f, 0.3f), 1.6f);
	pc.dims[0] = W;
	pc.dims[1] = H;
	pc.dims[2] = static_cast<int>(tris.size());
	pc.params[0] = cam.focalLength * cam.scale;

	// Record: dispatch one workgroup per 8x8 tile, then barrier for host read.
	VkCommandPoolCreateInfo cpi{};
	cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpi.queueFamilyIndex = computeFamily;
	VkCommandPool cmdPool;
	vkCreateCommandPool(dev, &cpi, nullptr, &cmdPool);
	VkCommandBufferAllocateInfo cbai{};
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = cmdPool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cbai, &cmd);

	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &set, 0, nullptr);
	vkCmdPushConstants(cmd, pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
	vkCmdDispatch(cmd, (W + 7) / 8, (H + 7) / 8, 1);
	VkMemoryBarrier mb{};
	mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr,
	                     0, nullptr);
	vkEndCommandBuffer(cmd);

	VkFenceCreateInfo fci{};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	vkCreateFence(dev, &fci, nullptr, &fence);
	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS)
		return fail("vkQueueSubmit failed"), 1;
	vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);

	// Read back into a Canvas (same 0xAARRGGBB packing) and save.
	Canvas canvas(W, H);
	std::memcpy(canvas.pixels.data(), pixMap, pixBytes);
	if (outPath.size() >= 4 && outPath.substr(outPath.size() - 4) == ".ppm")
		canvas.savePPM(outPath);
	else
		canvas.savePNG(outPath);

	std::printf("vk_pathtracer: %s on %s, %d triangles -> %s (%dx%d)\n", "rendered", props.deviceName,
	            static_cast<int>(tris.size()), outPath.c_str(), W, H);
	return 0;
}
