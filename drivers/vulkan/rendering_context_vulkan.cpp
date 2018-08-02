/*************************************************************************/
/*  rendering_context_vulkan.cpp                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "rendering_context_vulkan.h"
#include "os/os.h"
#include "os_windows.h"
#include "project_settings.h"
#include "thirdparty/shaderc/src/libshaderc/include/shaderc/shaderc.h"
#include "version_generated.gen.h"

#include <Windows.h>

#include "platform/windows/glad_vulkan_win.h"

#if defined(OPENGL_ENABLED) || defined(GLES_ENABLED)

RenderingContextVulkan::RenderingContextVulkan() {
}

bool RenderingContextVulkan::_is_swap_chain_adequate(const VkPhysicalDevice p_physical_device) {
	bool extensions_supported = _check_device_extension_support(p_physical_device);

	bool swapChainAdequate = false;
	if (extensions_supported) {
		SwapChainSupportDetails swapChainSupport = _query_swap_chain_support(p_physical_device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.present_modes.empty();
	}

	if (!swapChainAdequate) {
		return false;
	}

	return true;
}


Vector<VkCommandBuffer> &RenderingContextVulkan::_get_command_buffers() {
	return command_buffers;
}

Vector<VkSemaphore> &RenderingContextVulkan::_get_image_available_semaphore() {
	return image_available_semaphore;
}

Vector<VkSemaphore> &RenderingContextVulkan::_get_render_finished_semaphore() {
	return render_finished_semaphore;
}

VkDevice *RenderingContextVulkan::_get_device() {
	return &device;
}

VkInstance *RenderingContextVulkan::_get_instance() {
	return &instance;
}

VkSwapchainKHR *RenderingContextVulkan::_get_swap_chain() {
	return &swap_chain;
}

VkQueue *RenderingContextVulkan::_get_present_queue() {
	return &present_queue;
}

VkQueue *RenderingContextVulkan::_get_graphics_queue() {
	return &graphics_queue;
}

VkResult RenderingContextVulkan::_create_debug_report_callback_EXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback) {
	PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != NULL) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void RenderingContextVulkan::_destroy_debug_report_callback_EXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator) {
	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != NULL) {
		func(instance, callback, pAllocator);
	}
}


VKAPI_ATTR VkBool32 VKAPI_CALL RenderingContextVulkan::_debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userData) {
	print_line("validation layer: " + String(msg));

	return VK_FALSE;
}

void RenderingContextVulkan::make_current() {
}

int RenderingContextVulkan::get_window_width() {

	return OS::get_singleton()->get_video_mode().width;
}

int RenderingContextVulkan::get_window_height() {

	return OS::get_singleton()->get_video_mode().height;
}

void RenderingContextVulkan::swap_buffers() {
}

void RenderingContextVulkan::set_use_vsync(bool p_use) {
	// TODO: Implement vsync
	use_vsync = p_use;
}

bool RenderingContextVulkan::is_using_vsync() const {

	return use_vsync;
}

bool RenderingContextVulkan::_check_device_extension_support(VkPhysicalDevice device) {
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

	Vector<VkExtensionProperties> available_extensions;
	available_extensions.resize(extension_count);
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions.ptrw());

	Set<CharString> required_extensions;
	for (Set<CharString>::Element *E = device_extensions.front(); E; E = E->next()) {
		required_extensions.insert(E->get());
	}

	for (int i = 0; i < available_extensions.size(); i++) {
		required_extensions.erase(String(available_extensions[i].extensionName).utf8());
	}

	return required_extensions.size() == 0;
}

RenderingContextVulkan::SwapChainSupportDetails RenderingContextVulkan::_query_swap_chain_support(VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);

	if (format_count != 0) {
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.ptrw());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);

	if (present_mode_count != 0) {
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.ptrw());
	}

	return details;
}

VkSurfaceFormatKHR RenderingContextVulkan::_choose_swap_surface_format(const Vector<VkSurfaceFormatKHR> &available_formats) {
	if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}
	for (int i = 0; i < available_formats.size(); i++) {
		if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return available_formats[i];
		}
	}

	return available_formats[0];
}

VkPresentModeKHR RenderingContextVulkan::_choose_swap_present_mode(const Vector<VkPresentModeKHR> available_present_modes) {
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (int i = 0; i < available_present_modes.size(); i++) {
		if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return available_present_modes[i];
		} else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			best_mode = available_present_modes[i];
		}
	}

	return best_mode;
}

VkExtent2D RenderingContextVulkan::_choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actual_extent = { OS::get_singleton()->get_video_mode(0).width, OS::get_singleton()->get_video_mode(0).height };

		actual_extent.width = MAX(capabilities.minImageExtent.width, MIN(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height = MAX(capabilities.minImageExtent.height, MIN(capabilities.maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

Error RenderingContextVulkan::_pick_physical_device() {
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(*_get_instance(), &device_count, NULL);
	if (device_count == 0) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't find GPUs with Vulkan support");
	}

	Vector<VkPhysicalDevice> devices;
	devices.resize(device_count);
	vkEnumeratePhysicalDevices(*_get_instance(), &device_count, devices.ptrw());

	// Take the best available GPU
	int max_score = -1;
	for (int i = 0; i < devices.size(); i++) {
		int score = _get_physical_device_score(devices[i]);
		if (score > max_score) {
			physical_device = devices[i];
		}
	}

	if (physical_device == VK_NULL_HANDLE) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't find a suitable GPU");
	}

	glad_vk_version = gladLoaderLoadVulkan(*_get_instance(), physical_device, NULL);
	if (!glad_vk_version) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't Re-load Vulkan Symbols With Physical Device.");
	}

	return OK;
}

Error RenderingContextVulkan::_create_instance() {
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	const String name = ProjectSettings::get_singleton()->get("application/config/name");
	const String product_version = ProjectSettings::get_singleton()->get("application/product_version");
	const String application_name = name + " " + product_version;
	const CharString cstr = application_name.utf8();
	app_info.pApplicationName = cstr.ptr();
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = VERSION_NAME;
#if defined(VERSION_PATCH)
	const int patch = VERSION_PATCH;
#else
	const int patch = 0;
#endif
	app_info.engineVersion = VK_MAKE_VERSION(VERSION_MAJOR, VERSION_MINOR, patch);
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = NULL;
	create_info.pApplicationInfo = &app_info;
	create_info.flags = 0;

	extensions.insert(String(VK_KHR_SURFACE_EXTENSION_NAME).utf8());
	extensions.insert(String(VK_KHR_WIN32_SURFACE_EXTENSION_NAME).utf8());
	if (enable_validation) {
		extensions.insert(String(VK_EXT_DEBUG_REPORT_EXTENSION_NAME).utf8());
	}
	Vector<const char *> e;
	for (Set<CharString>::Element *E = extensions.front(); E; E = E->next()) {
		e.push_back(E->get().ptr());
	}
	create_info.enabledExtensionCount = e.size();
	create_info.ppEnabledExtensionNames = e.ptr();

	create_info.enabledLayerCount = 0;

	if (vkCreateInstance(&create_info, NULL, _get_instance()) != VK_SUCCESS) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't Create A Vulkan Instance.");
	}

	return OK;
}

RenderingContextVulkan::~RenderingContextVulkan() {
	vkDestroySurfaceKHR(*_get_instance(), surface, NULL);
	vkDestroyDevice(*_get_device(), NULL);
	vkDestroyInstance(*_get_instance(), NULL);
	gladLoaderUnloadVulkan();
}


Vector<VkImage> *RenderingContextVulkan::_get_swap_chain_images() {
	return &swap_chain_images;
}

VkCommandPool &RenderingContextVulkan::_get_command_pool() {
	return command_pool;
}

VkRenderPass &RenderingContextVulkan::_get_render_pass() {
	return render_pass;
}

Vector<VkFramebuffer> &RenderingContextVulkan::_get_swap_chain_framebuffers() {
	return swap_chain_framebuffers;
}

VkPipeline &RenderingContextVulkan::_get_graphics_pipeline() {
	return graphics_pipeline;
}

VkExtent2D &RenderingContextVulkan::_get_swap_chain_extent() {
	return swap_chain_extent;
}

VkPhysicalDevice &RenderingContextVulkan::_get_physical_device() {
	return physical_device;
}

Vector<VkImageView> &RenderingContextVulkan::_get_swap_chain_image_views() {
	return swap_chain_image_views;
}

VkPipelineLayout &RenderingContextVulkan::_get_pipeline_layout() {
	return pipeline_layout;
}

VkFormat &RenderingContextVulkan::_get_swap_chain_image_format() {
	return swap_chain_image_format;
}

Vector<VkFence> *RenderingContextVulkan::_get_in_flight_fences() {
	return &in_flight_fences;
}

Error RenderingContextVulkan::_create_swap_chain() {
	SwapChainSupportDetails swap_chain_support = _query_swap_chain_support(physical_device);

	VkSurfaceFormatKHR surface_format = _choose_swap_surface_format(swap_chain_support.formats);
	VkPresentModeKHR present_mode = _choose_swap_present_mode(swap_chain_support.present_modes);
	VkExtent2D extent = _choose_swap_extent(swap_chain_support.capabilities);

	uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
	if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
		image_count = swap_chain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;

	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = _pick_queue_families(physical_device);
	uint32_t queue_family_indices[] = { (uint32_t)indices.graphics_family, (uint32_t)indices.present_family };

	if (indices.graphics_family != indices.present_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = swap_chain_support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;

	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(*_get_device(), &create_info, NULL, _get_swap_chain()) != VK_SUCCESS) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't Create A Vulkan Swap Chain.")
	}

	vkGetSwapchainImagesKHR(*_get_device(), *_get_swap_chain(), &image_count, NULL);
	_get_swap_chain_images()->resize(image_count);
	vkGetSwapchainImagesKHR(*_get_device(), *_get_swap_chain(), &image_count, _get_swap_chain_images()->ptrw());

	swap_chain_image_format = surface_format.format;
	swap_chain_extent = extent;

	return OK;
}

Error RenderingContextVulkan::_create_logical_device() {
	QueueFamilyIndices indices = _pick_queue_families(physical_device);

	Vector<VkDeviceQueueCreateInfo> queue_create_infos;
	Set<int> unique_queue_families;
	unique_queue_families.insert(indices.graphics_family);
	unique_queue_families.insert(indices.present_family);

	float queue_priority = 1.0f;
	for (Set<int>::Element *E = unique_queue_families.front(); E; E = E->next()) {
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = E->get();
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceFeatures device_features = {};
	device_features.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	create_info.queueCreateInfoCount = queue_create_infos.size();
	create_info.pQueueCreateInfos = queue_create_infos.ptr();

	create_info.pEnabledFeatures = &device_features;

	Vector<const char *> e;
	for (Set<CharString>::Element *E = device_extensions.front(); E; E = E->next()) {
		e.push_back(E->get().ptr());
	}

	create_info.enabledExtensionCount = e.size();
	create_info.ppEnabledExtensionNames = e.ptr();

	create_info.enabledLayerCount = 0;

	if (vkCreateDevice(physical_device, &create_info, NULL, _get_device()) != VK_SUCCESS) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't Create A Vulkan Logical Device.");
	}

	glad_vk_version = gladLoaderLoadVulkan(*_get_instance(), physical_device, *_get_device());
	if (!glad_vk_version) {
		ERR_FAIL_V(ERR_CANT_CREATE, "Can't Re-load Vulkan Symbols With Device.");
	}

	vkGetDeviceQueue(*_get_device(), indices.graphics_family, 0, _get_graphics_queue());
	vkGetDeviceQueue(*_get_device(), indices.present_family, 0, _get_present_queue());

	return OK;
}

int RenderingContextVulkan::_get_physical_device_score(const VkPhysicalDevice p_physical_device) {
	VkPhysicalDeviceProperties device_properties;
	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceProperties(p_physical_device, &device_properties);
	vkGetPhysicalDeviceFeatures(p_physical_device, &device_features);

	int score = 0;
	// Device type
	if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score = 1;
	} else if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
		score = 0;
	} else {
		return -1;
	}

	// Geometry shader
	if (!device_features.geometryShader) {
		return -1;
	}
	if (!device_features.samplerAnisotropy) {
		return -1;
	}
	if (!_pick_queue_families(p_physical_device).is_complete()) {
		return -1;
	}
	if (!_is_swap_chain_adequate(p_physical_device)) {
		return -1;
	}

	if (!_check_device_extension_support(p_physical_device)) {
		return -1;
	}

	// Anisotropy support
	if (!device_features.samplerAnisotropy)
		return -1;

	return score;
}

struct RenderingContextVulkan::QueueFamilyIndices RenderingContextVulkan::_pick_queue_families(VkPhysicalDevice p_physical_device) {
	struct RenderingContextVulkan::QueueFamilyIndices indices;
	// Family count and types
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_physical_device, &queue_family_count, NULL);
	Vector<VkQueueFamilyProperties> queue_families;
	queue_families.resize(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(p_physical_device, &queue_family_count, queue_families.ptrw());

	for (int i = 0; i < queue_families.size(); i++) {
		if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_physical_device, i, surface, &present_support);

		if (queue_families[i].queueCount > 0 && present_support) {
			indices.present_family = i;
		}

		if (indices.is_complete()) {
			break;
		}
	}
	return indices;
}

void RenderingContextVulkan::_enable_debug() {
	// Debug layer
	if (enable_validation) {
		VkDebugReportCallbackCreateInfoEXT callback_info = {};
		callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callback_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
							  VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
							  VK_DEBUG_REPORT_ERROR_BIT_EXT |
							  VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		callback_info.pfnCallback = _debug_callback;

		if (_create_debug_report_callback_EXT(instance, &callback_info, NULL, &vulkan_debug_callback) != VK_SUCCESS) {
			ERR_EXPLAIN("Can't Set-Up Debug Callback.");
			ERR_FAIL();
		} else {
			print_line("ENABLED DEBUG !");
		}
	}
}

#endif
