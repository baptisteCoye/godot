#ifndef RASTERIZER_VULKAN_H
#define RASTERIZER_VULKAN_H

#include "core/project_settings.h"
#include "os/os.h"

#ifdef _WIN32
#include "platform/windows/rendering_context_vulkan_win.h"
#include "platform/windows/vma_windows.h"
#elif defined(__linux__)
#include "platform/x11/rendering_context_vulkan_x11.h"
#include "platform/x11/vma_windows.h"
#endif

#include "servers/visual/rasterizer.h"
#include "servers/visual/rendering_context.h"

#include "rasterizer_canvas_vulkan.h"
#include "rasterizer_scene_vulkan.h"
#include "rasterizer_storage_vulkan.h"

#include "main/splash.gen.h"


class MakeCurrentFunctVulkan : public MakeCurrentFunct {
	RenderingContext *context;

public:
	virtual Rasterizer *make_current();

	MakeCurrentFunctVulkan(RenderingContext *p_context);

	~MakeCurrentFunctVulkan();
};

class RasterizerVulkan : public Rasterizer {
private:
	void _cleanup();
	void _cleanup_swap_chain();
	void _recreate_swap_chain();
	void _create_image_views();
	void _create_descriptor_set_layout();
	void _update_descriptors();
	void _create_graphics_pipeline();
	void _create_framebuffers();
	void _create_command_pool();
	void _primary_buffer_end(size_t i);
	VkRenderPassBeginInfo &_create_render_pass_info(size_t i);
	void _create_primary_command_buffers();
	void _create_sync_objects();
	void _create_index_buffer(Vector<uint16_t> p_indices, VkBuffer &p_index_buffer);
	void _create_vertex_buffer(Vector<RasterizerStorageVulkan::Vertex> p_vertices, VkBuffer &p_vertex_buffer);
	void _create_descriptor_pool();
	void _create_descriptor_sets();
	void _create_render_pass_end();
	void _create_render_pass_begin();
	void _render_pass_begin();

private:
	VmaAllocation allocation;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;
	double time_total;
	bool framebuffer_resized = false;

protected:
	RasterizerCanvasVulkan *canvas;
	RasterizerStorageVulkan *storage;
	RasterizerSceneVulkan *scene;
	RenderingContext *context;

public:
	RenderingContextVulkan *get_instance_vulkan();
	RasterizerStorageVulkan *get_storage();
	RasterizerCanvasVulkan *get_canvas();
	RasterizerSceneVulkan *get_scene();

	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale);
	void initialize();
	void begin_frame(double frame_step);
	void set_current_render_target(RID p_render_target);
	void restore_render_target();
	void clear_render_target(const Color &p_color);
	void blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen = 0);
	void end_frame(bool p_swap_buffers);
	void finalize();
	static void make_current(RenderingContext *context);
	static void register_config();
	RasterizerVulkan(RenderingContext *p_context);
	~RasterizerVulkan();
};

#endif // RASTERIZER_VULKAN_H
