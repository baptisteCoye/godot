#include "rasterizer_canvas_vulkan.h"
#include "glad_vulkan_win.h"
#include "rasterizer_vulkan.h"

RenderingContextVulkan_Win *RasterizerCanvasVulkan::_get_instance_vulkan() {
	return dynamic_cast<RenderingContextVulkan_Win *>(context);
}

RID RasterizerCanvasVulkan::light_internal_create() {
	return RID();
}

void RasterizerCanvasVulkan::light_internal_update(RID p_rid, Light *p_light) {
}

void RasterizerCanvasVulkan::light_internal_free(RID p_rid) {
}

void RasterizerCanvasVulkan::canvas_begin() {
}

void RasterizerCanvasVulkan::canvas_end() {
}

void RasterizerCanvasVulkan::canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_transform) {
}

void RasterizerCanvasVulkan::canvas_debug_viewport_shadows(Light *p_lights_with_shadow) {
}

void RasterizerCanvasVulkan::canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) {
}

void RasterizerCanvasVulkan::reset_canvas() {
}

void RasterizerCanvasVulkan::draw_window_margins(int *black_margin, RID *black_image) {

	Vector2 window_size = OS::get_singleton()->get_window_size();
	int window_h = window_size.height;
	int window_w = window_size.width;

    RasterizerStorageVulkan::RenderTarget *rt = memnew(RasterizerStorageVulkan::RenderTarget);
	storage->render_target_owner.make_rid(rt);
	canvas_begin();

	if (black_image[MARGIN_LEFT].is_valid()) {
		//_bind_canvas_texture(black_image[MARGIN_LEFT], RID());
		Size2 sz(storage->texture_get_width(black_image[MARGIN_LEFT]), storage->texture_get_height(black_image[MARGIN_LEFT]));
		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h), Rect2(0, 0, sz.x, sz.y));
	} else if (black_margin[MARGIN_LEFT]) {
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_RIGHT].is_valid()) {
		//_bind_canvas_texture(black_image[MARGIN_RIGHT], RID());
		Size2 sz(storage->texture_get_width(black_image[MARGIN_RIGHT]), storage->texture_get_height(black_image[MARGIN_RIGHT]));
		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h), Rect2(0, 0, sz.x, sz.y));
	} else if (black_margin[MARGIN_RIGHT]) {
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_TOP].is_valid()) {
		//_bind_canvas_texture(black_image[MARGIN_TOP], RID());

		Size2 sz(storage->texture_get_width(black_image[MARGIN_TOP]), storage->texture_get_height(black_image[MARGIN_TOP]));
		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]), Rect2(0, 0, sz.x, sz.y));

	} else if (black_margin[MARGIN_TOP]) {
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_BOTTOM].is_valid()) {

		//_bind_canvas_texture(black_image[MARGIN_BOTTOM], RID());

		Size2 sz(storage->texture_get_width(black_image[MARGIN_BOTTOM]), storage->texture_get_height(black_image[MARGIN_BOTTOM]));
		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]), Rect2(0, 0, sz.x, sz.y));

	} else if (black_margin[MARGIN_BOTTOM]) {

		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]), Rect2(0, 0, 1, 1));
	}
}

void RasterizerCanvasVulkan::initialize() {
}

RasterizerCanvasVulkan::RasterizerCanvasVulkan(RenderingContext *p_context) {
	context = p_context;
	state.canvas_shader.init();
}

RasterizerCanvasVulkan::~RasterizerCanvasVulkan() {
}

Vector<VkDescriptorSet> *RasterizerCanvasVulkan::_get_descriptor_sets() {
	return &descriptor_sets;
}

void RasterizerCanvasVulkan::draw_generic_textured_rect(Rect2 screenrect, struct Rect2) {
	storage->data.indices.push_back(0);
	storage->data.indices.push_back(1);
	storage->data.indices.push_back(2);
	storage->data.indices.push_back(2);
	storage->data.indices.push_back(3);
	storage->data.indices.push_back(0);

	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(-screenrect.size.width, -screenrect.size.height), Vector3(1.0f, 0.0f, 0.0f), Vector2(1.0f, 0.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(screenrect.size.width, -screenrect.size.height), Vector3(0.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(screenrect.size.width, screenrect.size.height), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 1.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(-screenrect.size.width, screenrect.size.height), Vector3(1.0f, 1.0f, 1.0f), Vector2(1.0f, 1.0f) });

	VkDeviceSize buffer_size = sizeof(storage->data.indices[0]) * storage->data.indices.size();
	VkBuffer staging_buffer;
	VmaAllocationInfo staging_alloc_info;
	VmaAllocation allocation_staging;
	storage->_create_buffer_staging(*_get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer, allocation_staging, staging_alloc_info);

	memcpy(staging_alloc_info.pMappedData, storage->data.indices.ptr(), (size_t)buffer_size);
	storage->_create_buffer(*_get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, storage->data.index_buffer, storage->data.allocation_index);
	storage->_copy_buffer(staging_buffer, storage->data.index_buffer, buffer_size);

	vmaDestroyBuffer(*_get_instance_vulkan()->get_allocator(), staging_buffer, allocation_staging);
}
