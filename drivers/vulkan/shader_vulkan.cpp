/*************************************************************************/
/*  shader_vulkan.cpp                                                    */
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

#include "shader_vulkan.h"

#include "print_string.h"

#include "shaders/canvas.glsl.gen.h"
#include "thirdparty/shaderc/src/libshaderc/include/shaderc/shaderc.h"
#include "thirdparty/spirv-cross/spirv_cfg.hpp"
#include "thirdparty/spirv-cross/spirv_cross.hpp"
#include "ustring.h"

#define GLES_OVER_GL

//#define DEBUG_OPENGL

#ifdef DEBUG_OPENGL

//@TODO Output shaderc's glsl compiler errors.
#define DEBUG_TEST_ERROR(m_section)                                         \
	{                                                                       \
		uint32_t err = glGetError();                                        \
		if (err) {                                                          \
			print_line("OpenGL Error #" + itos(err) + " at: " + m_section); \
		}                                                                   \
	}
#else

#define DEBUG_TEST_ERROR(m_section)

#endif

ShaderVulkan *ShaderVulkan::active = NULL;

_FORCE_INLINE_ void ShaderVulkan::_set_uniform_variant(GLint p_uniform, const Variant &p_value) {
	if (p_uniform < 0)
		return; // do none
	switch (p_value.get_type()) {

		case Variant::BOOL:
		case Variant::INT: {

			int val = p_value;
			glUniform1i(p_uniform, val);
		} break;
		case Variant::REAL: {

			real_t val = p_value;
			glUniform1f(p_uniform, val);
		} break;
		case Variant::COLOR: {

			Color val = p_value;
			glUniform4f(p_uniform, val.r, val.g, val.b, val.a);
		} break;
		case Variant::VECTOR2: {

			Vector2 val = p_value;
			glUniform2f(p_uniform, val.x, val.y);
		} break;
		case Variant::VECTOR3: {

			Vector3 val = p_value;
			glUniform3f(p_uniform, val.x, val.y, val.z);
		} break;
		case Variant::PLANE: {

			Plane val = p_value;
			glUniform4f(p_uniform, val.normal.x, val.normal.y, val.normal.z, val.d);
		} break;
		case Variant::QUAT: {

			Quat val = p_value;
			glUniform4f(p_uniform, val.x, val.y, val.z, val.w);
		} break;

		case Variant::TRANSFORM2D: {

			Transform2D tr = p_value;
			GLfloat matrix[16] = { /* build a 16x16 matrix */
				tr.elements[0][0],
				tr.elements[0][1],
				0,
				0,
				tr.elements[1][0],
				tr.elements[1][1],
				0,
				0,
				0,
				0,
				1,
				0,
				tr.elements[2][0],
				tr.elements[2][1],
				0,
				1
			};

			glUniformMatrix4fv(p_uniform, 1, false, matrix);

		} break;
		case Variant::BASIS:
		case Variant::TRANSFORM: {

			Transform tr = p_value;
			GLfloat matrix[16] = { /* build a 16x16 matrix */
				tr.basis.elements[0][0],
				tr.basis.elements[1][0],
				tr.basis.elements[2][0],
				0,
				tr.basis.elements[0][1],
				tr.basis.elements[1][1],
				tr.basis.elements[2][1],
				0,
				tr.basis.elements[0][2],
				tr.basis.elements[1][2],
				tr.basis.elements[2][2],
				0,
				tr.origin.x,
				tr.origin.y,
				tr.origin.z,
				1
			};

			glUniformMatrix4fv(p_uniform, 1, false, matrix);
		} break;
		default: { ERR_FAIL(); } // do nothing
	}
}

//#define DEBUG_SHADER

#ifdef DEBUG_SHADER

#define DEBUG_PRINT(m_text) print_line(m_text);

#else

#define DEBUG_PRINT(m_text)

#endif

