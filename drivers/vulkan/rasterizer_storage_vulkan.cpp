#include "rasterizer_storage_vulkan.h"


RenderingContextVulkan_Win *RasterizerStorageVulkan::_get_instance_vulkan() {
	return dynamic_cast<RenderingContextVulkan_Win *>(context);
}

VkCommandBuffer RasterizerStorageVulkan::_begin_single_time_commands() {
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = _get_instance_vulkan()->_get_command_pool();
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	if (vkAllocateCommandBuffers(*_get_instance_vulkan()->_get_device(), &alloc_info, &command_buffer) != VK_SUCCESS) {
		CRASH_COND("Can't allocate command buffers!");
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;
}

void RasterizerStorageVulkan::_end_single_time_commands(VkCommandBuffer command_buffer) {
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	if (vkQueueSubmit(*_get_instance_vulkan()->_get_graphics_queue(), 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
		CRASH_COND("Can't Submit Draw Command Buffer.");
	}
	vkQueueWaitIdle(*_get_instance_vulkan()->_get_graphics_queue());

	vkFreeCommandBuffers(*_get_instance_vulkan()->_get_device(), _get_instance_vulkan()->_get_command_pool(), 1, &command_buffer);
}

void RasterizerStorageVulkan::_transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
	VkCommandBuffer command_buffer = _begin_single_time_commands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags source_stage;
	VkPipelineStageFlags destination_stage;

	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		CRASH_COND("Unsupported layout transition.");
	}

	vkCmdPipelineBarrier(
			command_buffer,
			source_stage, destination_stage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

	_end_single_time_commands(command_buffer);
}

void RasterizerStorageVulkan::_create_buffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation) {
	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_create_info = {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateBuffer(allocator, &buffer_info, &alloc_create_info, &buffer, &allocation, NULL) != VK_SUCCESS) {
		CRASH_COND("Can't Create Buffer.");
	}
}

void RasterizerStorageVulkan::_create_image(VmaAllocator allocator, VkImageCreateInfo image_info, VkImage &buffer, VmaAllocation &allocation) {
	VmaAllocationCreateInfo alloc_create_info = {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateImage(allocator, &image_info, &alloc_create_info, &buffer, &allocation, NULL) != VK_SUCCESS) {
		CRASH_COND("Can't Create Buffer.");
	}
}

void RasterizerStorageVulkan::_create_buffer_staging(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation, VmaAllocationInfo &alloc_info) {
	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_create_info = {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (vmaCreateBuffer(allocator, &buffer_info, &alloc_create_info, &buffer, &allocation, &alloc_info) != VK_SUCCESS) {
		CRASH_COND("Can't Create Buffer.");
	}
}

void RasterizerStorageVulkan::_copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer command_buffer = _begin_single_time_commands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	_end_single_time_commands(command_buffer);
}

void RasterizerStorageVulkan::_copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
	VkCommandBuffer command_buffer = _begin_single_time_commands();

	VkBufferCopy copy_region = {};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

	_end_single_time_commands(command_buffer);
}


RID RasterizerStorageVulkan::texture_create() {
	VulkanTexture *texture = memnew(VulkanTexture);
	ERR_FAIL_COND_V(!texture, RID());
	return texture_owner.make_rid(texture);
}

void RasterizerStorageVulkan::texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VisualServer::TextureType p_type, uint32_t p_flags) {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!t);
	t->width = p_width;
	t->height = p_height;
	t->flags = p_flags;
	t->format = p_format;
	t->image = Ref<Image>(memnew(Image));
	t->image->create(p_width, p_height, p_height, p_format);

	VkFormat format;
	_convert_formats(t->image, format, t);

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = static_cast<uint32_t>(p_width);
	image_info.extent.height = static_cast<uint32_t>(p_height);
	// TODO Handle 3d texture case
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0; // Optional

	VmaAllocationCreateInfo image_alloc_info;
	image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	_create_image(*_get_instance_vulkan()->get_allocator(), image_info, t->texture_image, t->texture_image_allocation);
	t->texture_sampler = _create_texture_sampler();
}

VkImageView *RasterizerStorageVulkan::_create_image_view(VkImage image, VkFormat format) {
	VkImageViewCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image = image;

	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;
	create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;
	VkImageView image_view;
	if (vkCreateImageView(*_get_instance_vulkan()->_get_device(), &create_info, NULL, &image_view) != VK_SUCCESS) {
		CRASH_COND("Can't Create Vulkan Texture Image View.");
	}
	return &image_view;
}

