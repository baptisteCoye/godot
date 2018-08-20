#include "drivers/vulkan/rasterizer_vulkan.h"

#include "platform/windows/os_windows.h"
#include "platform/windows/rendering_context_vulkan_win.h"
#include "platform/windows/vma_usage.h"
#include "shaders/canvas.glsl.gen.h"

Rasterizer *MakeCurrentFunctVulkan::make_current() {
	return memnew(RasterizerVulkan(context));
}

MakeCurrentFunctVulkan::MakeCurrentFunctVulkan(RenderingContext *p_context) {
	context = p_context;
}

MakeCurrentFunctVulkan::~MakeCurrentFunctVulkan() {
}

void RasterizerVulkan::_create_index_buffer(Vector<uint16_t> p_indices, VkBuffer &p_index_buffer) {
	VkDeviceSize buffer_size = sizeof(p_indices[0]) * p_indices.size();

	VkBuffer staging_buffer;
	VmaAllocationInfo staging_alloc_info;
	VmaAllocation allocation_staging;
	storage->_create_buffer_staging(*get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer, allocation_staging, staging_alloc_info);

	memcpy(staging_alloc_info.pMappedData, p_indices.ptr(), (size_t)buffer_size);
	storage->_create_buffer(*get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, p_index_buffer, storage->data.allocation_index);
	storage->_copy_buffer(staging_buffer, p_index_buffer, buffer_size);

	vmaDestroyBuffer(*get_instance_vulkan()->get_allocator(), staging_buffer, allocation_staging);
}

