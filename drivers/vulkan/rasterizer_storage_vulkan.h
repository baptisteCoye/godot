#ifndef RASTERIZER_STORAGE_VULKAN_H
#define RASTERIZER_STORAGE_VULKAN_H

#ifdef _WIN32
#include "platform/windows/rendering_context_vulkan_win.h"
#include "platform/windows/vma_windows.h"
#elif defined(__linux__)
#include "platform/x11/rendering_context_vulkan_x11.h"
#include "platform/x11/glad_vulkan_xlib.h"
#endif
#include "servers/visual/rasterizer.h"

class RasterizerStorageVulkan : public RasterizerStorage {
private:
	RenderingContext *context;
	RenderingContextVulkan *_get_instance_vulkan();
	
public:
	struct Vertex {
		Vector2 pos;
		Color color;
		Vector2 tex_coord;
		static VkVertexInputBindingDescription get_binding_description() {
			VkVertexInputBindingDescription bindingDescription = {};

			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static Vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
			Vector<VkVertexInputAttributeDescription> attribute_descriptions;
			attribute_descriptions.resize(3);

			attribute_descriptions.write[0].binding = 0;
			attribute_descriptions.write[0].location = 0;
			attribute_descriptions.write[0].format = VK_FORMAT_R32G32_SFLOAT;
			attribute_descriptions.write[0].offset = offsetof(Vertex, pos);

			attribute_descriptions.write[1].binding = 0;
			attribute_descriptions.write[1].location = 3;
			attribute_descriptions.write[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attribute_descriptions.write[1].offset = offsetof(Vertex, color);

			attribute_descriptions.write[2].binding = 0;
			attribute_descriptions.write[2].location = 1;
			attribute_descriptions.write[2].format = VK_FORMAT_R32G32_SFLOAT;
			attribute_descriptions.write[2].offset = offsetof(Vertex, tex_coord);

			return attribute_descriptions;
		}
	};

	struct Data {
		Vector<Vertex> vertices;
		Vector<uint16_t> indices;

		VkBuffer vertex_buffer;
		VkBuffer index_buffer;

		VmaAllocation allocation_index;
		VmaAllocation allocation_vertex;

	} data;

	struct RenderTarget : public RID_Data {

		VkCommandBuffer command_buffer = VK_NULL_HANDLE;

		VkPipeline graphics_pipeline = VK_NULL_HANDLE;
		uint32_t fbo;

		//GLuint color;

		//struct Buffers {

		//	bool active;
		//	bool effects_active;
		//	GLuint fbo;
		//	GLuint depth;
		//	GLuint specular;
		//	GLuint diffuse;
		//	GLuint normal_rough;
		//	GLuint sss;

		//	GLuint effect_fbo;
		//	GLuint effect;

		//} buffers;

		//struct Effects {

		//	struct MipMaps {

		//		struct Size {
		//			GLuint fbo;
		//			int width;
		//			int height;
		//		};

		//		Vector<Size> sizes;
		//		GLuint color;
		//		int levels;

		//		MipMaps() {
		//			color = 0;
		//			levels = 0;
		//		}
		//	};

		//	MipMaps mip_maps[2]; //first mipmap chain starts from full-screen
		//	//GLuint depth2; //depth for the second mipmap chain, in case of desiring upsampling

		//	struct SSAO {
		//		GLuint blur_fbo[2]; // blur fbo
		//		GLuint blur_red[2]; // 8 bits red buffer

		//		GLuint linear_depth;

		//		Vector<GLuint> depth_mipmap_fbos; //fbos for depth mipmapsla ver

		//		SSAO() {
		//			blur_fbo[0] = 0;
		//			blur_fbo[1] = 0;
		//			linear_depth = 0;
		//		}
		//	} ssao;

		//	Effects() {}

		//} effects;

		//struct Exposure {
		//	GLuint fbo;
		//	GLuint color;