void ShaderVulkan::bind_uniforms() {

	if (!uniforms_dirty) {
		return;
	};

	// upload default uniforms
	const Map<uint32_t, Variant>::Element *E = uniform_defaults.front();

	while (E) {
		int idx = E->key();
		int location = version->uniform_location[idx];

		if (location < 0) {
			E = E->next();
			continue;
		}

		const Variant &v = E->value();
		_set_uniform_variant(location, v);
		//print_line("uniform "+itos(location)+" value "+v+ " type "+Variant::get_type_name(v.get_type()));
		E = E->next();
	};

	const Map<uint32_t, CameraMatrix>::Element *C = uniform_cameras.front();
	while (C) {

		int location = version->uniform_location[C->key()];
		if (location < 0) {
			C = C->next();
			continue;
		}

		glUniformMatrix4fv(location, 1, false, &(C->get().matrix[0][0]));
		C = C->next();
	};

	uniforms_dirty = false;
}

PoolByteArray ShaderVulkan::get_vert_program() const {
	return version->vert;
}

void ShaderVulkan::get_descriptor_bindings(PoolByteArray &p_program, Vector<ShaderVulkan::SPIRVResource> &p_bindings, RID_Owner<RasterizerStorageVulkan::VulkanTexture> &texture_owner, VkBuffer &p_uniform, size_t p_uniform_size, VkDescriptorSet &p_descriptor_set, Vector<VkWriteDescriptorSet> &p_write_descriptor_set) {

	spirv_cross::Compiler comp(reinterpret_cast<const uint32_t *>(p_program.read().ptr()), p_program.size() / (sizeof(uint32_t) / (sizeof(uint8_t))));
	spirv_cross::ShaderResources resources = comp.get_shader_resources();

	// https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide

	//Only one stage flag?
	VkShaderStageFlags flags;
	if (comp.get_entry_points_and_stages()[0].execution_model == spv::ExecutionModelFragment) {
		flags = VK_SHADER_STAGE_FRAGMENT_BIT;
	} else if (comp.get_entry_points_and_stages()[0].execution_model == spv::ExecutionModelVertex) {
		flags = VK_SHADER_STAGE_VERTEX_BIT;
	} else {
		ERR_PRINT("Can't create descriptor set, unknown stage.");
	}

	for (size_t i = 0; i < resources.uniform_buffers.size(); i++) {
		spirv_cross::Resource r = resources.uniform_buffers[i];
		uint32_t set = comp.get_decoration(r.id, spv::DecorationDescriptorSet);

		const spirv_cross::SPIRType &base_type = comp.get_type(r.base_type_id);
		const spirv_cross::SPIRType &type = comp.get_type(r.base_type_id);

		ERR_EXPLAIN("Can't create descriptor set, more than one entry point.")
		ERR_CONTINUE(comp.get_entry_points_and_stages().size() != 1)

		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		sampler_layout_binding.stageFlags = flags;
		ShaderVulkan::SPIRVResource resource;
		resource.set = set;
		resource.binding = sampler_layout_binding;
		p_bindings.push_back(resource);

		if (p_uniform == VK_NULL_HANDLE) {
			return;
		}

		bool found = false;
		for (size_t i = 0; i < CanvasShaderVulkan::get_active()->ubo_count; i++) {
			if (sampler_layout_binding.binding == CanvasShaderVulkan::get_active()->ubo_pairs[i].index) {
				found = true;
			}
		}
		if (!found) {
			return;
		}

		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = p_uniform;
		buffer_info.offset = 0;
		buffer_info.range = p_uniform_size;

		VkWriteDescriptorSet buffer_descriptor = {};
		buffer_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		buffer_descriptor.dstSet = p_descriptor_set;
		buffer_descriptor.dstBinding = comp.get_decoration(r.id, spv::DecorationBinding);
		buffer_descriptor.dstArrayElement = 0;
		buffer_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		buffer_descriptor.descriptorCount = 1;
		buffer_descriptor.pBufferInfo = &buffer_info;

		p_write_descriptor_set.push_back(buffer_descriptor);
	}

	VkWriteDescriptorSet sampler_descriptor = {};

	for (size_t i = 0; i < resources.sampled_images.size(); i++) {
		spirv_cross::Resource r = resources.sampled_images[i];
		uint32_t set = comp.get_decoration(r.id, spv::DecorationDescriptorSet);

		const spirv_cross::SPIRType &type = comp.get_type(r.base_type_id);
		std::vector<spirv_cross::EntryPoint> entry_points_and_stages = comp.get_entry_points_and_stages();

		ERR_EXPLAIN("Can't create descriptor set, more than one entry point.")
		ERR_CONTINUE(entry_points_and_stages.size() != 1)

		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.pImmutableSamplers = nullptr;
		sampler_layout_binding.stageFlags = flags;

		ShaderVulkan::SPIRVResource resource;
		resource.set = set;
		resource.binding = sampler_layout_binding;
		p_bindings.push_back(resource);

		List<RID> textures;
		texture_owner.get_owned_list(&textures);

		if (textures.size() == 0) {
			return;
		}

		Vector<VkDescriptorImageInfo> image_infos;
		for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
			VkDescriptorImageInfo image_info = {};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			RasterizerStorageVulkan::VulkanTexture *t = texture_owner.getornull(E->get());
			image_info.imageView = t->texture_image_view;
			image_info.sampler = t->texture_sampler;
			image_infos.push_back(image_info);
		}

		sampler_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sampler_descriptor.dstSet = p_descriptor_set;
		sampler_descriptor.dstBinding = comp.get_decoration(r.id, spv::DecorationBinding);
		sampler_descriptor.dstArrayElement = 0;
		sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_descriptor.descriptorCount = image_infos.size();
		sampler_descriptor.pImageInfo = image_infos.ptr();

		if (image_infos.size() != 0) {
			p_write_descriptor_set.push_back(sampler_descriptor);
		}
	}
}