void RasterizerVulkan::_create_vertex_buffer(Vector<RasterizerStorageVulkan::Vertex> p_vertices, VkBuffer &vertex_buffer) {
	VkDeviceSize buffer_size = sizeof(p_vertices[0]) * p_vertices.size();
	VkBuffer staging_buffer;
	VmaAllocation allocation_staging;
	VmaAllocationInfo staging_alloc_info;
	storage->_create_buffer_staging(*get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer, allocation_staging, staging_alloc_info);

	memcpy(staging_alloc_info.pMappedData, p_vertices.ptr(), (size_t)buffer_size);

	storage->_create_buffer(*get_instance_vulkan()->get_allocator(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer, storage->data.allocation_vertex);

	storage->_copy_buffer(staging_buffer, vertex_buffer, buffer_size);

	vmaDestroyBuffer(*get_instance_vulkan()->get_allocator(), staging_buffer, allocation_staging);
}

void RasterizerVulkan::_create_descriptor_pool() {
	Vector<VkDescriptorPoolSize> pool_sizes;
	VkDescriptorPoolSize uniform_pool;
	uniform_pool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_pool.descriptorCount = static_cast<uint32_t>(get_instance_vulkan()->_get_swap_chain_images()->size());
	pool_sizes.push_back(uniform_pool);
	VkDescriptorPoolSize sampler_pool;
	sampler_pool.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_pool.descriptorCount = static_cast<uint32_t>(get_instance_vulkan()->_get_swap_chain_images()->size());
	pool_sizes.push_back(sampler_pool);

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.ptr();
	pool_info.maxSets = static_cast<uint32_t>(get_instance_vulkan()->_get_swap_chain_images()->size());

	if (vkCreateDescriptorPool(*get_instance_vulkan()->_get_device(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		CRASH_COND("Can't Create Descriptor Pool.");
	}
}

void RasterizerVulkan::_create_descriptor_sets() {
	Vector<VkDescriptorSetLayout> layouts;
	layouts.resize(get_instance_vulkan()->_get_swap_chain_images()->size());
	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
		layouts.write[i] = descriptor_set_layout;
	}
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(get_instance_vulkan()->_get_swap_chain_images()->size());
	alloc_info.pSetLayouts = layouts.ptr();

	canvas->_get_descriptor_sets()->resize(get_instance_vulkan()->_get_swap_chain_images()->size());
	if (vkAllocateDescriptorSets(*get_instance_vulkan()->_get_device(), &alloc_info, &canvas->_get_descriptor_sets()->write[0]) != VK_SUCCESS) {
		CRASH_COND("Can't Allocate Descriptor Sets.");
	}

	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = get_scene()->uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet buffer_descriptor = {};
		buffer_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		buffer_descriptor.dstSet = (*canvas->_get_descriptor_sets())[i];
		buffer_descriptor.dstBinding = 0;
		buffer_descriptor.dstArrayElement = 0;
		buffer_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		buffer_descriptor.descriptorCount = 1;
		buffer_descriptor.pBufferInfo = &buffer_info;

		Vector<VkWriteDescriptorSet> descriptor_writes;
		descriptor_writes.push_back(buffer_descriptor);

		vkUpdateDescriptorSets(*get_instance_vulkan()->_get_device(), descriptor_writes.size(), descriptor_writes.ptr(), 0, nullptr);
	}

	update_descriptors();
}

void RasterizerVulkan::_cleanup_swap_chain() {
	for (size_t i = 10; i < get_instance_vulkan()->_get_swap_chain_framebuffers().size(); i++) {
		vkDestroyFramebuffer(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_swap_chain_framebuffers()[i], nullptr);
	}

	vkFreeCommandBuffers(*get_instance_vulkan()->_get_device(),
			get_instance_vulkan()->_get_command_pool(), static_cast<uint32_t>(get_instance_vulkan()->_get_command_buffers().size()),
			get_instance_vulkan()->_get_command_buffers().ptr());

	vkDestroyPipeline(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_graphics_pipeline(), nullptr);
	vkDestroyPipelineLayout(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_pipeline_layout(), nullptr);
	//vkDestroyRenderPass(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_render_pass(), nullptr);

	for (size_t i = 10; i < get_instance_vulkan()->_get_swap_chain_image_views().size(); i++) {
		vkDestroyImageView(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_swap_chain_image_views()[i], nullptr);
	}

	vkDestroySwapchainKHR(*get_instance_vulkan()->_get_device(), *get_instance_vulkan()->_get_swap_chain(), nullptr);
}

RenderingContextVulkan_Win *RasterizerVulkan::get_instance_vulkan() {
	return dynamic_cast<RenderingContextVulkan_Win *>(context);
}

RasterizerStorageVulkan *RasterizerVulkan::get_storage() {
	return storage;
}

RasterizerCanvasVulkan *RasterizerVulkan::get_canvas() {
	return canvas;
}

RasterizerSceneVulkan *RasterizerVulkan::get_scene() {
	return scene;
}

void RasterizerVulkan::set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale) {
	if (p_image.is_null() || p_image->empty())
		return;

	uint32_t window_w = OS::get_singleton()->get_video_mode().width;
	uint32_t window_h = OS::get_singleton()->get_video_mode().height;

	Rect2 imgrect(0, 0, p_image->get_width(), p_image->get_height());
	Rect2 screenrect;
	if (p_scale) {

		if (window_w > window_h) {
			//scale horizontally
			screenrect.size.y = window_h;
			screenrect.size.x = imgrect.size.x * window_h / imgrect.size.y;
			screenrect.position.x = (window_w - screenrect.size.x) / 2;

		} else {
			//scale vertically
			screenrect.size.x = window_w;
			screenrect.size.y = imgrect.size.y * window_w / imgrect.size.x;
			screenrect.position.y = (window_h - screenrect.size.y) / 2;
		}
	} else {

		screenrect = imgrect;
		screenrect.position += ((Size2(window_w, window_h) - screenrect.size) / 2.0).floor();
	}
	screenrect.set_size(Vector2(1.f, screenrect.size.aspect()));

	storage->data.indices.clear();
	storage->data.indices.push_back(0);
	storage->data.indices.push_back(1);
	storage->data.indices.push_back(2);
	storage->data.indices.push_back(2);
	storage->data.indices.push_back(3);
	storage->data.indices.push_back(0);

	storage->data.vertices.clear();
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(-screenrect.size.width, -screenrect.size.height), Vector3(1.0f, 0.0f, 0.0f), Vector2(1.0f, 0.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(screenrect.size.width, -screenrect.size.height), Vector3(0.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(screenrect.size.width, screenrect.size.height), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 1.0f) });
	storage->data.vertices.push_back(RasterizerStorageVulkan::Vertex{ Vector2(-screenrect.size.width, screenrect.size.height), Vector3(1.0f, 1.0f, 1.0f), Vector2(1.0f, 1.0f) });

	if (OS::get_singleton()->get_window_per_pixel_transparency_enabled()) {
		storage->frame.clear_request_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
	} else {
		storage->frame.clear_request_color = Color(p_color.r, p_color.g, p_color.b, 1.0f);
	}

	begin_frame(0.0);

	canvas->canvas_begin();

	RID texture = storage->texture_create();
	storage->texture_allocate(texture, p_image->get_width(), p_image->get_height(), 0, p_image->get_format(), VS::TextureType::TEXTURE_TYPE_2D);
	storage->texture_set_data(texture, p_image);
	_create_graphics_pipeline();
	update_descriptors();

	_create_index_buffer(storage->data.indices, storage->data.index_buffer);
	_create_vertex_buffer(storage->data.vertices, storage->data.vertex_buffer);

	canvas->draw_generic_textured_rect(screenrect, Rect2(0, 0, 1, 1));
	scene->_update_uniform_buffer(storage->frame.image_index);
	canvas->canvas_end();

	_create_descriptor_set_layout();

	_render_pass_begin();
	
	end_frame(true);

	storage->free(texture); // free since it's only one frame that stays there
}