VkSampler RasterizerStorageVulkan::_create_texture_sampler() {
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = 16;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;
	VkSampler texture_sampler;
	if (vkCreateSampler(*_get_instance_vulkan()->_get_device(), &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS) {
		CRASH_COND("Can't Create Texture Sampler.");
	}
	return texture_sampler;
}

void RasterizerStorageVulkan::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_level) {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!t);
	t->width = p_image->get_width();
	t->height = p_image->get_height();
	t->format = p_image->get_format();
	t->image->create(t->width, t->height, false, t->format, p_image->get_data());
	t->image->flip_x();
	int size;
	VkFormat format;
	_convert_formats(p_image, format, t);

	bool mips = false;
	if (p_image->get_mipmap_count() > 1) {
		mips = true;
	}
	VkDeviceSize image_size = t->image->get_image_data_size(t->width, t->height, t->format, mips);
	VkBuffer staging_buffer;
	VmaAllocation allocation_staging;
	VmaAllocationInfo staging_alloc_info;
	_create_buffer_staging(*_get_instance_vulkan()->get_allocator(), image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer, allocation_staging, staging_alloc_info);
	PoolByteArray::Read r = t->image->get_data().read();
	memcpy(staging_alloc_info.pMappedData, r.ptr(), (size_t)image_size);

	_transition_image_layout(t->texture_image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	_copy_buffer_to_image(staging_buffer, t->texture_image, static_cast<uint32_t>(t->width), static_cast<uint32_t>(t->height));

	_transition_image_layout(t->texture_image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(*_get_instance_vulkan()->_get_device(), staging_buffer, NULL);
	vmaFreeMemory(*_get_instance_vulkan()->get_allocator(), allocation_staging);

	t->texture_image_view = *_create_image_view(t->texture_image, format);
}

void RasterizerStorageVulkan::_convert_formats(const Ref<Image> &p_image, VkFormat &format, VulkanTexture *t) {
	if (p_image->get_format() == Image::FORMAT_RGBA8) {
		format = VK_FORMAT_R8G8B8A8_UNORM;
	} else if (p_image->get_format() == Image::FORMAT_LA8) {
		format = VK_FORMAT_R8G8_UNORM;
	} else if (p_image->get_format() == Image::FORMAT_L8) {
		format = VK_FORMAT_R8_UNORM;
	} else if (p_image->get_format() == Image::FORMAT_RGB8) {
		t->image->convert(Image::FORMAT_RGBA8);
		t->format = t->image->get_format();
		format = VK_FORMAT_R8G8B8A8_UNORM;
	}
}


void RasterizerStorageVulkan::texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_level) {
	VulkanTexture *t = texture_owner.get(p_texture);

	ERR_FAIL_COND(!t);
	ERR_FAIL_COND(t->format != p_image->get_format());
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(src_w <= 0 || src_h <= 0);
	ERR_FAIL_COND(src_x < 0 || src_y < 0 || src_x + src_w > p_image->get_width() || src_y + src_h > p_image->get_height());
	ERR_FAIL_COND(dst_x < 0 || dst_y < 0 || dst_x + src_w > t->width || dst_y + src_h > t->height);

	t->image->blit_rect(p_image, Rect2(src_x, src_y, src_w, src_h), Vector2(dst_x, dst_y));
}

Ref<Image> RasterizerStorageVulkan::texture_get_data(RID p_texture, int p_level) const {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!t, Ref<Image>());
	return t->image;
}

VS::TextureType RasterizerStorageVulkan::texture_get_type(RID p_texture) const {
	return VS::TEXTURE_TYPE_2D;
}

void RasterizerStorageVulkan::texture_set_flags(RID p_texture, uint32_t p_flags) {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!t);
	t->flags = p_flags;
}

uint32_t RasterizerStorageVulkan::texture_get_flags(RID p_texture) const {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!t, 0);
	return t->flags;
}

Image::Format RasterizerStorageVulkan::texture_get_format(RID p_texture) const {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!t, Image::FORMAT_RGB8);
	return t->format;
}

uint32_t RasterizerStorageVulkan::texture_get_texid(RID p_texture) const {
	return 0;
}

uint32_t RasterizerStorageVulkan::texture_get_width(RID p_texture) const {
	return 0;
}

uint32_t RasterizerStorageVulkan::texture_get_height(RID p_texture) const {
	return 0;
}

