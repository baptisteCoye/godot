// [vertex]

layout(location = 0) in highp vec2 vertex;
layout(location = 3) in vec4 color_attrib;

#ifdef USE_SKELETON
layout(location = 6) in uvec4 bone_indices; // attrib:6
layout(location = 7) in vec4 bone_weights; // attrib:7
#endif

#ifdef USE_TEXTURE_RECT

layout(std140, binding = 1) uniform TextureRect { //ubo:1
	vec4 dst_rect;
	vec4 src_rect;
};

#else

#ifdef USE_INSTANCING

layout(location = 8) in highp vec4 instance_xform0;
layout(location = 9) in highp vec4 instance_xform1;
layout(location = 10) in highp vec4 instance_xform2;
layout(location = 11) in lowp vec4 instance_color;

#ifdef USE_INSTANCE_CUSTOM
layout(location = 12) in highp vec4 instance_custom_data;
#endif

#endif

layout(location = 4) in highp vec2 uv_attrib;

//skeleton
#endif

layout(std140, binding = 2) uniform ColorTexpixel { //ubo:2
	highp vec2 color_texpixel_size;
};

layout(std140, binding = 3) uniform CanvasItemData { //ubo:3

	highp mat4 projection_matrix;
	highp mat4 modelview_matrix;
	highp mat4 extra_matrix;
	highp float time;
};

layout(location = 1) out highp vec2 uv_interp;
layout(location = 2) out mediump vec4 color_interp;

#ifdef USE_NINEPATCH

layout(location = 3) out highp vec2 pixel_size_interp;
#endif

#ifdef USE_SKELETON

uniform mediump sampler2D skeleton_texture; //texunit:2
layout(std140, binding = 4) uniform Skeleton { //ubo:4
	highp mat4 skeleton_transform;
	highp mat4 skeleton_transform_inverse;
};
#endif

#ifdef USE_LIGHTING

layout(std140, binding = 5) uniform LightData { //ubo:5

	//light matrices
	highp mat4 light_matrix;
	highp mat4 light_local_matrix;
	highp mat4 shadow_matrix;
	highp vec4 light_color;
	highp vec4 light_shadow_color;
	highp vec2 light_pos;
	highp float shadowpixel_size;
	highp float shadow_gradient;
	highp float light_height;
	highp float light_outside_alpha;
	highp float shadow_distance_mult;
};

layout(location = 4) out vec4 light_uv_interp;
layout(location = 5) out vec2 transformed_light_uv;

layout(location = 6) out vec4 local_rot;

#ifdef USE_SHADOWS
layout(location = 7) out highp vec2 pos;
#endif

const bool at_light_pass = true;
#else
const bool at_light_pass = false;
#endif

#ifdef USE_PARTICLES
layout(std140, binding = 6) uniform Particles { //ubo:6
	int h_frames;
	int v_frames;
};
#endif

#if defined(USE_MATERIAL)

// clang-format off
layout(std140, binding = 7) uniform UniformData { //ubo:7
MATERIAL_UNIFORMS
	// clang-format on
};

#endif

VERTEX_SHADER_GLOBALS