void RasterizerVulkan::_render_pass_begin() {
	_create_primary_command_buffers();
	for (size_t i = 0; i < get_instance_vulkan()->_get_command_buffers().size(); i++) {
		VkRenderPassBeginInfo info = _create_render_pass_info(i);

		vkCmdBeginRenderPass(get_instance_vulkan()->_get_command_buffers()[i], &info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	}
}

void RasterizerVulkan::update_descriptors() {
	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = get_scene()->uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = VK_WHOLE_SIZE;

		List<RID> textures;
		storage->texture_owner.get_owned_list(&textures);

		Vector<VkDescriptorImageInfo> image_infos;
		for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
			VkDescriptorImageInfo image_info = {};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			RasterizerStorageVulkan::VulkanTexture *t = storage->texture_owner.getornull(E->get());
			image_info.imageView = t->texture_image_view;
			image_info.sampler = t->texture_sampler;
			image_infos.push_back(image_info);
		}

		VkWriteDescriptorSet sampler_descriptor = {};
		sampler_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sampler_descriptor.dstSet = (*canvas->_get_descriptor_sets())[i];
		sampler_descriptor.dstBinding = 1;
		sampler_descriptor.dstArrayElement = 0;
		sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_descriptor.descriptorCount = image_infos.size();
		sampler_descriptor.pImageInfo = image_infos.ptr();

		VkWriteDescriptorSet buffer_descriptor = {};
		buffer_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		buffer_descriptor.dstSet = (*canvas->_get_descriptor_sets())[i];
		buffer_descriptor.dstBinding = 0;
		buffer_descriptor.dstArrayElement = 0;
		buffer_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		buffer_descriptor.descriptorCount = 1;
		buffer_descriptor.pBufferInfo = &buffer_info;

		Vector<VkWriteDescriptorSet> descriptor_writes;
		descriptor_writes.push_back(buffer_descriptor);
		if (image_infos.size() != 0) {
			descriptor_writes.push_back(sampler_descriptor);
		}
		vkUpdateDescriptorSets(*get_instance_vulkan()->_get_device(), descriptor_writes.size(), descriptor_writes.ptr(), 0, nullptr);
	}
}

void RasterizerVulkan::initialize() {
	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("Using Vulkan video driver");
	}

	get_instance_vulkan()->allocate_vma();
	canvas->initialize();
	scene->initialize();
	_create_command_pool();
	_create_descriptor_pool();
	_create_descriptor_set_layout();

	_create_sync_objects();
	_create_descriptor_sets();
}

void RasterizerVulkan::begin_frame(double frame_step) {
	time_total += frame_step;

	if (frame_step == 0) {
		//to avoid hiccups
		frame_step = 0.001;
	}

	double time_roll_over = GLOBAL_GET("rendering/limits/time/time_rollover_secs");
	if (time_total > time_roll_over)
		time_total = 0; //roll over every day (should be custom)

	storage->frame.time[0] = time_total;
	storage->frame.time[1] = Math::fmod(time_total, 3600);
	storage->frame.time[2] = Math::fmod(time_total, 900);
	storage->frame.time[3] = Math::fmod(time_total, 60);
	storage->frame.count++;
	storage->frame.delta = frame_step;

	storage->update_dirty_resources();

	//storage->info.render_final = storage->info.render;
	//storage->info.render.reset();

	//scene->iteration();

	_create_image_views();
	_create_primary_command_buffers();
	_create_render_pass_begin();
	_create_render_pass_end();
	_create_framebuffers();
	_render_pass_begin();
}

void RasterizerVulkan::set_current_render_target(RID p_render_target) {
	if (!p_render_target.is_valid() && storage->frame.current_rt && storage->frame.clear_request) {
		//handle pending clear request, if the framebuffer was not cleared
		//glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->fbo);

		//glClearColor(
		//		storage->frame.clear_request_color.r,
		//		storage->frame.clear_request_color.g,
		//		storage->frame.clear_request_color.b,
		//		storage->frame.clear_request_color.a);

		//glClear(GL_COLOR_BUFFER_BIT);
	}

	if (p_render_target.is_valid()) {
		RasterizerStorageVulkan::RenderTarget *rt = storage->render_target_owner.getornull(p_render_target);
		storage->frame.current_rt = rt;
		ERR_FAIL_COND(!rt);
		storage->frame.clear_request = false;

		scene->_update_uniform_buffer(storage->frame.image_index);

		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = get_instance_vulkan()->_get_command_pool();
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		alloc_info.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(*get_instance_vulkan()->_get_device(), &alloc_info, &rt->command_buffer) != VK_SUCCESS) {
			CRASH_COND("Can't allocate command buffers!");
		}

		VkCommandBufferInheritanceInfo inheritance_info = {};
		inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance_info.pNext = NULL;
		inheritance_info.renderPass = get_instance_vulkan()->_get_render_pass();
		inheritance_info.subpass = 0;
		inheritance_info.framebuffer = VK_NULL_HANDLE; // @TODO Locate framebuffer for better performance

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = &inheritance_info;

		if (vkBeginCommandBuffer(rt->command_buffer, &begin_info) != VK_SUCCESS) {
			CRASH_COND("Can't Record Command Buffer!");
		}

		_create_graphics_pipeline();
	} else {
		storage->frame.current_rt = NULL;
		storage->frame.clear_request = false;
	}
}