uint32_t RasterizerStorageVulkan::texture_get_depth(RID p_texture) const {
	return 0;
}

void RasterizerStorageVulkan::texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d) {
}

void RasterizerStorageVulkan::texture_set_path(RID p_texture, const String &p_path) {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND(!t);
	t->path = p_path;
}

String RasterizerStorageVulkan::texture_get_path(RID p_texture) const {
	VulkanTexture *t = texture_owner.getornull(p_texture);
	ERR_FAIL_COND_V(!t, String());
	return t->path;
}

void RasterizerStorageVulkan::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
}

void RasterizerStorageVulkan::texture_debug_usage(List<VS::TextureInfo> *r_info) {
}

RID RasterizerStorageVulkan::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
	return RID();
}

void RasterizerStorageVulkan::texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
}

void RasterizerStorageVulkan::texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
}

void RasterizerStorageVulkan::texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) {
}

void RasterizerStorageVulkan::textures_keep_original(bool p_enable) {
}

void RasterizerStorageVulkan::texture_set_proxy(RID p_proxy, RID p_base) {
}

void RasterizerStorageVulkan::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
}

RID RasterizerStorageVulkan::sky_create() {
	return RID();
}

void RasterizerStorageVulkan::sky_set_texture(RID p_sky, RID p_cube_map, int p_radiance_size) {
}

RID RasterizerStorageVulkan::shader_create() {
	return RID();
}

void RasterizerStorageVulkan::shader_set_code(RID p_shader, const String &p_code) {
}

String RasterizerStorageVulkan::shader_get_code(RID p_shader) const {
	return "";
}

void RasterizerStorageVulkan::shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
}

void RasterizerStorageVulkan::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) {
}

RID RasterizerStorageVulkan::shader_get_default_texture_param(RID p_shader, const StringName &p_name) const {
	return RID();
}

RID RasterizerStorageVulkan::material_create() {
	return RID();
}

void RasterizerStorageVulkan::material_set_render_priority(RID p_material, int priority) {
}

void RasterizerStorageVulkan::material_set_shader(RID p_shader_material, RID p_shader) {
}

RID RasterizerStorageVulkan::material_get_shader(RID p_shader_material) const {
	return RID();
}

void RasterizerStorageVulkan::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
}

Variant RasterizerStorageVulkan::material_get_param(RID p_material, const StringName &p_param) const {
	return Variant();
}

void RasterizerStorageVulkan::material_set_line_width(RID p_material, float p_width) {
}

void RasterizerStorageVulkan::material_set_next_pass(RID p_material, RID p_next_material) {
}

bool RasterizerStorageVulkan::material_is_animated(RID p_material) {
	return false;
}

bool RasterizerStorageVulkan::material_casts_shadows(RID p_material) {
	return false;
}

void RasterizerStorageVulkan::material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
}

void RasterizerStorageVulkan::material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
}

RID RasterizerStorageVulkan::mesh_create() {
	VulkanMesh *mesh = memnew(VulkanMesh);
	ERR_FAIL_COND_V(!mesh, RID());
	mesh->blend_shape_count = 0;
	mesh->blend_shape_mode = VS::BLEND_SHAPE_MODE_NORMALIZED;
	return mesh_owner.make_rid(mesh);
}

void RasterizerStorageVulkan::mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t> > &p_blend_shapes, const Vector<AABB> &p_bone_aabbs) {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!m);

	m->surfaces.push_back(VulkanSurface());
	VulkanSurface *s = &m->surfaces.write[m->surfaces.size() - 1];
	s->format = p_format;
	s->primitive = p_primitive;
	s->array = p_array;
	s->vertex_count = p_vertex_count;
	s->index_array = p_index_array;
	s->index_count = p_index_count;
	s->aabb = p_aabb;
	s->blend_shapes = p_blend_shapes;
	s->bone_aabbs = p_bone_aabbs;
}

void RasterizerStorageVulkan::mesh_set_blend_shape_count(RID p_mesh, int p_amount) {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!m);
	m->blend_shape_count = p_amount;
}

int RasterizerStorageVulkan::mesh_get_blend_shape_count(RID p_mesh) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, 0);
	return m->blend_shape_count;
}

void RasterizerStorageVulkan::mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!m);
	m->blend_shape_mode = p_mode;
}

VS::BlendShapeMode RasterizerStorageVulkan::mesh_get_blend_shape_mode(RID p_mesh) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, VS::BLEND_SHAPE_MODE_NORMALIZED);
	return m->blend_shape_mode;
}