void ShaderVulkan::compile_shader(const String p_text, const String p_input_file_name, const shader_kind p_kind, PoolByteArray &output, String &error_message, int32_t &num_warnings, int32_t &num_errors) {
	shaderc_shader_kind kind = shaderc_shader_kind(p_kind);
	const shaderc_compiler_t compiler = shaderc_compiler_initialize();
	const CharString temp = p_text.utf8();
	shaderc_compile_options_t opts = shaderc_compile_options_initialize();
	shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
	shaderc_compile_options_set_optimization_level(opts, shaderc_optimization_level_performance);
	shaderc_compilation_result_t result = shaderc_compile_into_spv(
			compiler, temp.ptr(), temp.length(),
			kind, p_input_file_name.ascii().ptr(), "main", opts);

	const char *bytes = shaderc_result_get_bytes(result);
	PoolByteArray o;
	const size_t size = shaderc_result_get_length(result);
	o.resize(size);
	memcpy(o.write().ptr(), bytes, size);
	error_message = shaderc_result_get_error_message(result);
	num_errors = shaderc_result_get_num_errors(result);
	num_warnings = shaderc_result_get_num_warnings(result);
	output = o;
	shaderc_result_release(result);
}

void ShaderVulkan::compile_shader_fail(const String p_source, const String p_input_file_name, const shader_kind p_kind, PoolByteArray &p_output) {
	int32_t num_warnings = 0;
	int32_t num_errors = 0;
	String message = String();
	compile_shader(p_source, p_input_file_name, p_kind, p_output, message, num_errors, num_warnings);
	if (num_warnings > 0 && num_errors == 0) {
		ERR_PRINT(message.utf8().ptr());
	}
	ERR_EXPLAIN(message);
	ERR_FAIL_COND(num_errors != 0, message);
}

_FORCE_INLINE_ ShaderVulkan *ShaderVulkan::get_active() {
	return active;
}

bool ShaderVulkan::bind() {

	if (active != this || !version || new_conditional_version.key != conditional_version.key) {
		conditional_version = new_conditional_version;
		version = get_current_version();
	} else {

		return false;
	}

	ERR_FAIL_COND_V(!version, false);

	DEBUG_TEST_ERROR("Use Program");

	active = this;
	uniforms_dirty = true;

	return true;
}

void ShaderVulkan::unbind() {

	version = NULL;
	glUseProgram(0);
	uniforms_dirty = true;
	active = NULL;
}

static void _display_error_with_code(const String &p_error, const Vector<const char *> &p_code) {

	int line = 1;
	String total_code;

	for (int i = 0; i < p_code.size(); i++) {
		total_code += String(p_code[i]);
	}

	Vector<String> lines = String(total_code).split("\n");

	for (int j = 0; j < lines.size(); j++) {

		print_line(itos(line) + ": " + lines[j]);
		line++;
	}

	ERR_PRINTS(p_error);
}

