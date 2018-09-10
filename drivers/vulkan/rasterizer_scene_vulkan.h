#ifndef RASTERIZER_SCENE_VULKAN_H
#define RASTERIZER_SCENE_VULKAN_H

#include "drivers/vulkan/shaders/canvas.glsl.gen.h"
#ifdef _WIN32
#include "platform/windows/rendering_context_vulkan_win.h"
#include "platform/windows/vma_usage.h"
#elif defined(__linux__)
#include "platform/x11/rendering_context_vulkan_x11.h"
#include "platform/x11/vma_usage.h"
#endif

#include "rasterizer_storage_vulkan.h"
#include "servers/visual/rasterizer.h"

class RasterizerSceneVulkan : public RasterizerScene {
	RenderingContext *context;
	RenderingContextVulkan *get_instance_vulkan();

public:
	CameraMatrix _look_at(const Vector3 eye, const Vector3 center, const Vector3 up);

public:
	RasterizerStorageVulkan *storage;

	struct State {
		struct SceneDataUBO {
		} ubo_data;
	} state;

	/* SHADOW ATLAS API */

	RID shadow_atlas_create();
	void shadow_atlas_set_size(RID p_atlas, int p_size);
	void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision);
	bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version);

	int get_directional_light_shadow_size(RID p_light_intance);
	void set_directional_shadow_count(int p_count);

	/* ENVIRONMENT API */

	RID environment_create();

	void environment_set_background(RID p_env, VS::EnvironmentBG p_bg);
	void environment_set_sky(RID p_env, RID p_sky);
	void environment_set_sky_custom_fov(RID p_env, float p_scale);
	void environment_set_bg_color(RID p_env, const Color &p_color);
	void environment_set_bg_energy(RID p_env, float p_energy);
	void environment_set_canvas_max_layer(RID p_env, int p_max_layer);
	void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0);

	void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality);
	void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality);
	void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, bool p_bicubic_upscale);
	void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture);

	void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness);
	void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness);

	void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale);

	void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp);

	void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount);
	void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_curve, bool p_transmit, float p_transmit_curve);
	void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve);

	bool is_environment(RID p_env);
	VS::EnvironmentBG environment_get_background(RID p_env);
	int environment_get_canvas_max_layer(RID p_env);

	RID light_instance_create(RID p_light);
	void light_instance_set_transform(RID p_light_instance, const Transform &p_transform);
	void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0);
	void light_instance_mark_visible(RID p_light_instance);

	RID reflection_atlas_create();
	void reflection_atlas_set_size(RID p_ref_atlas, int p_size);
	void reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv);

	RID reflection_probe_instance_create(RID p_probe);
	void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform);
	void reflection_probe_release_atlas_index(RID p_instance);
	bool reflection_probe_instance_needs_redraw(RID p_instance);
	bool reflection_probe_instance_has_reflection(RID p_instance);
	bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas);
	bool reflection_probe_instance_postprocess_step(RID p_instance);

	RID gi_probe_instance_create();
	void gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data);
	void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform);
	void gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds);

	void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass);

	void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count);

	void set_scene_pass(uint64_t p_pass);
	void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw);

	bool free(RID p_rid);
	void initialize();

	RasterizerSceneVulkan(RenderingContext *p_context);
	~RasterizerSceneVulkan();
};

#endif // RASTERIZER_SCENE_VULKAN_H