void RasterizerVulkan::restore_render_target() {
	ERR_FAIL_COND(storage->frame.current_rt == NULL);
	_recreate_swap_chain();
}

void RasterizerVulkan::clear_render_target(const Color &p_color) {
	ERR_FAIL_COND(!storage->frame.current_rt);

	storage->frame.clear_request = true;
	storage->frame.clear_request_color = p_color;
}

void RasterizerVulkan::blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen) {
	RasterizerStorageVulkan::RenderTarget *rt = storage->render_target_owner.getornull(p_render_target);

	vkCmdBindPipeline(rt->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, get_instance_vulkan()->_get_graphics_pipeline());

	VkRect2D scissor_rect;
	scissor_rect.offset.x = 0;
	scissor_rect.offset.y = 0;
	scissor_rect.extent.width = p_screen_rect.size.width;
	scissor_rect.extent.height = p_screen_rect.size.height;
	vkCmdSetScissor(rt->command_buffer, 0, 1, &scissor_rect);

	VkBuffer vertex_buffers[] = { storage->data.vertex_buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(rt->command_buffer, 0, 1, vertex_buffers, offsets);

	vkCmdBindIndexBuffer(rt->command_buffer, storage->data.index_buffer, 0, VK_INDEX_TYPE_UINT16);
	VkPipelineLayout &pipeline_layout = get_instance_vulkan()->_get_pipeline_layout();
	vkCmdBindDescriptorSets(rt->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline_layout, 0, 1, canvas->_get_descriptor_sets()->ptrw(), 0, NULL);
	vkCmdDrawIndexed(rt->command_buffer, static_cast<uint32_t>(storage->data.indices.size()), 1, 0, 0, 0);

	if (vkEndCommandBuffer(rt->command_buffer) != VK_SUCCESS) {
		CRASH_COND("Can't Record Command Buffer!");
	}
	storage->frame.active_command_buffers.push_back(rt->command_buffer);
}

void RasterizerVulkan::_create_image_views() {
	get_instance_vulkan()->_get_swap_chain_image_views().resize(get_instance_vulkan()->_get_swap_chain_images()->size());
	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = (*get_instance_vulkan()->_get_swap_chain_images())[i];

		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = get_instance_vulkan()->_get_swap_chain_image_format();
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(*get_instance_vulkan()->_get_device(), &create_info, NULL, &get_instance_vulkan()->_get_swap_chain_image_views().write[i]) != VK_SUCCESS) {
			CRASH_COND("Can't Create Vulkan Image Views.");
		}
	}
}

void RasterizerVulkan::_create_render_pass_end() {

	VkAttachmentDescription color_attachment = {};
	color_attachment.format = get_instance_vulkan()->_get_swap_chain_image_format();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &color_attachment;
	renderPassInfo.subpassCount = storage->frame.subpasses.size();
	renderPassInfo.pSubpasses = storage->frame.subpasses.ptr();
	renderPassInfo.dependencyCount = storage->frame.dependencies.size();
	renderPassInfo.pDependencies = storage->frame.dependencies.ptr();

	if (vkCreateRenderPass(*get_instance_vulkan()->_get_device(), &renderPassInfo, NULL, &get_instance_vulkan()->_get_render_pass()) != VK_SUCCESS) {
		CRASH_COND("Can't Create Render Pass.");
	}
}

void RasterizerVulkan::_create_render_pass_begin() {

	VkAttachmentReference *color_attachment_ref = memnew(VkAttachmentReference);
	color_attachment_ref->attachment = 0;
	color_attachment_ref->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = color_attachment_ref;
	storage->frame.subpasses.clear();
	storage->frame.subpasses.push_back(subpass);
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	storage->frame.dependencies.clear();
	storage->frame.dependencies.push_back(dependency);
}