ShaderVulkan::Version *ShaderVulkan::get_current_version() {

	Version *_v = version_map.getptr(conditional_version);

	if (_v) {

		if (conditional_version.code_version != 0) {
			CustomCode *cc = custom_code_map.getptr(conditional_version.code_version);
			ERR_FAIL_COND_V(!cc, _v);
			if (cc->version == _v->code_version)
				return _v;
		} else {
			return _v;
		}
	}

	if (!_v)
		version_map[conditional_version] = Version();

	Version &v = version_map[conditional_version];

	if (!_v) {

		v.uniform_location = memnew_arr(GLint, uniform_count);

	} else {
		if (v.ok) {
			//bye bye shaders
			v.vert.resize(0);
			v.frag.resize(0);
		}
	}

	v.ok = false;
	/* SETUP CONDITIONALS */

	String strings;

	strings += String("#version 450\n");
	strings += String("#extension GL_ARB_separate_shader_objects : enable\n");
	strings += String("#define GLES_OVER_GL\n");

	int define_line_ofs = 1;

	for (int i = 0; i < custom_defines.size(); i++) {

		strings += String(custom_defines[i].get_data());
		define_line_ofs++;
	}

	for (int j = 0; j < conditional_count; j++) {

		bool enable = ((1 << j) & conditional_version.version);
		strings += String(enable ? conditional_defines[j] : "");
		if (enable)
			define_line_ofs++;

		if (enable) {
			DEBUG_PRINT(conditional_defines[j]);
		}
	}

	//keep them around during the function
	CharString code_string;
	CharString code_string2;
	CharString code_globals;
	CharString material_string;

	CustomCode *cc = NULL;

	if (conditional_version.code_version > 0) {
		//do custom code related stuff

		ERR_FAIL_COND_V(!custom_code_map.has(conditional_version.code_version), NULL);
		cc = &custom_code_map[conditional_version.code_version];
		v.code_version = cc->version;
		define_line_ofs += 2;
	}

	/* VERTEX SHADER */

	if (cc) {
		for (int i = 0; i < cc->custom_defines.size(); i++) {

			strings += String(cc->custom_defines[i].get_data());
			DEBUG_PRINT("CD #" + itos(i) + ": " + String(cc->custom_defines[i]));
		}
	}

	int strings_base_size = strings.size();

	//vertex precision is high
	strings += String("precision highp float;\n");
	strings += String("precision highp int;\n");

	strings += String(vertex_code0.get_data());

	if (cc) {
		material_string = cc->uniforms.ascii();
		strings += String(material_string.get_data());
	}

	strings += String(vertex_code1.get_data());

	if (cc) {
		code_globals = cc->vertex_globals.ascii();
		strings += String(code_globals.get_data());
	}

	strings += String(vertex_code2.get_data());

	if (cc) {
		code_string = cc->vertex.ascii();
		strings += String(code_string.get_data());
	}

	strings += String(vertex_code3.get_data());

	ShaderVulkan::get_active()->compile_shader_fail(strings, get_shader_name() + String(".vert"), ShaderVulkan::shaderc_vertex_shader, v.vert);

	/* FRAGMENT SHADER */

	strings.resize(strings_base_size);
	//fragment precision is medium
	strings += String("precision highp float;\n");
	strings += String("precision highp int;\n");

	strings += String(fragment_code0.get_data());
	if (cc) {
		material_string = cc->uniforms.ascii();
		strings += String(material_string.get_data());
	}

	strings += String(fragment_code1.get_data());

	if (cc) {
		code_globals = cc->fragment_globals.ascii();
		strings += String(code_globals.get_data());
	}

	strings += String(fragment_code2.get_data());

	if (cc) {
		code_string = cc->light.ascii();
		strings += String(code_string.get_data());
	}

	strings += String(fragment_code3.get_data());

	if (cc) {
		code_string2 = cc->fragment.ascii();
		strings += String(code_string2.get_data());
	}

	strings += String(fragment_code4.get_data());

	ShaderVulkan::get_active()->compile_shader_fail(strings, get_shader_name() + String(".frag"), ShaderVulkan::shaderc_fragment_shader, v.frag);

	v.ok = true;

	return &v;
}