void RasterizerStorageVulkan::mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {
}

void RasterizerStorageVulkan::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
}

RID RasterizerStorageVulkan::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	return RID();
}

int RasterizerStorageVulkan::mesh_surface_get_array_len(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, 0);

	return m->surfaces[p_surface].vertex_count;
}

int RasterizerStorageVulkan::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, 0);

	return m->surfaces[p_surface].index_count;
}

PoolVector<uint8_t> RasterizerStorageVulkan::mesh_surface_get_array(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, PoolVector<uint8_t>());

	return m->surfaces[p_surface].array;
}

PoolVector<uint8_t> RasterizerStorageVulkan::mesh_surface_get_index_array(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, PoolVector<uint8_t>());

	return m->surfaces[p_surface].index_array;
}

uint32_t RasterizerStorageVulkan::mesh_surface_get_format(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, 0);

	return m->surfaces[p_surface].format;
}

VS::PrimitiveType RasterizerStorageVulkan::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, VS::PRIMITIVE_POINTS);

	return m->surfaces[p_surface].primitive;
}

AABB RasterizerStorageVulkan::mesh_surface_get_aabb(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, AABB());

	return m->surfaces[p_surface].aabb;
}

Vector<PoolVector<uint8_t> > RasterizerStorageVulkan::mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, Vector<PoolVector<uint8_t> >());

	return m->surfaces[p_surface].blend_shapes;
}

Vector<AABB> RasterizerStorageVulkan::mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, Vector<AABB>());

	return m->surfaces[p_surface].bone_aabbs;
}

void RasterizerStorageVulkan::mesh_remove_surface(RID p_mesh, int p_index) {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND(!m);
	ERR_FAIL_COND(p_index >= m->surfaces.size());

	m->surfaces.remove(p_index);
}

int RasterizerStorageVulkan::mesh_get_surface_count(RID p_mesh) const {
	VulkanMesh *m = mesh_owner.getornull(p_mesh);
	ERR_FAIL_COND_V(!m, 0);
	return m->surfaces.size();
}

void RasterizerStorageVulkan::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
}

AABB RasterizerStorageVulkan::mesh_get_custom_aabb(RID p_mesh) const {
	return AABB();
}

AABB RasterizerStorageVulkan::mesh_get_aabb(RID p_mesh, RID p_skeleton) const {
	return AABB();
}

void RasterizerStorageVulkan::mesh_clear(RID p_mesh) {
}

RID RasterizerStorageVulkan::multimesh_create() {
	return RID();
}

void RasterizerStorageVulkan::multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data) {
}

int RasterizerStorageVulkan::multimesh_get_instance_count(RID p_multimesh) const {
	return 0;
}

void RasterizerStorageVulkan::multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
}

void RasterizerStorageVulkan::multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) {
}

void RasterizerStorageVulkan::multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
}

void RasterizerStorageVulkan::multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
}

void RasterizerStorageVulkan::multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
}

RID RasterizerStorageVulkan::multimesh_get_mesh(RID p_multimesh) const {
	return RID();
}

Transform RasterizerStorageVulkan::multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	return Transform();
}

Transform2D RasterizerStorageVulkan::multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	return Transform2D();
}

Color RasterizerStorageVulkan::multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	return Color();
}

Color RasterizerStorageVulkan::multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	return Color();
}

void RasterizerStorageVulkan::multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) {
}

void RasterizerStorageVulkan::multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
}

int RasterizerStorageVulkan::multimesh_get_visible_instances(RID p_multimesh) const {
	return 0;
}

AABB RasterizerStorageVulkan::multimesh_get_aabb(RID p_multimesh) const {
	return AABB();
}

RID RasterizerStorageVulkan::immediate_create() {
	return RID();
}

void RasterizerStorageVulkan::immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture) {
}

void RasterizerStorageVulkan::immediate_vertex(RID p_immediate, const Vector3 &p_vertex) {
}

void RasterizerStorageVulkan::immediate_normal(RID p_immediate, const Vector3 &p_normal) {
}

void RasterizerStorageVulkan::immediate_tangent(RID p_immediate, const Plane &p_tangent) {
}

void RasterizerStorageVulkan::immediate_color(RID p_immediate, const Color &p_color) {
}

void RasterizerStorageVulkan::immediate_uv(RID p_immediate, const Vector2 &tex_uv) {
}