void main() {

	vec4 color = color_attrib;

#ifdef USE_INSTANCING
	mat4 extra_matrix2 = extra_matrix * transpose(mat4(instance_xform0, instance_xform1, instance_xform2, vec4(0.0, 0.0, 0.0, 1.0)));
	color *= instance_color;
	vec4 instance_custom = instance_custom_data;

#else
	mat4 extra_matrix2 = extra_matrix;
	vec4 instance_custom = vec4(0.0);
#endif

#ifdef USE_TEXTURE_RECT

	if (dst_rect.z < 0.0) { // Transpose is encoded as negative dst_rect.z
		uv_interp = src_rect.xy + abs(src_rect.zw) * vertex.yx;
	} else {
		uv_interp = src_rect.xy + abs(src_rect.zw) * vertex;
	}
	highp vec4 outvec = vec4(dst_rect.xy + abs(dst_rect.zw) * mix(vertex, vec2(1.0, 1.0) - vertex, lessThan(src_rect.zw, vec2(0.0, 0.0))), 0.0, 1.0);

#else
	uv_interp = uv_attrib;
	highp vec4 outvec = vec4(vertex, 0.0, 1.0);
#endif

#ifdef USE_PARTICLES
	//scale by texture size
	outvec.xy /= color_texpixel_size;

	//compute h and v frames and adjust UV interp for animation
	int total_frames = h_frames * v_frames;
	int frame = min(int(float(total_frames) * instance_custom.z), total_frames - 1);
	float frame_w = 1.0 / float(h_frames);
	float frame_h = 1.0 / float(v_frames);
	uv_interp.x = uv_interp.x * frame_w + frame_w * float(frame % h_frames);
	uv_interp.y = uv_interp.y * frame_h + frame_h * float(frame / h_frames);

#endif

#define extra_matrix extra_matrix2

	// clang-format off
{
VERTEX_SHADER_CODE
}
	// clang-format on

#ifdef USE_NINEPATCH

	pixel_size_interp = abs(dst_rect.zw) * vertex;
#endif

#if !defined(SKIP_TRANSFORM_USED)
	outvec = extra_matrix * outvec;
	outvec = modelview_matrix * outvec;
#endif

#undef extra_matrix

	color_interp = color;

#ifdef USE_PIXEL_SNAP

	outvec.xy = floor(outvec + 0.5).xy;
#endif

#ifdef USE_SKELETON

	if (bone_weights != vec4(0.0)) { //must be a valid bone
		//skeleton transform

		ivec4 bone_indicesi = ivec4(bone_indices);

		ivec2 tex_ofs = ivec2(bone_indicesi.x % 256, (bone_indicesi.x / 256) * 2);

		highp mat2x4 m = mat2x4(
								 texelFetch(skeleton_texture, tex_ofs, 0),
								 texelFetch(skeleton_texture, tex_ofs + ivec2(0, 1), 0)) *
						 bone_weights.x;

		tex_ofs = ivec2(bone_indicesi.y % 256, (bone_indicesi.y / 256) * 2);

		m += mat2x4(
					 texelFetch(skeleton_texture, tex_ofs, 0),
					 texelFetch(skeleton_texture, tex_ofs + ivec2(0, 1), 0)) *
			 bone_weights.y;

		tex_ofs = ivec2(bone_indicesi.z % 256, (bone_indicesi.z / 256) * 2);

		m += mat2x4(
					 texelFetch(skeleton_texture, tex_ofs, 0),
					 texelFetch(skeleton_texture, tex_ofs + ivec2(0, 1), 0)) *
			 bone_weights.z;

		tex_ofs = ivec2(bone_indicesi.w % 256, (bone_indicesi.w / 256) * 2);

		m += mat2x4(
					 texelFetch(skeleton_texture, tex_ofs, 0),
					 texelFetch(skeleton_texture, tex_ofs + ivec2(0, 1), 0)) *
			 bone_weights.w;

		mat4 bone_matrix = skeleton_transform * transpose(mat4(m[0], m[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))) * skeleton_transform_inverse;

		outvec = bone_matrix * outvec;
	}

#endif

	gl_Position = projection_matrix * outvec;

#ifdef USE_LIGHTING

	light_uv_interp.xy = (light_matrix * outvec).xy;
	light_uv_interp.zw = (light_local_matrix * outvec).xy;

	mat3 inverse_light_matrix = mat3(inverse(light_matrix));
	inverse_light_matrix[0] = normalize(inverse_light_matrix[0]);
	inverse_light_matrix[1] = normalize(inverse_light_matrix[1]);
	inverse_light_matrix[2] = normalize(inverse_light_matrix[2]);
	transformed_light_uv = (inverse_light_matrix * vec3(light_uv_interp.zw, 0.0)).xy; //for normal mapping

#ifdef USE_SHADOWS
	pos = outvec.xy;
#endif

	local_rot.xy = normalize((modelview_matrix * (extra_matrix * vec4(1.0, 0.0, 0.0, 0.0))).xy);
	local_rot.zw = normalize((modelview_matrix * (extra_matrix * vec4(0.0, 1.0, 0.0, 0.0))).xy);
#ifdef USE_TEXTURE_RECT
	local_rot.xy *= sign(src_rect.z);
	local_rot.zw *= sign(src_rect.w);
#endif

#endif
}

// [fragment]
layout(binding = 9) uniform mediump sampler2D color_texture; // texunit:9
layout(binding = 10) uniform mediump sampler2D normal_texture; // texunit:10

layout(std140, binding = 11) uniform ColorTexpixel { //ubo:11

	highp vec2 color_texpixel_size;
};

layout(location = 1) in highp vec2 uv_interp;
layout(location = 2) in mediump vec4 color_interp;

#if defined(SCREEN_TEXTURE_USED)

layout(binding = 12) uniform sampler2D screen_texture; // texunit:12

#endif

#if defined(SCREEN_UV_USED)

layout(std140, binding = 13) uniform ScreenUV { //ubo:9
	vec2 screen_pixel_size;
};
#endif