void ShaderVulkan::setup(const char **p_conditional_defines, int p_conditional_count, const char **p_uniform_names, int p_uniform_count, const AttributePair *p_attribute_pairs, int p_attribute_count, const TexUnitPair *p_texunit_pairs, int p_texunit_pair_count, const UBOPair *p_ubo_pairs, int p_ubo_pair_count, const Feedback *p_feedback, int p_feedback_count, const char *p_vertex_code, const char *p_fragment_code, int p_vertex_code_start, int p_fragment_code_start) {

	ERR_FAIL_COND(version);
	conditional_version.key = 0;
	new_conditional_version.key = 0;
	uniform_count = p_uniform_count;
	conditional_count = p_conditional_count;
	conditional_defines = p_conditional_defines;
	uniform_names = p_uniform_names;
	vertex_code = p_vertex_code;
	fragment_code = p_fragment_code;
	texunit_pairs = p_texunit_pairs;
	texunit_pair_count = p_texunit_pair_count;
	vertex_code_start = p_vertex_code_start;
	fragment_code_start = p_fragment_code_start;
	attribute_pairs = p_attribute_pairs;
	attribute_pair_count = p_attribute_count;
	ubo_pairs = p_ubo_pairs;
	ubo_count = p_ubo_pair_count;
	feedbacks = p_feedback;
	feedback_count = p_feedback_count;

	//split vertex and shader code (thank you, shader compiler programmers from you know what company).
	{
		String globals_tag = "\nVERTEX_SHADER_GLOBALS";
		String material_tag = "\nMATERIAL_UNIFORMS";
		String code_tag = "\nVERTEX_SHADER_CODE";
		String code = vertex_code;
		int cpos = code.find(material_tag);
		if (cpos == -1) {
			vertex_code0 = code.ascii();
		} else {
			vertex_code0 = code.substr(0, cpos).ascii();
			code = code.substr(cpos + material_tag.length(), code.length());

			cpos = code.find(globals_tag);

			if (cpos == -1) {
				vertex_code1 = code.ascii();
			} else {

				vertex_code1 = code.substr(0, cpos).ascii();
				String code2 = code.substr(cpos + globals_tag.length(), code.length());

				cpos = code2.find(code_tag);
				if (cpos == -1) {
					vertex_code2 = code2.ascii();
				} else {

					vertex_code2 = code2.substr(0, cpos).ascii();
					vertex_code3 = code2.substr(cpos + code_tag.length(), code2.length()).ascii();
				}
			}
		}
	}

	{
		String globals_tag = "\nFRAGMENT_SHADER_GLOBALS";
		String material_tag = "\nMATERIAL_UNIFORMS";
		String code_tag = "\nFRAGMENT_SHADER_CODE";
		String light_code_tag = "\nLIGHT_SHADER_CODE";
		String code = fragment_code;
		int cpos = code.find(material_tag);
		if (cpos == -1) {
			fragment_code0 = code.ascii();
		} else {
			fragment_code0 = code.substr(0, cpos).ascii();
			//print_line("CODE0:\n"+String(fragment_code0.get_data()));
			code = code.substr(cpos + material_tag.length(), code.length());
			cpos = code.find(globals_tag);

			if (cpos == -1) {
				fragment_code1 = code.ascii();
			} else {

				fragment_code1 = code.substr(0, cpos).ascii();
				//print_line("CODE1:\n"+String(fragment_code1.get_data()));

				String code2 = code.substr(cpos + globals_tag.length(), code.length());
				cpos = code2.find(light_code_tag);

				if (cpos == -1) {
					fragment_code2 = code2.ascii();
				} else {

					fragment_code2 = code2.substr(0, cpos).ascii();
					//print_line("CODE2:\n"+String(fragment_code2.get_data()));

					String code3 = code2.substr(cpos + light_code_tag.length(), code2.length());

					cpos = code3.find(code_tag);
					if (cpos == -1) {
						fragment_code3 = code3.ascii();
					} else {

						fragment_code3 = code3.substr(0, cpos).ascii();
						//print_line("CODE3:\n"+String(fragment_code3.get_data()));
						fragment_code4 = code3.substr(cpos + code_tag.length(), code3.length()).ascii();
						//print_line("CODE4:\n"+String(fragment_code4.get_data()));
					}
				}
			}
		}
	}

	//glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_image_units);
}

uint32_t ShaderVulkan::get_version() const {
	return new_conditional_version.version;
}

void ShaderVulkan::set_uniform_camera(int p_idx, const CameraMatrix &p_mat) {
	uniform_cameras[p_idx] = p_mat;
	uniforms_dirty = true;
}