void RasterizerVulkan::_create_descriptor_set_layout() {
	VkDescriptorSetLayoutBinding ubo_layout_binding = {};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	ubo_layout_binding.pImmutableSamplers = NULL;

	VkDescriptorSetLayoutBinding sampler_layout_binding = {};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = nullptr;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Vector<VkDescriptorSetLayoutBinding> bindings;
	bindings.push_back(ubo_layout_binding);
	bindings.push_back(sampler_layout_binding);

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings = bindings.ptr();

	if (vkCreateDescriptorSetLayout(*get_instance_vulkan()->_get_device(), &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
		CRASH_COND("Can't Create Descriptor Set Layout.");
	}
}

void RasterizerVulkan::_create_graphics_pipeline() {
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;

	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_TEXTURE_RECT, true);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_LIGHTING, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_SHADOWS, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::SHADOW_FILTER_NEAREST, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::SHADOW_FILTER_PCF5, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::SHADOW_FILTER_PCF13, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_DISTANCE_FIELD, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_NINEPATCH, false);
	canvas->state.canvas_shader.set_conditional(CanvasShaderVulkan::USE_SKELETON, false);

	canvas->state.canvas_shader.set_custom_shader(0);

	canvas->state.canvas_shader.bind();
	//canvas->state.canvas_shader.set_uniform(CanvasShaderVulkan::FINAL_MODULATE, Color(1, 1, 1, 1));
	//canvas->state.canvas_shader.set_uniform(CanvasShaderVulkan::MODELVIEW_MATRIX, Transform2D());
	//canvas->state.canvas_shader.set_uniform(CanvasShaderVulkan::EXTRA_MATRIX, Transform2D());
	//if (storage->frame.current_rt) {
	//	canvas->state.canvas_shader.set_uniform(CanvasShaderVulkan::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	//} else {
	//	canvas->state.canvas_shader.set_uniform(CanvasShaderVulkan::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	//}

	//state.canvas_shader.set_uniform(CanvasShaderGLES3::PROJECTION_MATRIX,state.vp);
	//state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,Transform());
	//state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform());

	//glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.canvas_item_ubo);
	//glBindVertexArray(data.canvas_quad_array);
	canvas->state.using_texture_rect = true;
	canvas->state.using_ninepatch = false;
	canvas->state.using_skeleton = false;

	VkShaderModuleCreateInfo vert_create_info = {};
	vert_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

	const char *vert = "#version 450\n"
					   "#extension GL_ARB_separate_shader_objects : enable\n"
					   "\n"
					   "layout(binding = 0) uniform UniformBufferObject {\n"
					   "    mat4 model;\n"
					   "    mat4 view;\n"
					   "    mat4 proj;\n"
					   "} ubo;\n"
					   "\n"
					   "layout(location = 0) in vec2 inPosition;\n"
					   "layout(location = 1) in vec3 inColor;\n"
					   "layout(location = 2) in vec2 inTexCoord;\n"
					   "\n"
					   "layout(location = 0) out vec3 fragColor;\n"
					   "layout(location = 1) out vec2 fragTexCoord;\n"
					   "\n"
					   "out gl_PerVertex {\n"
					   "    vec4 gl_Position;\n"
					   "};\n"
					   "\n"
					   "void main() {\n"
					   "    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);\n"
					   "    fragColor = inColor;\n"
					   "    fragTexCoord = inTexCoord;\n"
					   "}";
	PoolByteArray v;
	ShaderVulkan::get_active()->compile_shader_fail(vert, "shader.vert", ShaderVulkan::shaderc_vertex_shader, v);

	vert_create_info.codeSize = v.size();
	vert_create_info.pCode = reinterpret_cast<const uint32_t *>(v.read().ptr());

	if (vkCreateShaderModule(*get_instance_vulkan()->_get_device(), &vert_create_info, NULL, &vert_shader_module) != VK_SUCCESS) {
		CRASH_COND("Can't Create Shader Module");
	}

	VkShaderModuleCreateInfo frag_create_info = {};
	frag_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

	const char *frag = "#version 450\n"
					   "#extension GL_ARB_separate_shader_objects : enable\n"
					   "\n"
					   "layout(binding = 1) uniform sampler2D texSampler;\n"
					   "\n"
					   "layout(location = 0) in vec3 fragColor;\n"
					   "layout(location = 1) in vec2 fragTexCoord;\n"
					   "\n"
					   "layout(location = 0) out vec4 outColor;\n"
					   "\n"
					   "void main() {\n"
					   "    outColor = texture(texSampler, fragTexCoord);\n"
					   "}";
	PoolByteArray f;
	ShaderVulkan::get_active()->compile_shader_fail(frag, "shader.frag", ShaderVulkan::shaderc_fragment_shader, f);
	frag_create_info.codeSize = f.size();
	frag_create_info.pCode = reinterpret_cast<const uint32_t *>(f.read().ptr());

	if (vkCreateShaderModule(*get_instance_vulkan()->_get_device(), &frag_create_info, NULL, &frag_shader_module) != VK_SUCCESS) {
		CRASH_COND("Can't Create Shader Module");
	}

	VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
	vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;

	vert_shader_stage_info.module = vert_shader_module;
	vert_shader_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
	frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.module = frag_shader_module;
	frag_shader_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription binding_description = RasterizerStorageVulkan::Vertex::get_binding_description();
	Vector<VkVertexInputAttributeDescription> attribute_descriptions = RasterizerStorageVulkan::Vertex::get_attribute_descriptions();

	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
	vertex_input_info.pVertexBindingDescriptions = &binding_description;
	vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.ptr();

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = (float)get_instance_vulkan()->_get_swap_chain_extent().height;
	viewport.width = (float)get_instance_vulkan()->_get_swap_chain_extent().width;
	viewport.height = -(float)get_instance_vulkan()->_get_swap_chain_extent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

	rasterizer.lineWidth = 1.0f;

	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = NULL; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	color_blending.blendConstants[0] = 0.0f; // Optional
	color_blending.blendConstants[1] = 0.0f; // Optional
	color_blending.blendConstants[2] = 0.0f; // Optional
	color_blending.blendConstants[3] = 0.0f; // Optional

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 1;
	dynamic_state.pDynamicStates = dynamic_states;

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

	if (vkCreatePipelineLayout(*get_instance_vulkan()->_get_device(), &pipeline_layout_info, NULL, &get_instance_vulkan()->_get_pipeline_layout()) != VK_SUCCESS) {
		CRASH_COND("Can't Create Pipeline Layout.")
	}

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.layout = get_instance_vulkan()->_get_pipeline_layout();
	pipeline_info.renderPass = get_instance_vulkan()->_get_render_pass();
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.pDynamicState = &dynamic_state;

	if (vkCreateGraphicsPipelines(*get_instance_vulkan()->_get_device(), VK_NULL_HANDLE, 1, &pipeline_info, NULL, &get_instance_vulkan()->_get_graphics_pipeline()) != VK_SUCCESS) {
		CRASH_COND("Can't Create Graphics Pipeline.");
	}

	vkDestroyShaderModule(*get_instance_vulkan()->_get_device(), frag_shader_module, NULL);
	vkDestroyShaderModule(*get_instance_vulkan()->_get_device(), vert_shader_module, NULL);
}