void RasterizerStorageVulkan::immediate_uv2(RID p_immediate, const Vector2 &tex_uv) {
}

void RasterizerStorageVulkan::immediate_end(RID p_immediate) {
}

void RasterizerStorageVulkan::immediate_clear(RID p_immediate) {
}

void RasterizerStorageVulkan::immediate_set_material(RID p_immediate, RID p_material) {
}

RID RasterizerStorageVulkan::immediate_get_material(RID p_immediate) const {
	return RID();
}

AABB RasterizerStorageVulkan::immediate_get_aabb(RID p_immediate) const {
	return AABB();
}

RID RasterizerStorageVulkan::skeleton_create() {
	return RID();
}

void RasterizerStorageVulkan::skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
}

void RasterizerStorageVulkan::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
}

int RasterizerStorageVulkan::skeleton_get_bone_count(RID p_skeleton) const {
	return 0;
}

void RasterizerStorageVulkan::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) {
}

Transform RasterizerStorageVulkan::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	return Transform();
}

void RasterizerStorageVulkan::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
}

Transform2D RasterizerStorageVulkan::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	return Transform2D();
}

RID RasterizerStorageVulkan::light_create(VS::LightType p_type) {
	return RID();
}

RID RasterizerStorageVulkan::directional_light_create() {
	return light_create(VS::LIGHT_DIRECTIONAL);
}

RID RasterizerStorageVulkan::omni_light_create() {
	return light_create(VS::LIGHT_OMNI);
}

RID RasterizerStorageVulkan::spot_light_create() {
	return light_create(VS::LIGHT_SPOT);
}

void RasterizerStorageVulkan::light_set_color(RID p_light, const Color &p_color) {
}

void RasterizerStorageVulkan::light_set_param(RID p_light, VS::LightParam p_param, float p_value) {
}

void RasterizerStorageVulkan::light_set_shadow(RID p_light, bool p_enabled) {
}

void RasterizerStorageVulkan::light_set_shadow_color(RID p_light, const Color &p_color) {
}

void RasterizerStorageVulkan::light_set_projector(RID p_light, RID p_texture) {
}

void RasterizerStorageVulkan::light_set_negative(RID p_light, bool p_enable) {
}

void RasterizerStorageVulkan::light_set_cull_mask(RID p_light, uint32_t p_mask) {
}

void RasterizerStorageVulkan::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
}

void RasterizerStorageVulkan::light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) {
}

void RasterizerStorageVulkan::light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) {
}

void RasterizerStorageVulkan::light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) {
}

void RasterizerStorageVulkan::light_directional_set_blend_splits(RID p_light, bool p_enable) {
}

bool RasterizerStorageVulkan::light_directional_get_blend_splits(RID p_light) const {
	return false;
}

void RasterizerStorageVulkan::light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) {
}

VS::LightDirectionalShadowDepthRangeMode RasterizerStorageVulkan::light_directional_get_shadow_depth_range_mode(RID p_light) const {
	return VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE;
}

VS::LightDirectionalShadowMode RasterizerStorageVulkan::light_directional_get_shadow_mode(RID p_light) {
	return VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
}

VS::LightOmniShadowMode RasterizerStorageVulkan::light_omni_get_shadow_mode(RID p_light) {
	return VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
}

bool RasterizerStorageVulkan::light_has_shadow(RID p_light) const {
	return false;
}

VS::LightType RasterizerStorageVulkan::light_get_type(RID p_light) const {
	return VS::LIGHT_OMNI;
}

AABB RasterizerStorageVulkan::light_get_aabb(RID p_light) const {
	return AABB();
}

float RasterizerStorageVulkan::light_get_param(RID p_light, VS::LightParam p_param) {
	return 0.0;
}

Color RasterizerStorageVulkan::light_get_color(RID p_light) {
	return Color();
}

uint64_t RasterizerStorageVulkan::light_get_version(RID p_light) const {
	return 0;
}

RID RasterizerStorageVulkan::reflection_probe_create() {
	return RID();
}

void RasterizerStorageVulkan::reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) {
}

void RasterizerStorageVulkan::reflection_probe_set_intensity(RID p_probe, float p_intensity) {
}

void RasterizerStorageVulkan::reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) {
}

void RasterizerStorageVulkan::reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) {
}

void RasterizerStorageVulkan::reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) {
}

void RasterizerStorageVulkan::reflection_probe_set_max_distance(RID p_probe, float p_distance) {
}

