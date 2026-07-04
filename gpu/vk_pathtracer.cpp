// Headless Vulkan compute path tracer.
//
// SDK-free: we declare no Vulkan prototypes (VK_NO_PROTOTYPES) and load the
// loader library (vulkan-1.dll / libvulkan.so.1) at runtime, resolving the
// entry points we need via vkGetInstanceProcAddr. So this builds against only
// the Khronos Vulkan-Headers and runs against any installed Vulkan driver - no
// SDK or import library required.
//
// Step 1 (this file for now): create an instance and enumerate the GPUs, to
// prove the loader + headers + driver path works. The compute pipeline that
// actually ray-traces follows.

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

// Open the platform Vulkan loader and return vkGetInstanceProcAddr, or nullptr.
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

const char *deviceTypeName(VkPhysicalDeviceType t) {
	switch (t) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return "discrete GPU";
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return "integrated GPU";
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return "virtual GPU";
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		return "CPU";
	default:
		return "other";
	}
}

} // namespace

int main() {
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = loadLoader();
	if (!vkGetInstanceProcAddr) {
		std::fprintf(stderr, "Vulkan: could not load the loader (vulkan-1.dll / libvulkan.so.1). "
		                     "Is a Vulkan driver installed?\n");
		return 1;
	}

	auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
	if (!vkCreateInstance) {
		std::fprintf(stderr, "Vulkan: vkCreateInstance not found\n");
		return 1;
	}

	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "rednoise-vk";
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici{};
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) {
		std::fprintf(stderr, "Vulkan: vkCreateInstance failed\n");
		return 1;
	}

	auto vkEnumeratePhysicalDevices =
	    reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
	auto vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
	    vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
	auto vkDestroyInstance =
	    reinterpret_cast<PFN_vkDestroyInstance>(vkGetInstanceProcAddr(instance, "vkDestroyInstance"));

	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(instance, &count, devices.data());

	std::printf("Vulkan: %u device(s)\n", count);
	for (VkPhysicalDevice d : devices) {
		VkPhysicalDeviceProperties p{};
		vkGetPhysicalDeviceProperties(d, &p);
		std::printf("  - %s (%s, Vulkan %u.%u.%u)\n", p.deviceName, deviceTypeName(p.deviceType),
		            VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion));
	}

	vkDestroyInstance(instance, nullptr);
	return count > 0 ? 0 : 1;
}