layout(std140, std140, binding = 14) uniform CanvasItemData { //ubo:10

	highp mat4 projection_matrix;
	highp float time;
	highp mat4 modelview_matrix;
	highp mat4 extra_matrix;
};

#ifdef USE_LIGHTING

layout(std140, binding = 15) uniform LightData { //ubo:15

	highp mat4 light_matrix;
	highp mat4 light_local_matrix;
	highp mat4 shadow_matrix;
	highp vec4 light_color;
	highp vec4 light_shadow_color;
	highp vec2 light_pos;
	highp float shadowpixel_size;
	highp float shadow_gradient;
	highp float light_height;
	highp float light_outside_alpha;
	highp float shadow_distance_mult;
};

layout(binding = 16) uniform lowp sampler2D light_texture; // texunit:16

layout(location = 4) in vec4 light_uv_interp;
layout(location = 5) in vec2 transformed_light_uv;

layout(location = 6) in vec4 local_rot;

#ifdef USE_SHADOWS

layout(binding = 17) uniform highp sampler2D shadow_texture; // texunit:17

layout(location = 7) in highp vec2 pos;

#endif

const bool at_light_pass = true;
#else
const bool at_light_pass = false;
#endif

layout(std140, binding = 18) uniform Modulate { //ubo:18
	mediump vec4 final_modulate;
};

layout(location = 0) out mediump vec4 frag_color;

#if defined(USE_MATERIAL)

// clang-format off
layout(std140, binding = 19) uniform UniformData{ //ubo:19
MATERIAL_UNIFORMS
	// clang-format on
};

#endif

FRAGMENT_SHADER_GLOBALS

void light_compute(
		inout vec4 light,
		inout vec2 light_vec,
		inout float light_height,
		inout vec4 light_color,
		vec2 light_uv,
		inout vec4 shadow_color,
		vec3 normal,
		vec2 uv,
#if defined(SCREEN_UV_USED)
		vec2 screen_uv,
#endif
		vec4 color) {

#if defined(USE_LIGHT_SHADER_CODE)
	// clang-format off
LIGHT_SHADER_CODE
// clang-format on
#endif
}

#ifdef USE_TEXTURE_RECT

layout(std140, binding = 20) uniform TextureRect { //ubo:20
	highp vec4 dst_rect;
	highp vec4 src_rect;
	bool clip_rect_uv;
};

#ifdef USE_NINEPATCH

layout(location = 3) in highp vec2 pixel_size_interp;

layout(std140, binding = 21) uniform NinePatch { //ubo:21
	int np_repeat_v;
	int np_repeat_h;
	bool np_draw_center;
	//left top right bottom in pixel coordinates
	vec4 np_margins;
};

float map_ninepatch_axis(float pixel, float draw_size, float tex_pixel_size, float margin_begin, float margin_end, int np_repeat, inout int draw_center) {

	float tex_size = 1.0 / tex_pixel_size;

	if (pixel < margin_begin) {
		return pixel * tex_pixel_size;
	} else if (pixel >= draw_size - margin_end) {
		return (tex_size - (draw_size - pixel)) * tex_pixel_size;
	} else {
		if (!np_draw_center) {
			draw_center--;
		}

		if (np_repeat == 0) { //stretch
			//convert to ratio
			float ratio = (pixel - margin_begin) / (draw_size - margin_begin - margin_end);
			//scale to source texture
			return (margin_begin + ratio * (tex_size - margin_begin - margin_end)) * tex_pixel_size;
		} else if (np_repeat == 1) { //tile
			//convert to ratio
			float ofs = mod((pixel - margin_begin), tex_size - margin_begin - margin_end);
			//scale to source texture
			return (margin_begin + ofs) * tex_pixel_size;
		} else if (np_repeat == 2) { //tile fit
			//convert to ratio
			float src_area = draw_size - margin_begin - margin_end;
			float dst_area = tex_size - margin_begin - margin_end;
			float scale = max(1.0, floor(src_area / max(dst_area, 0.0000001) + 0.5));

			//convert to ratio
			float ratio = (pixel - margin_begin) / src_area;
			ratio = mod(ratio * scale, 1.0);
			return (margin_begin + ratio * dst_area) * tex_pixel_size;
		}
	}
}

#endif
#endif

layout(std140, binding = 22) uniform Normal { //ubo:22
	bool use_default_normal;
};