void RasterizerStorageVulkan::reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) {
}

void RasterizerStorageVulkan::reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) {
}

void RasterizerStorageVulkan::reflection_probe_set_as_interior(RID p_probe, bool p_enable) {
}

void RasterizerStorageVulkan::reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) {
}

void RasterizerStorageVulkan::reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) {
}

void RasterizerStorageVulkan::reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) {
}

AABB RasterizerStorageVulkan::reflection_probe_get_aabb(RID p_probe) const {
	return AABB();
}

VS::ReflectionProbeUpdateMode RasterizerStorageVulkan::reflection_probe_get_update_mode(RID p_probe) const {
	return VisualServer::REFLECTION_PROBE_UPDATE_ONCE;
}

uint32_t RasterizerStorageVulkan::reflection_probe_get_cull_mask(RID p_probe) const {
	return 0;
}

Vector3 RasterizerStorageVulkan::reflection_probe_get_extents(RID p_probe) const {
	return Vector3();
}

Vector3 RasterizerStorageVulkan::reflection_probe_get_origin_offset(RID p_probe) const {
	return Vector3();
}

float RasterizerStorageVulkan::reflection_probe_get_origin_max_distance(RID p_probe) const {
	return 0.0;
}

bool RasterizerStorageVulkan::reflection_probe_renders_shadows(RID p_probe) const {
	return false;
}

void RasterizerStorageVulkan::instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
}

void RasterizerStorageVulkan::instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
}

void RasterizerStorageVulkan::instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
}

void RasterizerStorageVulkan::instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
}

RID RasterizerStorageVulkan::gi_probe_create() {
	return RID();
}

void RasterizerStorageVulkan::gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) {
}

AABB RasterizerStorageVulkan::gi_probe_get_bounds(RID p_probe) const {
	return AABB();
}

void RasterizerStorageVulkan::gi_probe_set_cell_size(RID p_probe, float p_range) {
}

float RasterizerStorageVulkan::gi_probe_get_cell_size(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageVulkan::gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) {
}

Transform RasterizerStorageVulkan::gi_probe_get_to_cell_xform(RID p_probe) const {
	return Transform();
}

void RasterizerStorageVulkan::gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) {
}

PoolVector<int> RasterizerStorageVulkan::gi_probe_get_dynamic_data(RID p_probe) const {
	PoolVector<int> p;
	return p;
}

void RasterizerStorageVulkan::gi_probe_set_dynamic_range(RID p_probe, int p_range) {
}

int RasterizerStorageVulkan::gi_probe_get_dynamic_range(RID p_probe) const {
	return 0;
}

void RasterizerStorageVulkan::gi_probe_set_energy(RID p_probe, float p_range) {
}

float RasterizerStorageVulkan::gi_probe_get_energy(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageVulkan::gi_probe_set_bias(RID p_probe, float p_range) {
}

float RasterizerStorageVulkan::gi_probe_get_bias(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageVulkan::gi_probe_set_normal_bias(RID p_probe, float p_range) {
}

float RasterizerStorageVulkan::gi_probe_get_normal_bias(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageVulkan::gi_probe_set_propagation(RID p_probe, float p_range) {
}

float RasterizerStorageVulkan::gi_probe_get_propagation(RID p_probe) const {
	return 0.0;
}

void RasterizerStorageVulkan::gi_probe_set_interior(RID p_probe, bool p_enable) {
}

bool RasterizerStorageVulkan::gi_probe_is_interior(RID p_probe) const {
	return false;
}

void RasterizerStorageVulkan::gi_probe_set_compress(RID p_probe, bool p_enable) {
}

bool RasterizerStorageVulkan::gi_probe_is_compressed(RID p_probe) const {
	return false;
}

uint32_t RasterizerStorageVulkan::gi_probe_get_version(RID p_probe) {
	return 0;
}

RasterizerStorage::GIProbeCompression RasterizerStorageVulkan::gi_probe_get_dynamic_data_get_preferred_compression() const {
	return GI_PROBE_UNCOMPRESSED;
}

RID RasterizerStorageVulkan::gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) {
	return RID();
}

void RasterizerStorageVulkan::gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) {
}

void RasterizerStorageVulkan::lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) {
}

AABB RasterizerStorageVulkan::lightmap_capture_get_bounds(RID p_capture) const {
	return AABB();
}

void RasterizerStorageVulkan::lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) {
}

