#ifndef RASTERIZER_CANVAS_VULKAN_H
#define RASTERIZER_CANVAS_VULKAN_H

#include "platform/windows/rendering_context_vulkan_win.h"
#include "rasterizer_storage_vulkan.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual/rendering_context.h"
#include "shaders/canvas.glsl.gen.h"

class RasterizerCanvasVulkan : public RasterizerCanvas {
private:
	RenderingContext *context;
	Vector<VkDescriptorSet> descriptor_sets;

public:
	RenderingContextVulkan_Win *_get_instance_vulkan();
	Vector<VkDescriptorSet> *_get_descriptor_sets();

	void _update_uniform_buffer(uint32_t current_image) {
		//state.ubo_data.model_matrix.set_identity();
		//state.ubo_data.view_matrix = look_at(Vector3(2.f, 2.f, 2.f), Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 1.f));
		//state.ubo_data.projection_matrix.set_perspective(
		//		45.f,
		//		get_instance_vulkan()->get_swap_chain_extent().width / (float)get_instance_vulkan()->get_swap_chain_extent().height,
		//		0.1f,
		//		10.f,
		//		false);
		CameraMatrix identity;
		identity.set_identity();
		for (size_t i = 0; i < 4; i++) {
			for (size_t j = 0; j < 4; j++) {
				state.canvas_item_ubo_data.projection_matrix[i * 4 + j] = identity.matrix[i][j];
			}
		}

		for (size_t i = 0; i < 4; i++) {
			for (size_t j = 0; j < 4; j++) {
				state.canvas_item_ubo_data.modelview_matrix[i * 4 + j] = identity.matrix[i][j];
			}
		}

		CameraMatrix correction;
		correction.set_identity();
		correction.matrix[1][1] = -1;
		correction.matrix[2][2] = 1.f / 2.f;
		correction.matrix[2][3] = 1.f / 2.f;
		for (size_t i = 0; i < 4; i++) {
			for (size_t j = 0; j < 4; j++) {
				state.canvas_item_ubo_data.projection_matrix[i * 4 + j] = correction.matrix[i][j];
			}
		}

		void *mapped_data;
		vmaMapMemory(*_get_instance_vulkan()->get_allocator(),
				state.allocation_uniforms[current_image], &mapped_data);
		memcpy(mapped_data, &state.canvas_item_ubo_data, sizeof(state.canvas_item_ubo_data));
		vmaUnmapMemory(*_get_instance_vulkan()->get_allocator(), state.allocation_uniforms[current_image]);
	}

	void _create_uniform_buffers() {
		VkDeviceSize buffer_size = sizeof(state.canvas_item_ubo_data);
		state.uniform_buffers.resize(_get_instance_vulkan()->_get_swap_chain_images()->size());
		state.allocation_uniforms.resize(_get_instance_vulkan()->_get_swap_chain_images()->size());

		for (size_t i = 0; i < _get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
			_create_buffer_host_cpu_to_gpu(*_get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, state.uniform_buffers.write[i], state.allocation_uniforms.write[i]);
		}
	}

	void _create_buffer_host_cpu_to_gpu(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation) {
		VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_create_info = {};
		alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_create_info, &buffer, &allocation, NULL) != VK_SUCCESS) {
			CRASH_COND("Can't Create Buffer.");
		}
	}

public:
	RasterizerStorageVulkan *storage;

	struct State {
		struct CanvasItemUBO {
			float projection_matrix[16];
			float time;
			uint8_t padding[12];
			float modelview_matrix[16];
			float extra_matrix[16];
		} canvas_item_ubo_data;

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
