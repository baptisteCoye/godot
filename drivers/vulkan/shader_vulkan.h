/*************************************************************************/
/*  shader_vulkan.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef SHADER_VULKAN_H
#define SHADER_VULKAN_H

#include <stdio.h>

#include "platform_config.h"
#ifndef GLES3_INCLUDE_H
#include <GLES3/gl3.h>
#else
#include GLES3_INCLUDE_H
#endif

#include "camera_matrix.h"
#include "hash_map.h"
#include "map.h"
#include "variant.h"
#ifdef _WIN32
#include "platform/windows/glad_vulkan_win.h"
#elif defined(__linux__)
#include "os_x11.h"
#include "platform/x11/glad_vulkan_xlib.h"
#endif

#include "drivers/vulkan/rasterizer_storage_vulkan.h"
#include "thirdparty/spirv-cross/spirv_cross.hpp"

class ShaderVulkan {
protected:
	struct Enum {

		uint64_t mask;
		uint64_t shift;
		const char *defines[16];
	};

	struct EnumValue {

		uint64_t set_mask;
		uint64_t clear_mask;
	};

	struct AttributePair {

		const char *name;
		int index;
	};

	struct UniformPair {
		const char *name;
		Variant::Type type_hint;
	};

	struct TexUnitPair {

		const char *name;
		int index;
	};

	struct UBOPair {

		const char *name;
		int index;
	};

	struct Feedback {

		const char *name;
		int conditional;
	};

	bool uniforms_dirty;

protected:
	int ubo_count;
	const UBOPair *ubo_pairs;

public:
	struct UBO {};

private:
	//@TODO Optimize to a fixed set of shader pools and use a LRU
	int uniform_count;
	int texunit_pair_count;
	int conditional_count;
	int feedback_count;
	int vertex_code_start;
	int fragment_code_start;
	int attribute_pair_count;

	struct CustomCode {

		String vertex;
		String vertex_globals;
		String fragment;
		String fragment_globals;
		String light;
		String uniforms;
		uint32_t version;
		Vector<StringName> texture_uniforms;
		Vector<CharString> custom_defines;
	};

	struct Version {

		GLuint id;
		GLuint vert_id;
		GLuint frag_id;
		PoolByteArray vert;
		PoolByteArray frag;
		GLint *uniform_location;
		Vector<GLint> texture_uniform_locations;
		uint32_t code_version;
		bool ok;
		Version() {
			code_version = 0;
			ok = false;
			uniform_location = NULL;
		}
	};

	Version *version;

	union VersionKey {

		struct {
			uint32_t version;
			uint32_t code_version;
		};
		uint64_t key;
		bool operator==(const VersionKey &p_key) const { return key == p_key.key; }
		bool operator<(const VersionKey &p_key) const { return key < p_key.key; }
	};

	struct VersionKeyHash {

		static _FORCE_INLINE_ uint32_t hash(const VersionKey &p_key) { return HashMapHasherDefault::hash(p_key.key); };
	};

	//this should use a way more cache friendly version..
	HashMap<VersionKey, Version, VersionKeyHash> version_map;

	HashMap<uint32_t, CustomCode> custom_code_map;
	uint32_t last_custom_code;

	VersionKey conditional_version;
	VersionKey new_conditional_version;

	virtual String get_shader_name() const = 0;

	const char **conditional_defines;
	const char **uniform_names;
	const AttributePair *attribute_pairs;
	const TexUnitPair *texunit_pairs;
	const Feedback *feedbacks;
	const char *vertex_code;
	const char *fragment_code;
	CharString fragment_code0;
	CharString fragment_code1;
	CharString fragment_code2;
	CharString fragment_code3;
	CharString fragment_code4;

	CharString vertex_code0;
	CharString vertex_code1;
	CharString vertex_code2;
	CharString vertex_code3;

	Vector<CharString> custom_defines;

	int base_material_tex_index;

	Version *get_current_version();

	static ShaderVulkan *active;

	int max_image_units;

	_FORCE_INLINE_ void _set_uniform_variant(GLint p_uniform, const Variant &p_value);

	Map<uint32_t, Variant> uniform_defaults;
	Map<uint32_t, CameraMatrix> uniform_cameras;

protected:
	Map<int32_t, UBO> ubos;

	_FORCE_INLINE_ void _set_conditional(int p_which, bool p_value) {
		ERR_FAIL_INDEX(p_which, conditional_count);
		if (p_value)
			new_conditional_version.version |= (1 << p_which);
		else
			new_conditional_version.version &= ~(1 << p_which);
	}

	_FORCE_INLINE_ void _set_uniform_buffer_object(int32_t binding, UBO &p_value) {
		ubos.insert(binding, p_value);
	}

	_FORCE_INLINE_ void _unset_uniform_buffer_object(int32_t binding) {
		ubos.erase(binding);
	}
	void setup(const char **p_conditional_defines, int p_conditional_count, const AttributePair *p_attribute_pairs, int p_attribute_count, const TexUnitPair *p_texunit_pairs, int p_texunit_pair_count, const UBOPair *p_ubo_pairs, int p_ubo_pair_count, const Feedback *p_feedback, int p_feedback_count, const char *p_vertex_code, const char *p_fragment_code, int p_vertex_code_start, int p_fragment_code_start);

	ShaderVulkan();

public:
	struct SPIRVResource {
		uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	enum {
		CUSTOM_SHADER_DISABLED = 0
	};

	typedef enum {
		// Forced shader kinds. These shader kinds force the compiler to compile the
		// source code as the specified kind of shader.
		shaderc_vertex_shader,
		shaderc_fragment_shader,
		shaderc_compute_shader,
		shaderc_geometry_shader,
		shaderc_tess_control_shader,
		shaderc_tess_evaluation_shader,

		shaderc_glsl_vertex_shader = shaderc_vertex_shader,
		shaderc_glsl_fragment_shader = shaderc_fragment_shader,
		shaderc_glsl_compute_shader = shaderc_compute_shader,
		shaderc_glsl_geometry_shader = shaderc_geometry_shader,
		shaderc_glsl_tess_control_shader = shaderc_tess_control_shader,
		shaderc_glsl_tess_evaluation_shader = shaderc_tess_evaluation_shader,
		// Deduce the shader kind from #pragma annotation in the source code. Compiler
		// will emit error if #pragma annotation is not found.
		shaderc_glsl_infer_from_source,
		// Default shader kinds. Compiler will fall back to compile the source code as
		// the specified kind of shader when #pragma annotation is not found in the
		// source code.
		shaderc_glsl_default_vertex_shader,
		shaderc_glsl_default_fragment_shader,
		shaderc_glsl_default_compute_shader,
		shaderc_glsl_default_geometry_shader,
		shaderc_glsl_default_tess_control_shader,
		shaderc_glsl_default_tess_evaluation_shader,
		shaderc_spirv_assembly,
	} shader_kind;

	void get_descriptor_bindings(Vector<ShaderVulkan::SPIRVResource> &p_bindings, VkBuffer &p_uniform, RID_Owner<RasterizerStorageVulkan::VulkanTexture> &texture_owner, VkDescriptorSet &p_descriptor_set, Vector<VkWriteDescriptorSet> &p_write_descriptor_set);

	void compile_shader(const String p_text, const String p_input_file_name, const shader_kind p_kind, PoolByteArray &output, String &error_message, int32_t &num_warnings, int32_t &num_errors);

	void compile_shader_fail(const String p_source, const String p_input_file_name, const shader_kind p_kind, PoolByteArray &p_output);

	static _FORCE_INLINE_ ShaderVulkan *ShaderVulkan::get_active() {
		return active;
	}
	bool bind();
	void unbind();

	PoolByteArray &get_vert_program() const;
	PoolByteArray &get_frag_program() const;

	void clear_caches();

	uint32_t create_custom_shader();
	void set_custom_shader_code(uint32_t p_code_id, const String &p_vertex, const String &p_vertex_globals, const String &p_fragment, const String &p_light, const String &p_fragment_globals, const String &p_uniforms, const Vector<StringName> &p_texture_uniforms, const Vector<CharString> &p_custom_defines);
	void set_custom_shader(uint32_t p_code_id);
	void free_custom_shader(uint32_t p_code_id);

	uint32_t get_version() const;

	void set_uniform_camera(int p_idx, const CameraMatrix &p_mat);

	_FORCE_INLINE_ void set_texture_uniform(int p_idx, const Variant &p_value);

	_FORCE_INLINE_ GLint get_texture_uniform_location(int p_idx);

	virtual void init() = 0;
	void finish();

	void set_base_material_tex_index(int p_idx);

	void add_custom_define(const String &p_define);

	virtual ~ShaderVulkan();
};

#endif