		//	Exposure() { fbo = 0; }
		//} exposure;

		//uint64_t last_exposure_tick;

		int width, height;

		uint32_t depth;

		//bool flags[RENDER_TARGET_FLAG_MAX];

		//bool used_in_frame;
		//VS::ViewportMSAA msaa;

		RID texture;

		RenderTarget();
	};

	mutable RID_Owner<RenderTarget> render_target_owner;

	struct Frame {

		RenderTarget *current_rt;

		bool clear_request;
		Color clear_request_color;
		int canvas_draw_commands;
		float time[4];
		float delta;
		uint64_t count; // Number of frames
		uint32_t image_index = 0; // Vulkan number of frames
		uint64_t current_frame = 0; // Buffer number (can be out of 3)

		Vector<VkSubpassDescription> subpasses;
		Vector<VkSubpassDependency> dependencies;

		VkCommandBuffer primary_command_buffer;
		Vector<VkCommandBuffer> active_command_buffers;
	} frame;

	/* TEXTURE API */
	struct VulkanTexture : public RID_Data {
		int width;
		int height;
		uint32_t flags;
		Image::Format format;
		Ref<Image> image;
		String path;
		VkImage texture_image;
		VmaAllocation texture_image_allocation;
		VkImageView texture_image_view;
		VkSampler texture_sampler;
	};

	struct VulkanSurface {
		uint32_t format;
		VS::PrimitiveType primitive;
		PoolVector<uint8_t> array;
		int vertex_count;
		PoolVector<uint8_t> index_array;
		int index_count;
		AABB aabb;
		Vector<PoolVector<uint8_t> > blend_shapes;
		Vector<AABB> bone_aabbs;
	};

	struct VulkanMesh : public RID_Data {
		Vector<VulkanSurface> surfaces;
		int blend_shape_count;
		VS::BlendShapeMode blend_shape_mode;
	};

public:
	void _render_target_clear(RasterizerStorageVulkan::RenderTarget *rt);
	VkImageView *_create_image_view(VkImage image, VkFormat format);
	VkSampler _create_texture_sampler();
	void _convert_formats(const Ref<Image> &p_image, VkFormat &format, VulkanTexture *t);

	VkCommandBuffer _begin_single_time_commands();
	void _end_single_time_commands(VkCommandBuffer commandBuffer);
	void _transition_image_layout(VkImage texture_image, VkFormat param2, VkImageLayout param3, VkImageLayout param4);
	void _create_buffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation);
	void _create_image(VmaAllocator allocator, VkImageCreateInfo image_info, VkImage &buffer, VmaAllocation &allocation);
	void _create_buffer_staging(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VmaAllocation &allocation, VmaAllocationInfo &alloc_info);
	void _copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void _copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

public:
	mutable RID_Owner<VulkanTexture> texture_owner;
	mutable RID_Owner<VulkanMesh> mesh_owner;