_FORCE_INLINE_ void ShaderVulkan::set_texture_uniform(int p_idx, const Variant &p_value) {
	ERR_FAIL_COND(!version);
	ERR_FAIL_INDEX(p_idx, version->texture_uniform_locations.size());
	_set_uniform_variant(version->texture_uniform_locations[p_idx], p_value);
}

_FORCE_INLINE_ GLint ShaderVulkan::get_texture_uniform_location(int p_idx) {
	ERR_FAIL_COND_V(!version, -1);
	ERR_FAIL_INDEX_V(p_idx, version->texture_uniform_locations.size(), -1);
	return version->texture_uniform_locations[p_idx];
}

void ShaderVulkan::finish() {

	const VersionKey *V = NULL;
	while ((V = version_map.next(V))) {

		Version &v = version_map[*V];
		glDeleteShader(v.vert_id);
		glDeleteShader(v.frag_id);
		glDeleteProgram(v.id);
		memdelete_arr(v.uniform_location);
	}
}

PoolByteArray ShaderVulkan::get_frag_program() const {
	return version->frag;
}

void ShaderVulkan::clear_caches() {

	const VersionKey *V = NULL;
	while ((V = version_map.next(V))) {

		Version &v = version_map[*V];
		glDeleteShader(v.vert_id);
		glDeleteShader(v.frag_id);
		glDeleteProgram(v.id);
		memdelete_arr(v.uniform_location);
	}

	version_map.clear();

	custom_code_map.clear();
	version = NULL;
	last_custom_code = 1;
	uniforms_dirty = true;
}

uint32_t ShaderVulkan::create_custom_shader() {

	custom_code_map[last_custom_code] = CustomCode();
	custom_code_map[last_custom_code].version = 1;
	return last_custom_code++;
}

void ShaderVulkan::set_custom_shader_code(uint32_t p_code_id, const String &p_vertex, const String &p_vertex_globals, const String &p_fragment, const String &p_light, const String &p_fragment_globals, const String &p_uniforms, const Vector<StringName> &p_texture_uniforms, const Vector<CharString> &p_custom_defines) {

	ERR_FAIL_COND(!custom_code_map.has(p_code_id));
	CustomCode *cc = &custom_code_map[p_code_id];

	cc->vertex = p_vertex;
	cc->vertex_globals = p_vertex_globals;
	cc->fragment = p_fragment;
	cc->fragment_globals = p_fragment_globals;
	cc->light = p_light;
	cc->texture_uniforms = p_texture_uniforms;
	cc->uniforms = p_uniforms;
	cc->custom_defines = p_custom_defines;
	cc->version++;
}

void ShaderVulkan::set_custom_shader(uint32_t p_code_id) {

	new_conditional_version.code_version = p_code_id;
}

void ShaderVulkan::free_custom_shader(uint32_t p_code_id) {

	/*  if (! custom_code_map.has( p_code_id )) {
        print_line("no code id "+itos(p_code_id));
    } else {
        print_line("freed code id "+itos(p_code_id));

    }*/

	ERR_FAIL_COND(!custom_code_map.has(p_code_id));
	if (conditional_version.code_version == p_code_id)
		conditional_version.code_version = 0; //bye

	custom_code_map.erase(p_code_id);
}

void ShaderVulkan::set_uniform_default(int p_idx, const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {

		uniform_defaults.erase(p_idx);
	} else {

		uniform_defaults[p_idx] = p_value;
	}
	uniforms_dirty = true;
}

void ShaderVulkan::set_base_material_tex_index(int p_idx) {

	base_material_tex_index = p_idx;
}

void ShaderVulkan::add_custom_define(const String &p_define) {
	custom_defines.push_back(p_define.utf8());
}

ShaderVulkan::ShaderVulkan() {
	version = NULL;
	last_custom_code = 1;
	uniforms_dirty = true;
	base_material_tex_index = 0;
}

ShaderVulkan::~ShaderVulkan() {

	finish();
}

int ShaderVulkan::_get_uniform(int p_which) const {
	ERR_FAIL_INDEX_V(p_which, uniform_count, -1);
	ERR_FAIL_COND_V(!version, -1);
	return version->uniform_location[p_which];
}