RID RasterizerStorageVulkan::lightmap_capture_create() {
	LightmapCapture *capture = memnew(LightmapCapture);
	return lightmap_capture_data_owner.make_rid(capture);
}

PoolVector<uint8_t> RasterizerStorageVulkan::lightmap_capture_get_octree(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, PoolVector<uint8_t>());
	return PoolVector<uint8_t>();
}

void RasterizerStorageVulkan::lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) {
}

Transform RasterizerStorageVulkan::lightmap_capture_get_octree_cell_transform(RID p_capture) const {
	return Transform();
}

void RasterizerStorageVulkan::lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) {
}

int RasterizerStorageVulkan::lightmap_capture_get_octree_cell_subdiv(RID p_capture) const {
	return 0;
}

void RasterizerStorageVulkan::lightmap_capture_set_energy(RID p_capture, float p_energy) {
}

float RasterizerStorageVulkan::lightmap_capture_get_energy(RID p_capture) const {
	return 0.0;
}

const PoolVector<RasterizerStorage::LightmapCaptureOctree> *RasterizerStorageVulkan::lightmap_capture_get_octree_ptr(RID p_capture) const {
	const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
	ERR_FAIL_COND_V(!capture, NULL);
	return &capture->octree;
}

RID RasterizerStorageVulkan::particles_create() {
	return RID();
}

void RasterizerStorageVulkan::particles_set_emitting(RID p_particles, bool p_emitting) {
}

void RasterizerStorageVulkan::particles_set_amount(RID p_particles, int p_amount) {
}

void RasterizerStorageVulkan::particles_set_lifetime(RID p_particles, float p_lifetime) {
}

void RasterizerStorageVulkan::particles_set_one_shot(RID p_particles, bool p_one_shot) {
}

void RasterizerStorageVulkan::particles_set_pre_process_time(RID p_particles, float p_time) {
}

void RasterizerStorageVulkan::particles_set_explosiveness_ratio(RID p_particles, float p_ratio) {
}

void RasterizerStorageVulkan::particles_set_randomness_ratio(RID p_particles, float p_ratio) {
}

void RasterizerStorageVulkan::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
}

void RasterizerStorageVulkan::particles_set_speed_scale(RID p_particles, float p_scale) {
}

void RasterizerStorageVulkan::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
}

void RasterizerStorageVulkan::particles_set_process_material(RID p_particles, RID p_material) {
}

void RasterizerStorageVulkan::particles_set_fixed_fps(RID p_particles, int p_fps) {
}

void RasterizerStorageVulkan::particles_set_fractional_delta(RID p_particles, bool p_enable) {
}

void RasterizerStorageVulkan::particles_restart(RID p_particles) {
}

void RasterizerStorageVulkan::particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) {
}

void RasterizerStorageVulkan::particles_set_draw_passes(RID p_particles, int p_count) {
}

void RasterizerStorageVulkan::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
}

void RasterizerStorageVulkan::particles_request_process(RID p_particles) {
}

AABB RasterizerStorageVulkan::particles_get_current_aabb(RID p_particles) {
	return AABB();
}

AABB RasterizerStorageVulkan::particles_get_aabb(RID p_particles) const {
	return AABB();
}

void RasterizerStorageVulkan::particles_set_emission_transform(RID p_particles, const Transform &p_transform) {
}

bool RasterizerStorageVulkan::particles_get_emitting(RID p_particles) {
	return false;
}

int RasterizerStorageVulkan::particles_get_draw_passes(RID p_particles) const {
	return 0;
}

RID RasterizerStorageVulkan::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	return RID();
}

RID RasterizerStorageVulkan::render_target_create() {
	RenderTarget *rt = memnew(RenderTarget);

	VulkanTexture *t = memnew(VulkanTexture);

	t->flags = 0;
	t->width = 0;
	t->height = 0;
	//t->alloc_height = 0;
	//t->alloc_width = 0;
	t->format = Image::FORMAT_R8;
	//t->target = GL_TEXTURE_2D;
	//t->gl_format_cache = 0;
	//t->gl_internal_format_cache = 0;
	//t->gl_type_cache = 0;
	//t->data_size = 0;
	//t->compressed = false;
	//t->srgb = false;
	//t->total_data_size = 0;
	//t->ignore_mipmaps = false;
	//t->mipmaps = 1;
	//t->active = true;
	//t->tex_id = 0;
	//t->render_target = rt;

	rt->texture = texture_owner.make_rid(t);

	return render_target_owner.make_rid(rt);
}

