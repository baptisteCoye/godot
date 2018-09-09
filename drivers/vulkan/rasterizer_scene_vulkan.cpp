#include "rasterizer_scene_vulkan.h"

void RasterizerSceneVulkan::environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) {
}

void RasterizerSceneVulkan::environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) {
}

void RasterizerSceneVulkan::environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) {
}

RenderingContextVulkan *RasterizerSceneVulkan::get_instance_vulkan() {
	return dynamic_cast<RenderingContextVulkan *>(context);
}

RID RasterizerSceneVulkan::shadow_atlas_create() {
	return RID();
}

void RasterizerSceneVulkan::shadow_atlas_set_size(RID p_atlas, int p_size) {
}

void RasterizerSceneVulkan::shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
}

bool RasterizerSceneVulkan::shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) {
	return false;
}

int RasterizerSceneVulkan::get_directional_light_shadow_size(RID p_light_intance) {
	return 0;
}

void RasterizerSceneVulkan::set_directional_shadow_count(int p_count) {
}

RID RasterizerSceneVulkan::environment_create() {
	return RID();
}

void RasterizerSceneVulkan::environment_set_background(RID p_env, VS::EnvironmentBG p_bg) {
}

void RasterizerSceneVulkan::environment_set_sky(RID p_env, RID p_sky) {
}

void RasterizerSceneVulkan::environment_set_sky_custom_fov(RID p_env, float p_scale) {
}

void RasterizerSceneVulkan::environment_set_bg_color(RID p_env, const Color &p_color) {
}

void RasterizerSceneVulkan::environment_set_bg_energy(RID p_env, float p_energy) {
}

void RasterizerSceneVulkan::environment_set_canvas_max_layer(RID p_env, int p_max_layer) {
}

void RasterizerSceneVulkan::environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution) {
}

void RasterizerSceneVulkan::environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {
}

void RasterizerSceneVulkan::environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {
}

void RasterizerSceneVulkan::environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, bool p_bicubic_upscale) {
}

void RasterizerSceneVulkan::environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) {
}

void RasterizerSceneVulkan::environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) {
}

void RasterizerSceneVulkan::environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) {
}

void RasterizerSceneVulkan::environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_curve, bool p_transmit, float p_transmit_curve) {
}

void RasterizerSceneVulkan::environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) {
}

bool RasterizerSceneVulkan::is_environment(RID p_env) {
	return false;
}

VS::EnvironmentBG RasterizerSceneVulkan::environment_get_background(RID p_env) {
	return VS::ENV_BG_KEEP;
}

int RasterizerSceneVulkan::environment_get_canvas_max_layer(RID p_env) {
	return 0;
}

RID RasterizerSceneVulkan::light_instance_create(RID p_light) {
	return RID();
}

void RasterizerSceneVulkan::light_instance_set_transform(RID p_light_instance, const Transform &p_transform) {
}

void RasterizerSceneVulkan::light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale) {
}

void RasterizerSceneVulkan::light_instance_mark_visible(RID p_light_instance) {
}

RID RasterizerSceneVulkan::reflection_atlas_create() {
	return RID();
}

void RasterizerSceneVulkan::reflection_atlas_set_size(RID p_ref_atlas, int p_size) {
}

void RasterizerSceneVulkan::reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv) {
}

RID RasterizerSceneVulkan::reflection_probe_instance_create(RID p_probe) {
	return RID();
}

void RasterizerSceneVulkan::reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) {
}

void RasterizerSceneVulkan::reflection_probe_release_atlas_index(RID p_instance) {
}

bool RasterizerSceneVulkan::reflection_probe_instance_needs_redraw(RID p_instance) {
	return false;
}

bool RasterizerSceneVulkan::reflection_probe_instance_has_reflection(RID p_instance) {
	return false;
}

bool RasterizerSceneVulkan::reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) {
	return false;
}

bool RasterizerSceneVulkan::reflection_probe_instance_postprocess_step(RID p_instance) {
	return true;
}

RID RasterizerSceneVulkan::gi_probe_instance_create() {
	return RID();
}

void RasterizerSceneVulkan::gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data) {
}

void RasterizerSceneVulkan::gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) {
}

void RasterizerSceneVulkan::gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds) {
}

void RasterizerSceneVulkan::render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) {
}

// https://stackoverflow.com/a/21830596/381724
CameraMatrix RasterizerSceneVulkan::_look_at(const Vector3 eye, const Vector3 center, const Vector3 up) {
	CameraMatrix m;

	Vector3 x, y, z;

	z = eye - center;
	z.normalize();
	y = up;
	x = y.cross(z);

	y = z.cross(x);
	x.normalize();
	y.normalize();
	m.matrix[0][0] = x.x;
	m.matrix[1][0] = x.y;
	m.matrix[2][0] = x.z;
	m.matrix[3][0] = -x.dot(eye);
	m.matrix[0][1] = y.x;
	m.matrix[1][1] = y.y;
	m.matrix[2][1] = y.z;
	m.matrix[3][1] = -y.dot(eye);
	m.matrix[0][2] = z.x;
	m.matrix[1][2] = z.y;
	m.matrix[2][2] = z.z;
	m.matrix[3][2] = -z.dot(eye);
	m.matrix[0][3] = 0;
	m.matrix[1][3] = 0;
	m.matrix[2][3] = 0;
	m.matrix[3][3] = 1.0f;

	return m;
}

void RasterizerSceneVulkan::render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) {
}

void RasterizerSceneVulkan::set_scene_pass(uint64_t p_pass) {
}

void RasterizerSceneVulkan::set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) {
}

bool RasterizerSceneVulkan::free(RID p_rid) {
	return true;
}

void RasterizerSceneVulkan::initialize() {
}

RasterizerSceneVulkan::RasterizerSceneVulkan(RenderingContext *p_context) {
	context = p_context;
}

RasterizerSceneVulkan::~RasterizerSceneVulkan() {
}