	RID texture_create();
	void texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d,
			Image::Format p_format, VisualServer::TextureType p_type = VS::TEXTURE_TYPE_2D,
			uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT);
	void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_level = 0);

	void texture_set_data_partial(RID p_texture,
			const Ref<Image> &p_image,
			int src_x, int src_y,
			int src_w, int src_h,
			int dst_x, int dst_y,
			int p_dst_mip,
			int p_level = 0);

	Ref<Image> texture_get_data(RID p_texture, int p_level = 0) const;
	VS::TextureType texture_get_type(RID p_texture) const;

	void texture_set_flags(RID p_texture, uint32_t p_flags);
	uint32_t texture_get_flags(RID p_texture) const;
	Image::Format texture_get_format(RID p_texture) const;
	uint32_t texture_get_texid(RID p_texture) const;
	uint32_t texture_get_width(RID p_texture) const;
	uint32_t texture_get_height(RID p_texture) const;
	uint32_t texture_get_depth(RID p_texture) const;
	void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d);

	void texture_set_path(RID p_texture, const String &p_path);
	String texture_get_path(RID p_texture) const;


	void texture_set_shrink_all_x2_on_set_data(bool p_enable);

	void texture_debug_usage(List<VS::TextureInfo> *r_info);

	RID texture_create_radiance_cubemap(RID p_source, int p_resolution = -1) const;

	void texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);
	void texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);
	void texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);

	void textures_keep_original(bool p_enable);

	void texture_set_proxy(RID p_proxy, RID p_base);
	void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable);

	/* SKY API */

	RID sky_create();
	void sky_set_texture(RID p_sky, RID p_cube_map, int p_radiance_size);

	/* SHADER API */

	RID shader_create();

	void shader_set_code(RID p_shader, const String &p_code);
	String shader_get_code(RID p_shader) const;
	void shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const;

	void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture);
	RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const;

	/* COMMON MATERIAL API */

	RID material_create();

	void material_set_render_priority(RID p_material, int priority);
	void material_set_shader(RID p_shader_material, RID p_shader);
	RID material_get_shader(RID p_shader_material) const;

	void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value);
	Variant material_get_param(RID p_material, const StringName &p_param) const;

	void material_set_line_width(RID p_material, float p_width);

	void material_set_next_pass(RID p_material, RID p_next_material);

	bool material_is_animated(RID p_material);
	bool material_casts_shadows(RID p_material);

	void material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);
	void material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);

	/* MESH API */

	RID mesh_create();

	void mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t> > &p_blend_shapes = Vector<PoolVector<uint8_t> >(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>());

	void mesh_set_blend_shape_count(RID p_mesh, int p_amount);
	int mesh_get_blend_shape_count(RID p_mesh) const;

	void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode);
	VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const;

	void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data);

	void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material);
	RID mesh_surface_get_material(RID p_mesh, int p_surface) const;

	int mesh_surface_get_array_len(RID p_mesh, int p_surface) const;
	int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const;

	PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const;
	PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const;

	uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const;
	VS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const;

	AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const;
	Vector<PoolVector<uint8_t> > mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const;
	Vector<AABB> mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const;

	void mesh_remove_surface(RID p_mesh, int p_index);
	int mesh_get_surface_count(RID p_mesh) const;

	void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb);
	AABB mesh_get_custom_aabb(RID p_mesh) const;

	AABB mesh_get_aabb(RID p_mesh, RID p_skeleton) const;
	void mesh_clear(RID p_mesh);

	/* MULTIMESH API */

	virtual RID multimesh_create();

	void multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data = VS::MULTIMESH_CUSTOM_DATA_NONE);
	int multimesh_get_instance_count(RID p_multimesh) const;

	void multimesh_set_mesh(RID p_multimesh, RID p_mesh);
	void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform);
	void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform);
	void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color);
	void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color);

	RID multimesh_get_mesh(RID p_multimesh) const;

	Transform multimesh_instance_get_transform(RID p_multimesh, int p_index) const;
	Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const;
	Color multimesh_instance_get_color(RID p_multimesh, int p_index) const;
	Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const;

	void multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array);

	void multimesh_set_visible_instances(RID p_multimesh, int p_visible);
	int multimesh_get_visible_instances(RID p_multimesh) const;

	AABB multimesh_get_aabb(RID p_multimesh) const;

	/* IMMEDIATE API */

	RID immediate_create();
	void immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID());
	void immediate_vertex(RID p_immediate, const Vector3 &p_vertex);
	void immediate_normal(RID p_immediate, const Vector3 &p_normal);
	void immediate_tangent(RID p_immediate, const Plane &p_tangent);
	void immediate_color(RID p_immediate, const Color &p_color);
	void immediate_uv(RID p_immediate, const Vector2 &tex_uv);
	void immediate_uv2(RID p_immediate, const Vector2 &tex_uv);
	void immediate_end(RID p_immediate);
	void immediate_clear(RID p_immediate);
	void immediate_set_material(RID p_immediate, RID p_material);
	RID immediate_get_material(RID p_immediate) const;
	AABB immediate_get_aabb(RID p_immediate) const;

	/* SKELETON API */

	RID skeleton_create();
	void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false);
	void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform);
	int skeleton_get_bone_count(RID p_skeleton) const;
	void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform);
	Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const;
	void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform);
	Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const;

	/* Light API */

	RID light_create(VS::LightType p_type);

	RID directional_light_create();
	RID omni_light_create();
	RID spot_light_create();

	void light_set_color(RID p_light, const Color &p_color);
	void light_set_param(RID p_light, VS::LightParam p_param, float p_value);
	void light_set_shadow(RID p_light, bool p_enabled);
	void light_set_shadow_color(RID p_light, const Color &p_color);
	void light_set_projector(RID p_light, RID p_texture);
	void light_set_negative(RID p_light, bool p_enable);
	void light_set_cull_mask(RID p_light, uint32_t p_mask);
	void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled);

	void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode);
	void light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail);

	void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode);
	void light_directional_set_blend_splits(RID p_light, bool p_enable);
	bool light_directional_get_blend_splits(RID p_light) const;
	void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode);
	VS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const;

	VS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light);
	VS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light);

	bool light_has_shadow(RID p_light) const;

	VS::LightType light_get_type(RID p_light) const;
	AABB light_get_aabb(RID p_light) const;
	float light_get_param(RID p_light, VS::LightParam p_param);
	Color light_get_color(RID p_light);
	uint64_t light_get_version(RID p_light) const;

	/* PROBE API */

	RID reflection_probe_create();

	void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode);
	void reflection_probe_set_intensity(RID p_probe, float p_intensity);
	void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient);
	void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy);
	void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib);
	void reflection_probe_set_max_distance(RID p_probe, float p_distance);
	void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents);
	void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset);
	void reflection_probe_set_as_interior(RID p_probe, bool p_enable);
	void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable);
	void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable);
	void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers);

	AABB reflection_probe_get_aabb(RID p_probe) const;
	VS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const;
	uint32_t reflection_probe_get_cull_mask(RID p_probe) const;
	Vector3 reflection_probe_get_extents(RID p_probe) const;
	Vector3 reflection_probe_get_origin_offset(RID p_probe) const;
	float reflection_probe_get_origin_max_distance(RID p_probe) const;
	bool reflection_probe_renders_shadows(RID p_probe) const;

	void instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);
	void instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);

	void instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance);
	void instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance);

	/* GI PROBE API */

	RID gi_probe_create();

	void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds);
	AABB gi_probe_get_bounds(RID p_probe) const;

	void gi_probe_set_cell_size(RID p_probe, float p_range);
	float gi_probe_get_cell_size(RID p_probe) const;

	void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform);
	Transform gi_probe_get_to_cell_xform(RID p_probe) const;

	void gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data);
	PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const;

	void gi_probe_set_dynamic_range(RID p_probe, int p_range);
	int gi_probe_get_dynamic_range(RID p_probe) const;

	void gi_probe_set_energy(RID p_probe, float p_range);
	float gi_probe_get_energy(RID p_probe) const;

	void gi_probe_set_bias(RID p_probe, float p_range);
	float gi_probe_get_bias(RID p_probe) const;

	void gi_probe_set_normal_bias(RID p_probe, float p_range);
	float gi_probe_get_normal_bias(RID p_probe) const;

	void gi_probe_set_propagation(RID p_probe, float p_range);
	float gi_probe_get_propagation(RID p_probe) const;

	void gi_probe_set_interior(RID p_probe, bool p_enable);
	bool gi_probe_is_interior(RID p_probe) const;

	void gi_probe_set_compress(RID p_probe, bool p_enable);
	bool gi_probe_is_compressed(RID p_probe) const;

	uint32_t gi_probe_get_version(RID p_probe);

	GIProbeCompression gi_probe_get_dynamic_data_get_preferred_compression() const;
	RID gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression);
	void gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data);

	/* LIGHTMAP CAPTURE */
	struct Instantiable : public RID_Data {

		SelfList<RasterizerScene::InstanceBase>::List instance_list;

		_FORCE_INLINE_ void instance_change_notify();

		_FORCE_INLINE_ void instance_material_change_notify();

		_FORCE_INLINE_ void instance_remove_deps();

		Instantiable();
		virtual ~Instantiable();
	};

	struct LightmapCapture : public Instantiable {

		PoolVector<LightmapCaptureOctree> octree;
		AABB bounds;
		Transform cell_xform;
		int cell_subdiv;
		float energy;
		LightmapCapture();
	};

	mutable RID_Owner<LightmapCapture> lightmap_capture_data_owner;
	void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds);
	AABB lightmap_capture_get_bounds(RID p_capture) const;
	void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree);
	RID lightmap_capture_create();
	PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const;
	void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform);
	Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const;
	void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv);
	int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const;
	void lightmap_capture_set_energy(RID p_capture, float p_energy);
	float lightmap_capture_get_energy(RID p_capture) const;
	const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const;

	/* PARTICLES */

	RID particles_create();

	void particles_set_emitting(RID p_particles, bool p_emitting);
	void particles_set_amount(RID p_particles, int p_amount);
	void particles_set_lifetime(RID p_particles, float p_lifetime);
	void particles_set_one_shot(RID p_particles, bool p_one_shot);
	void particles_set_pre_process_time(RID p_particles, float p_time);
	void particles_set_explosiveness_ratio(RID p_particles, float p_ratio);
	void particles_set_randomness_ratio(RID p_particles, float p_ratio);
	void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb);
	void particles_set_speed_scale(RID p_particles, float p_scale);
	void particles_set_use_local_coordinates(RID p_particles, bool p_enable);
	void particles_set_process_material(RID p_particles, RID p_material);
	void particles_set_fixed_fps(RID p_particles, int p_fps);
	void particles_set_fractional_delta(RID p_particles, bool p_enable);
	void particles_restart(RID p_particles);

	void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order);

	void particles_set_draw_passes(RID p_particles, int p_count);
	void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh);

	void particles_request_process(RID p_particles);
	AABB particles_get_current_aabb(RID p_particles);
	AABB particles_get_aabb(RID p_particles) const;

	void particles_set_emission_transform(RID p_particles, const Transform &p_transform);

	bool particles_get_emitting(RID p_particles);
	int particles_get_draw_passes(RID p_particles) const;
	RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const;

	/* RENDER TARGET */

	RID render_target_create();
	void render_target_set_size(RID p_render_target, int p_width, int p_height);
	RID render_target_get_texture(RID p_render_target) const;
	void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value);
	bool render_target_was_used(RID p_render_target);
	void render_target_clear_used(RID p_render_target);
	void render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa);

	/* CANVAS SHADOW */

	RID canvas_light_shadow_buffer_create(int p_width);

	/* LIGHT SHADOW MAPPING */

	RID canvas_light_occluder_create();
	void canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines);

	VS::InstanceType get_base_type(RID p_rid) const;

	bool free(RID p_rid);

	bool has_os_feature(const String &p_feature) const;

	void update_dirty_resources();

	void set_debug_generate_wireframes(bool p_generate);

	void render_info_begin_capture();
	void render_info_end_capture();
	int get_captured_render_info(VS::RenderInfo p_info);

	int get_render_info(VS::RenderInfo p_info);

	static RasterizerStorage *base_singleton;

	RasterizerStorageVulkan(RenderingContext *p_context);
	;
	~RasterizerStorageVulkan();
};

#endif // RASTERIZER_STORAGE_VULKAN_H