void RasterizerStorageVulkan::render_target_set_size(RID p_render_target, int p_width, int p_height) {
}

RID RasterizerStorageVulkan::render_target_get_texture(RID p_render_target) const {
	return RID();
}

void RasterizerStorageVulkan::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
}

bool RasterizerStorageVulkan::render_target_was_used(RID p_render_target) {
	return false;
}

void RasterizerStorageVulkan::render_target_clear_used(RID p_render_target) {
}

void RasterizerStorageVulkan::render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) {
}

RID RasterizerStorageVulkan::canvas_light_shadow_buffer_create(int p_width) {
	return RID();
}

RID RasterizerStorageVulkan::canvas_light_occluder_create() {
	return RID();
}

void RasterizerStorageVulkan::canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines) {
}

VS::InstanceType RasterizerStorageVulkan::get_base_type(RID p_rid) const {
	if (mesh_owner.owns(p_rid)) {
		return VS::INSTANCE_MESH;
	}

	return VS::INSTANCE_NONE;
}

bool RasterizerStorageVulkan::free(RID p_rid) {
	if (texture_owner.owns(p_rid)) {
		// delete the texture
		VulkanTexture *texture = texture_owner.get(p_rid);
		//TODO implement more of the api
		//vkDestroySampler(*get_instance_vulkan()->get_device(), texture->texture_sampler, nullptr);
		//vkDestroyImageView(*get_instance_vulkan()->get_device(), texture->texture_image_view, nullptr);
		//vkDestroyImage(*get_instance_vulkan()->get_device(), texture->texture_image, nullptr);
		//vmaFreeMemory(*get_instance_vulkan()->get_allocator(), texture->texture_image_allocation);
		texture_owner.free(p_rid);
		memdelete(texture);
	}
	return true;
}

bool RasterizerStorageVulkan::has_os_feature(const String &p_feature) const {
	return false;
}

void RasterizerStorageVulkan::update_dirty_resources() {
}

void RasterizerStorageVulkan::set_debug_generate_wireframes(bool p_generate) {
}

void RasterizerStorageVulkan::render_info_begin_capture() {
}

void RasterizerStorageVulkan::render_info_end_capture() {
}

int RasterizerStorageVulkan::get_captured_render_info(VS::RenderInfo p_info) {
	return 0;
}

int RasterizerStorageVulkan::get_render_info(VS::RenderInfo p_info) {
	return 0;
}

RasterizerStorageVulkan::RasterizerStorageVulkan(RenderingContext *p_context) {
	context = p_context;
}

RasterizerStorageVulkan::~RasterizerStorageVulkan() {
}

RasterizerStorageVulkan::LightmapCapture::LightmapCapture() {
	energy = 1.0;
	cell_subdiv = 1;
}


_FORCE_INLINE_ void RasterizerStorageVulkan::Instantiable::instance_change_notify() {
	SelfList<RasterizerScene::InstanceBase> *instances = instance_list.first();
	while (instances) {

		instances->self()->base_changed();
		instances = instances->next();
	}
}

_FORCE_INLINE_ void RasterizerStorageVulkan::Instantiable::instance_material_change_notify() {
	SelfList<RasterizerScene::InstanceBase> *instances = instance_list.first();
	while (instances) {

		instances->self()->base_material_changed();
		instances = instances->next();
	}
}

_FORCE_INLINE_ void RasterizerStorageVulkan::Instantiable::instance_remove_deps() {
	SelfList<RasterizerScene::InstanceBase> *instances = instance_list.first();
	while (instances) {

		SelfList<RasterizerScene::InstanceBase> *next = instances->next();
		instances->self()->base_removed();
		instances = next;
	}
}

RasterizerStorageVulkan::Instantiable::Instantiable() {
}

RasterizerStorageVulkan::Instantiable::~Instantiable() {
}

RasterizerStorageVulkan::RenderTarget::RenderTarget() {
	//msaa = VS::VIEWPORT_MSAA_DISABLED;
	width = 0;
	height = 0;
	depth = 0;
	fbo = 0;
	//exposure.fbo = 0;
	//buffers.fbo = 0;
	//used_in_frame = false;

	//for (int i = 0; i < RENDER_TARGET_FLAG_MAX; i++) {
	//	flags[i] = false;
	//}
	//flags[RENDER_TARGET_HDR] = true;

	//buffers.active = false;
	//buffers.effects_active = false;

	//last_exposure_tick = 0;
}