void main() {

	vec4 color = color_interp;
	vec2 uv = uv_interp;

#ifdef USE_TEXTURE_RECT

#ifdef USE_NINEPATCH

	int draw_center = 2;
	uv = vec2(
			map_ninepatch_axis(pixel_size_interp.x, abs(dst_rect.z), color_texpixel_size.x, np_margins.x, np_margins.z, np_repeat_h, draw_center),
			map_ninepatch_axis(pixel_size_interp.y, abs(dst_rect.w), color_texpixel_size.y, np_margins.y, np_margins.w, np_repeat_v, draw_center));

	if (draw_center == 0) {
		color.a = 0.0;
	}

	uv = uv * src_rect.zw + src_rect.xy; //apply region if needed
#endif

	if (clip_rect_uv) {

		uv = clamp(uv, src_rect.xy, src_rect.xy + abs(src_rect.zw));
	}

#endif

#if !defined(COLOR_USED)
	//default behavior, texture by color

#ifdef USE_DISTANCE_FIELD
	const float smoothing = 1.0 / 32.0;
	float distance = textureLod(color_texture, uv, 0.0).a;
	color.a = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance) * color.a;
#else
	color *= texture(color_texture, uv);

#endif

#endif

	vec3 normal;

#if defined(NORMAL_USED)

	bool normal_used = true;
#else
	bool normal_used = false;
#endif

	if (use_default_normal) {
		normal.xy = textureLod(normal_texture, uv, 0.0).xy * 2.0 - 1.0;
		normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
		normal_used = true;
	} else {
		normal = vec3(0.0, 0.0, 1.0);
	}

#if defined(SCREEN_UV_USED)
	vec2 screen_uv = gl_FragCoord.xy * screen_pixel_size;
#endif

	{
		float normal_depth = 1.0;

#if defined(NORMALMAP_USED)
		vec3 normal_map = vec3(0.0, 0.0, 1.0);
#endif

		// clang-format off
FRAGMENT_SHADER_CODE
		// clang-format on

#if defined(NORMALMAP_USED)
		normal = mix(vec3(0.0, 0.0, 1.0), normal_map * vec3(2.0, -2.0, 1.0) - vec3(1.0, -1.0, 0.0), normal_depth);
#endif
	}
#ifdef DEBUG_ENCODED_32
	highp float enc32 = dot(color, highp vec4(1.0 / (256.0 * 256.0 * 256.0), 1.0 / (256.0 * 256.0), 1.0 / 256.0, 1));
	color = vec4(vec3(enc32), 1.0);
#endif

	color *= final_modulate;

