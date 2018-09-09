/*************************************************************************/
/*  rendering_context.h                                                  */
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

#ifndef RENDERING_CONTEXT_VULKAN_H
#define RENDERING_CONTEXT_VULKAN_H

//#if defined(OPENGL_ENABLED) || defined(GLES_ENABLED)

#include "core/set.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "servers/visual/rendering_context.h"
#include "typedefs.h"

#ifdef _WIN32
#include "platform/windows/os_windows.h"
#include "platform/windows/glad_vulkan_win.h"
#elif defined(__linux__)
#include "os_x11.h"
#include "platform/x11/glad_vulkan_xlib.h"
#include "thirdparty/vulkan_memory_allocator/vk_mem_alloc.h"
#endif

#include "thirdparty/shaderc/src/libshaderc/include/shaderc/shaderc.h"

class RenderingContextVulkan : public RenderingContext {
private:
	const bool enable_validation = true;
	VkResult _create_debug_report_callback_EXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback);
	void _destroy_debug_report_callback_EXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator);
	static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userData);
	VkDebugReportCallbackEXT vulkan_debug_callback;

private:
	static RenderingContextVulkan *singleton;

	bool use_vsync;
	int glad_vk_version = 0;

	VkInstance instance;
	VkDebugReportCallbackEXT callback;
	VkDevice device;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkQueue graphics_queue;
	VkQueue present_queue;

	VkSwapchainKHR swap_chain;
	Vector<VkImage> swap_chain_images;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	Vector<VkImageView> swap_chain_image_views;
	Vector<VkFramebuffer> swap_chain_framebuffers;

	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;

	VkCommandPool command_pool;
	Vector<VkCommandBuffer> command_buffers;

	Vector<VkSemaphore> image_available_semaphore;
	Vector<VkSemaphore> render_finished_semaphore;
	Vector<VkFence> in_flight_fences;

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		Vector<VkSurfaceFormatKHR> formats;
		Vector<VkPresentModeKHR> present_modes;
	};

protected:
	VkSurfaceKHR surface;

	Set<CharString> device_extensions;
	Set<CharString> extensions;
	Set<CharString> validation_layers;
	RenderingContext *context;

public:
	struct QueueFamilyIndices {
		int graphics_family = -1;
		int present_family = -1;

		bool is_complete() {
			return graphics_family >= 0 && present_family >= 0;
		}
	};
	void _enable_debug();
	bool _check_device_extension_support(VkPhysicalDevice device);
	RenderingContextVulkan::SwapChainSupportDetails _query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR _choose_swap_surface_format(const Vector<VkSurfaceFormatKHR> &available_formats);
	VkPresentModeKHR _choose_swap_present_mode(const Vector<VkPresentModeKHR> available_present_modes);
	VkExtent2D _choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities);
	Error _pick_physical_device();
	Error _create_instance();
	Vector<VkImage> *_get_swap_chain_images();
	VkCommandPool &_get_command_pool();
	VkRenderPass &_get_render_pass();
	Vector<VkFramebuffer> &_get_swap_chain_framebuffers();
	VkPipeline &_get_graphics_pipeline();
	VkExtent2D &_get_swap_chain_extent();
	VkPhysicalDevice &_get_physical_device();
	Vector<VkImageView> &_get_swap_chain_image_views();
	VkPipelineLayout &_get_pipeline_layout();
	VkFormat &_get_swap_chain_image_format();
	Vector<VkFence> *_get_in_flight_fences();
	Error _create_swap_chain();
	Error _create_logical_device();
	int _get_physical_device_score(const VkPhysicalDevice p_physical_device);
	struct RenderingContextVulkan::QueueFamilyIndices _pick_queue_families(VkPhysicalDevice p_physical_device);
	bool _is_swap_chain_adequate(const VkPhysicalDevice p_physical_device);
	Vector<VkCommandBuffer> &_get_command_buffers();
	Vector<VkSemaphore> &_get_image_available_semaphore();
	Vector<VkSemaphore> &_get_render_finished_semaphore();
	VkDevice *_get_device();
	VkInstance *_get_instance();
	VkSwapchainKHR *_get_swap_chain();
	VkQueue *_get_present_queue();
	VkQueue *_get_graphics_queue();

	VmaAllocator allocator;
	VmaAllocator *get_allocator() {
		return &allocator;
	}

	void allocate_vma() {
		VmaAllocatorCreateInfo allocator_info = {};

		VmaVulkanFunctions vma_vulkan_functions;
		vma_vulkan_functions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		vma_vulkan_functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		vma_vulkan_functions.vkAllocateMemory = vkAllocateMemory;
		vma_vulkan_functions.vkFreeMemory = vkFreeMemory;
		vma_vulkan_functions.vkMapMemory = vkMapMemory;
		vma_vulkan_functions.vkUnmapMemory = vkUnmapMemory;
		vma_vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
		vma_vulkan_functions.vkBindImageMemory = vkBindImageMemory;
		vma_vulkan_functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		vma_vulkan_functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		vma_vulkan_functions.vkCreateBuffer = vkCreateBuffer;
		vma_vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
		vma_vulkan_functions.vkCreateImage = vkCreateImage;
		vma_vulkan_functions.vkDestroyImage = vkDestroyImage;
		vma_vulkan_functions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
		vma_vulkan_functions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;

		allocator_info.pVulkanFunctions = &vma_vulkan_functions;
		allocator_info.physicalDevice = _get_physical_device();
		allocator_info.device = *_get_device();

		vmaCreateAllocator(&allocator_info, &allocator);
	}

public:
	static const int max_frames_in_flight = 2;

	virtual void release_current() = 0;

	virtual void make_current() = 0;

	int get_window_width();
	int get_window_height();
	virtual void swap_buffers() = 0;

	virtual Error initialize() = 0;

	virtual void set_use_vsync(bool p_use) = 0;
	virtual bool is_using_vsync() const = 0;

	RenderingContextVulkan();
	~RenderingContextVulkan();
};

#endif