void RasterizerVulkan::_create_framebuffers() {
	get_instance_vulkan()->_get_swap_chain_framebuffers().resize(get_instance_vulkan()->_get_swap_chain_image_views().size());
	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_image_views().size(); i++) {
		VkImageView attachments[] = {
			get_instance_vulkan()->_get_swap_chain_image_views()[i]
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = get_instance_vulkan()->_get_render_pass();
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = get_instance_vulkan()->_get_swap_chain_extent().width;
		framebuffer_info.height = get_instance_vulkan()->_get_swap_chain_extent().height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(*get_instance_vulkan()->_get_device(), &framebuffer_info, NULL, &get_instance_vulkan()->_get_swap_chain_framebuffers().write[i]) != VK_SUCCESS) {
			CRASH_COND("Can't Create Framebuffer.");
		}
	}
}

void RasterizerVulkan::_create_command_pool() {
	RenderingContextVulkan::QueueFamilyIndices queue_family_indices = get_instance_vulkan()->_pick_queue_families(get_instance_vulkan()->_get_physical_device());

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
	pool_info.flags = 0; // Optional

	if (vkCreateCommandPool(*get_instance_vulkan()->_get_device(), &pool_info, NULL, &get_instance_vulkan()->_get_command_pool()) != VK_SUCCESS) {
		CRASH_COND("Can't Create Command Pool.");
	}
}

void RasterizerVulkan::_primary_buffer_end(size_t i) {
	if (vkEndCommandBuffer(get_instance_vulkan()->_get_command_buffers()[i]) != VK_SUCCESS) {
		CRASH_COND("Can't Record Command Buffer!");
	}
}

VkRenderPassBeginInfo &RasterizerVulkan::_create_render_pass_info(size_t i) {
	VkCommandBufferBeginInfo being_info = {};
	being_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	being_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	being_info.pInheritanceInfo = NULL; // Optional

	if (vkBeginCommandBuffer(get_instance_vulkan()->_get_command_buffers().write[i], &being_info) != VK_SUCCESS) {
		CRASH_COND("Can't Begin Recording Command Buffer.");
	}

	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = get_instance_vulkan()->_get_render_pass();
	render_pass_info.framebuffer = get_instance_vulkan()->_get_swap_chain_framebuffers()[i];

	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = get_instance_vulkan()->_get_swap_chain_extent();

	VkClearValue clear_color = { storage->frame.clear_request_color.r,
		storage->frame.clear_request_color.g,
		storage->frame.clear_request_color.b,
		storage->frame.clear_request_color.a };
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	return render_pass_info;
}

void RasterizerVulkan::_create_primary_command_buffers() {
	get_instance_vulkan()->_get_command_buffers().resize(get_instance_vulkan()->_get_swap_chain_image_views().size());
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = get_instance_vulkan()->_get_command_pool();
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = (uint32_t)get_instance_vulkan()->_get_swap_chain_image_views().size();

	if (vkAllocateCommandBuffers(*get_instance_vulkan()->_get_device(), &alloc_info, get_instance_vulkan()->_get_command_buffers().ptrw()) != VK_SUCCESS) {
		CRASH_COND("Can't allocate command buffers!");
	}
}