#ifdef USE_LIGHTING

	vec2 light_vec = transformed_light_uv;

	if (normal_used) {
		normal.xy = mat2(local_rot.xy, local_rot.zw) * normal.xy;
	}

	float att = 1.0;

	vec2 light_uv = light_uv_interp.xy;
	vec4 light = texture(light_texture, light_uv);

	if (any(lessThan(light_uv_interp.xy, vec2(0.0, 0.0))) || any(greaterThanEqual(light_uv_interp.xy, vec2(1.0, 1.0)))) {
		color.a *= light_outside_alpha; //invisible

	} else {
		float real_light_height = light_height;
		vec4 real_light_color = light_color;
		vec4 real_light_shadow_color = light_shadow_color;

#if defined(USE_LIGHT_SHADER_CODE)
		//light is written by the light shader
		light_compute(
				light,
				light_vec,
				real_light_height,
				real_light_color,
				light_uv,
				real_light_shadow_color,
				normal,
				uv,
#if defined(SCREEN_UV_USED)
				screen_uv,
#endif
				color);
#endif

		light *= real_light_color;

		if (normal_used) {
			vec3 light_normal = normalize(vec3(light_vec, -real_light_height));
			light *= max(dot(-light_normal, normal), 0.0);
		}

		color *= light;

#ifdef USE_SHADOWS
		light_vec = light_uv_interp.zw; //for shadows
		float angle_to_light = -atan(light_vec.x, light_vec.y);
		float PI = 3.14159265358979323846264;
		/*int i = int(mod(floor((angle_to_light+7.0*PI/6.0)/(4.0*PI/6.0))+1.0, 3.0)); // +1 pq os indices estao em ordem 2,0,1 nos arrays
		float ang*/

		float su, sz;

		float abs_angle = abs(angle_to_light);
		vec2 point;
		float sh;
		if (abs_angle < 45.0 * PI / 180.0) {
			point = light_vec;
			sh = 0.0 + (1.0 / 8.0);
		} else if (abs_angle > 135.0 * PI / 180.0) {
			point = -light_vec;
			sh = 0.5 + (1.0 / 8.0);
		} else if (angle_to_light > 0.0) {

			point = vec2(light_vec.y, -light_vec.x);
			sh = 0.25 + (1.0 / 8.0);
		} else {

			point = vec2(-light_vec.y, light_vec.x);
			sh = 0.75 + (1.0 / 8.0);
		}

		highp vec4 s = shadow_matrix * vec4(point, 0.0, 1.0);
		s.xyz /= s.w;
		su = s.x * 0.5 + 0.5;
		sz = s.z * 0.5 + 0.5;
		//sz=lightlength(light_vec);

		highp float shadow_attenuation = 0.0;

#ifdef USE_RGBA_SHADOWS

#define SHADOW_DEPTH(m_tex, m_uv) dot(texture((m_tex), (m_uv)), vec4(1.0 / (256.0 * 256.0 * 256.0), 1.0 / (256.0 * 256.0), 1.0 / 256.0, 1))

#else

#define SHADOW_DEPTH(m_tex, m_uv) (texture((m_tex), (m_uv)).r)

#endif

#ifdef SHADOW_USE_GRADIENT

#define SHADOW_TEST(m_ofs)                                                    \
	{                                                                         \
		highp float sd = SHADOW_DEPTH(shadow_texture, vec2(m_ofs, sh));       \
		shadow_attenuation += 1.0 - smoothstep(sd, sd + shadow_gradient, sz); \
	}

#else

#define SHADOW_TEST(m_ofs)                                              \
	{                                                                   \
		highp float sd = SHADOW_DEPTH(shadow_texture, vec2(m_ofs, sh)); \
		shadow_attenuation += step(sz, sd);                             \
	}

#endif

#ifdef SHADOW_FILTER_NEAREST

		SHADOW_TEST(su);

#endif

#ifdef SHADOW_FILTER_PCF3

		SHADOW_TEST(su + shadowpixel_size);
		SHADOW_TEST(su);
		SHADOW_TEST(su - shadowpixel_size);
		shadow_attenuation /= 3.0;

#endif

#ifdef SHADOW_FILTER_PCF5

		SHADOW_TEST(su + shadowpixel_size * 2.0);
		SHADOW_TEST(su + shadowpixel_size);
		SHADOW_TEST(su);
		SHADOW_TEST(su - shadowpixel_size);
		SHADOW_TEST(su - shadowpixel_size * 2.0);
		shadow_attenuation /= 5.0;

#endif

#ifdef SHADOW_FILTER_PCF7

		SHADOW_TEST(su + shadowpixel_size * 3.0);
		SHADOW_TEST(su + shadowpixel_size * 2.0);
		SHADOW_TEST(su + shadowpixel_size);
		SHADOW_TEST(su);
		SHADOW_TEST(su - shadowpixel_size);
		SHADOW_TEST(su - shadowpixel_size * 2.0);
		SHADOW_TEST(su - shadowpixel_size * 3.0);
		shadow_attenuation /= 7.0;

#endif

#ifdef SHADOW_FILTER_PCF9

		SHADOW_TEST(su + shadowpixel_size * 4.0);
		SHADOW_TEST(su + shadowpixel_size * 3.0);
		SHADOW_TEST(su + shadowpixel_size * 2.0);
		SHADOW_TEST(su + shadowpixel_size);
		SHADOW_TEST(su);
		SHADOW_TEST(su - shadowpixel_size);
		SHADOW_TEST(su - shadowpixel_size * 2.0);
		SHADOW_TEST(su - shadowpixel_size * 3.0);
		SHADOW_TEST(su - shadowpixel_size * 4.0);
		shadow_attenuation /= 9.0;

#endif

#ifdef SHADOW_FILTER_PCF13

		SHADOW_TEST(su + shadowpixel_size * 6.0);
		SHADOW_TEST(su + shadowpixel_size * 5.0);
		SHADOW_TEST(su + shadowpixel_size * 4.0);
		SHADOW_TEST(su + shadowpixel_size * 3.0);
		SHADOW_TEST(su + shadowpixel_size * 2.0);
		SHADOW_TEST(su + shadowpixel_size);
		SHADOW_TEST(su);
		SHADOW_TEST(su - shadowpixel_size);
		SHADOW_TEST(su - shadowpixel_size * 2.0);
		SHADOW_TEST(su - shadowpixel_size * 3.0);
		SHADOW_TEST(su - shadowpixel_size * 4.0);
		SHADOW_TEST(su - shadowpixel_size * 5.0);
		SHADOW_TEST(su - shadowpixel_size * 6.0);
		shadow_attenuation /= 13.0;

#endif

		//color*=shadow_attenuation;
		color = mix(real_light_shadow_color, color, shadow_attenuation);
		//use shadows
#endif
	}

//use lighting
#endif
	//color.rgb*=color.a;
	frag_color = color;
}
