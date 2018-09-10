#ifndef RASTERIZER_CANVAS_VULKAN_H
#define RASTERIZER_CANVAS_VULKAN_H

#ifdef _WIN32
#include "platform/windows/rendering_context_vulkan_win.h"
#elif defined(__linux__)
#include "platform/x11/rendering_context_vulkan_x11.h"
#endif

#include "rasterizer_storage_vulkan.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual/rendering_context.h"
#include "shaders/canvas.glsl.gen.h"

class RasterizerCanvasVulkan : public RasterizerCanvas {
private:
	RenderingContext *context;
	Vector<VkDescriptorSet> descriptor_sets;

public:
	RenderingContextVulkan *_get_instance_vulkan();
	Vector<VkDescriptorSet> *_get_descriptor_sets();

	void _update_uniform_buffer(uint32_t current_image);
	void _create_uniform_buffers();
	void _create_buffer_host_cpu_to_gpu(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation);

public:
	RasterizerStorageVulkan *storage;

	struct State {
		CanvasShaderVulkan::CanvasFbos canvas_fbos;

		Vector<VkBuffer> uniform_buffers;
		Vector<VmaAllocation> allocation_uniforms;
		bool canvas_texscreen_used;
		CanvasShaderVulkan canvas_shader;
		//CanvasShadowShaderGLES3 canvas_shadow_shader;

		bool using_texture_rect;
		bool using_ninepatch;

		RID current_tex;
		RID current_normal;
		//RasterizerStorageGLES3::Texture *current_tex_ptr;

		Transform vp;

		Color canvas_item_modulate;
		Transform2D extra_matrix;
		Transform2D final_transform;
		bool using_skeleton;
		Transform2D skeleton_transform;
		Transform2D skeleton_transform_inverse;

	} state;

	RID light_internal_create();
	void light_internal_update(RID p_rid, Light *p_light);
	void light_internal_free(RID p_rid);
	void canvas_begin();
	void canvas_end();
	void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_transform);
	void canvas_debug_viewport_shadows(Light *p_lights_with_shadow);
	void canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache);
	void reset_canvas();
	void draw_window_margins(int *p_margins, RID *p_margin_textures);
	void initialize();
	void draw_generic_textured_rect(Rect2 screenrect, struct Rect2);

	RasterizerCanvasVulkan(RenderingContext *p_context);
	~RasterizerCanvasVulkan();
};
#endif // RASTERIZER_CANVAS_VULKAN_H