void RasterizerVulkan::_create_sync_objects() {
	get_instance_vulkan()->_get_image_available_semaphore().resize(get_instance_vulkan()->max_frames_in_flight);
	get_instance_vulkan()->_get_render_finished_semaphore().resize(get_instance_vulkan()->max_frames_in_flight);
	get_instance_vulkan()->_get_in_flight_fences()->resize(get_instance_vulkan()->max_frames_in_flight);

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < get_instance_vulkan()->max_frames_in_flight; i++) {
		if (vkCreateSemaphore(*get_instance_vulkan()->_get_device(), &semaphore_info, NULL, &get_instance_vulkan()->_get_image_available_semaphore().write[i]) != VK_SUCCESS ||
				vkCreateSemaphore(*get_instance_vulkan()->_get_device(), &semaphore_info, NULL, &get_instance_vulkan()->_get_render_finished_semaphore().write[i]) != VK_SUCCESS ||
				vkCreateFence(*get_instance_vulkan()->_get_device(), &fence_info, nullptr, &get_instance_vulkan()->_get_in_flight_fences()->write[i]) != VK_SUCCESS) {
			CRASH_COND("Can't Create Semaphores.");
		}
	}
}

void RasterizerVulkan::_recreate_swap_chain() {
	vkDeviceWaitIdle(*get_instance_vulkan()->_get_device());

	_cleanup_swap_chain();

	get_instance_vulkan()->_create_swap_chain();
	_create_image_views();
	_create_primary_command_buffers();
	_create_render_pass_begin();
	_create_render_pass_end();
	_create_framebuffers();
	_render_pass_begin();
	_create_graphics_pipeline();
}

void RasterizerVulkan::end_frame(bool p_swap_buffers) {
	ERR_EXPLAIN("Vulkan Doesn't Swap Buffers Manually.");
	ERR_FAIL_COND(p_swap_buffers == false);

	for (size_t i = 0; i < get_instance_vulkan()->_get_command_buffers().size(); i++) {
		if (storage->frame.active_command_buffers.size() == 0) {
			VkCommandBufferAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = get_instance_vulkan()->_get_command_pool();
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			alloc_info.commandBufferCount = 1;

			VkCommandBuffer command_buffer;

			if (vkAllocateCommandBuffers(*get_instance_vulkan()->_get_device(), &alloc_info, &command_buffer) != VK_SUCCESS) {
				CRASH_COND("Can't allocate command buffers!");
			}

			VkCommandBufferInheritanceInfo inheritance_info = {};
			inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritance_info.pNext = NULL;
			inheritance_info.renderPass = get_instance_vulkan()->_get_render_pass();
			inheritance_info.subpass = 0;
			inheritance_info.framebuffer = VK_NULL_HANDLE; // @TODO Locate framebuffer for better performance

			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin_info.pInheritanceInfo = &inheritance_info;

			if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
				CRASH_COND("Can't Record Command Buffer!");
			}

			_create_graphics_pipeline();

			vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, get_instance_vulkan()->_get_graphics_pipeline());

			VkRect2D scissor_rect;
			scissor_rect.offset.x = 0;
			scissor_rect.offset.y = 0;
			scissor_rect.extent.width = get_instance_vulkan()->get_window_width();
			scissor_rect.extent.height = get_instance_vulkan()->get_window_width();
			vkCmdSetScissor(command_buffer, 0, 1, &scissor_rect);

			VkBuffer vertex_buffers[] = { storage->data.vertex_buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

			vkCmdBindIndexBuffer(command_buffer, storage->data.index_buffer, 0, VK_INDEX_TYPE_UINT16);
			VkPipelineLayout &pipeline_layout = get_instance_vulkan()->_get_pipeline_layout();
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipeline_layout, 0, 1, canvas->_get_descriptor_sets()->ptrw(), 0, NULL);
			vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(storage->data.indices.size()), 1, 0, 0, 0);

			if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
				CRASH_COND("Can't Record Command Buffer!");
			}
			vkCmdExecuteCommands(get_instance_vulkan()->_get_command_buffers().write[i], 1, &command_buffer);
		} else {
			vkCmdExecuteCommands(get_instance_vulkan()->_get_command_buffers().write[i], storage->frame.active_command_buffers.size(), storage->frame.active_command_buffers.ptrw());
		}
	}

	storage->frame.active_command_buffers.clear();

	for (size_t i = 0; i < get_instance_vulkan()->_get_command_buffers().size(); i++) {
		vkCmdEndRenderPass(get_instance_vulkan()->_get_command_buffers().write[i]);
		//@TODO move back here
		_primary_buffer_end(i);
	}

	vkWaitForFences(*get_instance_vulkan()->_get_device(), 1, &(*get_instance_vulkan()->_get_in_flight_fences())[storage->frame.current_frame], VK_TRUE, UINT64_MAX);

	uint32_t image_index;
	VkResult result = vkAcquireNextImageKHR(*get_instance_vulkan()->_get_device(), *get_instance_vulkan()->_get_swap_chain(),
			UINT64_MAX, get_instance_vulkan()->_get_image_available_semaphore()[storage->frame.current_frame], VK_NULL_HANDLE, &image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		_recreate_swap_chain();
		storage->frame.image_index = image_index;
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		CRASH_COND("Can't Acquire Swap Chain Image.");
	}
	storage->frame.image_index = image_index;

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { get_instance_vulkan()->_get_image_available_semaphore()[storage->frame.current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &get_instance_vulkan()->_get_command_buffers()[image_index];

	VkSemaphore signal_semaphores[] = { get_instance_vulkan()->_get_render_finished_semaphore()[storage->frame.current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vkResetFences(*get_instance_vulkan()->_get_device(), 1, &(*get_instance_vulkan()->_get_in_flight_fences())[storage->frame.current_frame]);

	if (vkQueueSubmit(*get_instance_vulkan()->_get_graphics_queue(), 1, &submit_info, (*get_instance_vulkan()->_get_in_flight_fences())[storage->frame.current_frame]) != VK_SUCCESS) {
		CRASH_COND("Can't Submit Draw Command Buffer.");
	}

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swap_chains[] = { *get_instance_vulkan()->_get_swap_chain() };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;

	present_info.pImageIndices = &image_index;

	result = vkQueuePresentKHR(*get_instance_vulkan()->_get_present_queue(), &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
		framebuffer_resized = false;
		_recreate_swap_chain();
	} else if (result != VK_SUCCESS) {
		CRASH_COND("Can't Present Swap Chain Image.");
	}

	storage->frame.current_frame = (storage->frame.current_frame + 1) % get_instance_vulkan()->max_frames_in_flight;
}

void RasterizerVulkan::finalize() {
}

void RasterizerVulkan::make_current(RenderingContext *context) {
	_create_func = memnew(MakeCurrentFunctVulkan(context));
}

void RasterizerVulkan::_cleanup() {
	_cleanup_swap_chain();

	vkDestroyDescriptorSetLayout(*get_instance_vulkan()->_get_device(), descriptor_set_layout, nullptr);
	vkDestroyDescriptorPool(*get_instance_vulkan()->_get_device(), descriptor_pool, nullptr);
	vmaDestroyBuffer(*get_instance_vulkan()->get_allocator(), storage->data.index_buffer, storage->data.allocation_index);
	vmaDestroyBuffer(*get_instance_vulkan()->get_allocator(), storage->data.vertex_buffer, storage->data.allocation_vertex);
	for (size_t i = 0; i < get_instance_vulkan()->_get_swap_chain_images()->size(); i++) {
		vmaDestroyBuffer(*get_instance_vulkan()->get_allocator(), scene->uniform_buffers[i], scene->allocation_uniforms[i]);
	}
	vmaDestroyAllocator(*get_instance_vulkan()->get_allocator());
	for (size_t i = 0; i < get_instance_vulkan()->max_frames_in_flight; i++) {
		vkDestroySemaphore(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_render_finished_semaphore()[i], NULL);
		vkDestroySemaphore(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_image_available_semaphore()[i], NULL);
		vkDestroyFence(*get_instance_vulkan()->_get_device(), (*get_instance_vulkan()->_get_in_flight_fences())[i], nullptr);
	}
	vkDestroyCommandPool(*get_instance_vulkan()->_get_device(), get_instance_vulkan()->_get_command_pool(), NULL);
}

void RasterizerVulkan::register_config() {
	GLOBAL_DEF("rendering/quality/filters/use_nearest_mipmap_filter", false);
	GLOBAL_DEF("rendering/quality/filters/anisotropic_filter_level", 4);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/filters/anisotropic_filter_level", PropertyInfo(Variant::INT, "rendering/quality/filters/anisotropic_filter_level", PROPERTY_HINT_RANGE, "1,16,1"));
	GLOBAL_DEF("rendering/limits/time/time_rollover_secs", 3600);
}

RasterizerVulkan::RasterizerVulkan(RenderingContext *p_context) {
	context = p_context;
	canvas = memnew(RasterizerCanvasVulkan(p_context));
	storage = memnew(RasterizerStorageVulkan(p_context));
	scene = memnew(RasterizerSceneVulkan(p_context));

	scene->storage = storage;
	canvas->storage = storage;
}

RasterizerVulkan::~RasterizerVulkan() {
	vkDeviceWaitIdle(*get_instance_vulkan()->_get_device());
	_cleanup();
}
