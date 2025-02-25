/*************************************************************************/
/*  editor_scene_importer_gltf.cpp                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "editor_scene_importer_gltf.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/math/disjoint_set.h"
#include "core/math/math_defs.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "modules/regex/regex.h"
#include "resource_importer_scene.h"
#include "scene/3d/bone_attachment.h"
#include "scene/3d/camera.h"
#include "scene/3d/mesh_instance.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/surface_tool.h"
#include "servers/visual_server.h"

uint32_t EditorSceneImporterGLTF::get_import_flags() const {

	return IMPORT_SCENE | IMPORT_ANIMATION;
}
void EditorSceneImporterGLTF::get_extensions(List<String> *r_extensions) const {

	r_extensions->push_back("gltf");
	r_extensions->push_back("glb");
}

Error EditorSceneImporterGLTF::_parse_json(const String &p_path, GLTFState &state) {

	Error err;
	FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (!f) {
		return err;
	}

	Vector<uint8_t> array;
	array.resize(f->get_len());
	f->get_buffer(array.ptrw(), array.size());
	String text;
	text.parse_utf8((const char *)array.ptr(), array.size());

	String err_txt;
	int err_line;
	Variant v;
	err = JSON::parse(text, v, err_txt, err_line);
	if (err != OK) {
		_err_print_error("", p_path.utf8().get_data(), err_line, err_txt.utf8().get_data(), ERR_HANDLER_SCRIPT);
		return err;
	}
	state.json = v;

	return OK;
}

Error EditorSceneImporterGLTF::_parse_glb(const String &p_path, GLTFState &state) {

	Error err;
	FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (!f) {
		return err;
	}

	uint32_t magic = f->get_32();
	ERR_FAIL_COND_V(magic != 0x46546C67, ERR_FILE_UNRECOGNIZED); //glTF
	f->get_32(); // version
	f->get_32(); // length

	uint32_t chunk_length = f->get_32();
	uint32_t chunk_type = f->get_32();

	ERR_FAIL_COND_V(chunk_type != 0x4E4F534A, ERR_PARSE_ERROR); //JSON
	Vector<uint8_t> json_data;
	json_data.resize(chunk_length);
	uint32_t len = f->get_buffer(json_data.ptrw(), chunk_length);
	ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

	String text;
	text.parse_utf8((const char *)json_data.ptr(), json_data.size());

	String err_txt;
	int err_line;
	Variant v;
	err = JSON::parse(text, v, err_txt, err_line);
	if (err != OK) {
		_err_print_error("", p_path.utf8().get_data(), err_line, err_txt.utf8().get_data(), ERR_HANDLER_SCRIPT);
		return err;
	}

	state.json = v;

	//data?

	chunk_length = f->get_32();
	chunk_type = f->get_32();

	if (f->eof_reached()) {
		return OK; //all good
	}

	ERR_FAIL_COND_V(chunk_type != 0x004E4942, ERR_PARSE_ERROR); //BIN

	state.glb_data.resize(chunk_length);
	len = f->get_buffer(state.glb_data.ptrw(), chunk_length);
	ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

	return OK;
}

static Vector3 _arr_to_vec3(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 3, Vector3());
	return Vector3(p_array[0], p_array[1], p_array[2]);
}

static Quat _arr_to_quat(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 4, Quat());
	return Quat(p_array[0], p_array[1], p_array[2], p_array[3]);
}

static Transform _arr_to_xform(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 16, Transform());

	Transform xform;
	xform.basis.set_axis(Vector3::AXIS_X, Vector3(p_array[0], p_array[1], p_array[2]));
	xform.basis.set_axis(Vector3::AXIS_Y, Vector3(p_array[4], p_array[5], p_array[6]));
	xform.basis.set_axis(Vector3::AXIS_Z, Vector3(p_array[8], p_array[9], p_array[10]));
	xform.set_origin(Vector3(p_array[12], p_array[13], p_array[14]));

	return xform;
}

String EditorSceneImporterGLTF::_sanitize_scene_name(const String &name) {
	RegEx regex("([^a-zA-Z0-9_ -]+)");
	String p_name = regex.sub(name, "", true);
	return p_name;
}

String EditorSceneImporterGLTF::_gen_unique_name(GLTFState &state, const String &p_name) {

	const String s_name = _sanitize_scene_name(p_name);

	String name;
	int index = 1;
	while (true) {
		name = s_name;

		if (index > 1) {
			name += " " + itos(index);
		}
		if (!state.unique_names.has(name)) {
			break;
		}
		index++;
	}

	state.unique_names.insert(name);

	return name;
}

String EditorSceneImporterGLTF::_sanitize_bone_name(const String &name) {
	String p_name = name.camelcase_to_underscore(true);

	RegEx pattern_del("([^a-zA-Z0-9_ ])+");
	p_name = pattern_del.sub(p_name, "", true);

	RegEx pattern_nospace(" +");
	p_name = pattern_nospace.sub(p_name, "_", true);

	RegEx pattern_multiple("_+");
	p_name = pattern_multiple.sub(p_name, "_", true);

	RegEx pattern_padded("0+(\\d+)");
	p_name = pattern_padded.sub(p_name, "$1", true);

	return p_name;
}

String EditorSceneImporterGLTF::_gen_unique_bone_name(GLTFState &state, const GLTFSkeletonIndex skel_i, const String &p_name) {

	const String s_name = _sanitize_bone_name(p_name);

	String name;
	int index = 1;
	while (true) {
		name = s_name;

		if (index > 1) {
			name += "_" + itos(index);
		}
		if (!state.skeletons[skel_i].unique_names.has(name)) {
			break;
		}
		index++;
	}

	state.skeletons.write[skel_i].unique_names.insert(name);

	return name;
}

Error EditorSceneImporterGLTF::_parse_scenes(GLTFState &state) {

	ERR_FAIL_COND_V(!state.json.has("scenes"), ERR_FILE_CORRUPT);
	const Array &scenes = state.json["scenes"];
	for (int i = 0; i < 1; i++) { //only first scene is imported
		const Dictionary &s = scenes[i];
		ERR_FAIL_COND_V(!s.has("nodes"), ERR_UNAVAILABLE);
		const Array &nodes = s["nodes"];
		for (int j = 0; j < nodes.size(); j++) {
			state.root_nodes.push_back(nodes[j]);
		}

		if (s.has("name") && s["name"] != "") {
			state.scene_name = _gen_unique_name(state, s["name"]);
		} else {
			state.scene_name = _gen_unique_name(state, "Scene");
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_parse_nodes(GLTFState &state) {

	ERR_FAIL_COND_V(!state.json.has("nodes"), ERR_FILE_CORRUPT);
	const Array &nodes = state.json["nodes"];
	for (int i = 0; i < nodes.size(); i++) {

		GLTFNode *node = memnew(GLTFNode);
		const Dictionary &n = nodes[i];

		if (n.has("name")) {
			node->name = n["name"];
		}
		if (n.has("camera")) {
			node->camera = n["camera"];
		}
		if (n.has("mesh")) {
			node->mesh = n["mesh"];
		}
		if (n.has("skin")) {
			node->skin = n["skin"];
		}
		if (n.has("matrix")) {
			node->xform = _arr_to_xform(n["matrix"]);

		} else {

			if (n.has("translation")) {
				node->translation = _arr_to_vec3(n["translation"]);
			}
			if (n.has("rotation")) {
				node->rotation = _arr_to_quat(n["rotation"]);
			}
			if (n.has("scale")) {
				node->scale = _arr_to_vec3(n["scale"]);
			}

			node->xform.basis.set_quat_scale(node->rotation, node->scale);
			node->xform.origin = node->translation;
		}

		if (n.has("children")) {
			const Array &children = n["children"];
			for (int j = 0; j < children.size(); j++) {
				node->children.push_back(children[j]);
			}
		}

		state.nodes.push_back(node);
	}

	// build the hierarchy
	for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); node_i++) {

		for (int j = 0; j < state.nodes[node_i]->children.size(); j++) {
			GLTFNodeIndex child_i = state.nodes[node_i]->children[j];

			ERR_FAIL_INDEX_V(child_i, state.nodes.size(), ERR_FILE_CORRUPT);
			ERR_CONTINUE(state.nodes[child_i]->parent != -1); //node already has a parent, wtf.

			state.nodes[child_i]->parent = node_i;
		}
	}

	_compute_node_heights(state);

	return OK;
}

void EditorSceneImporterGLTF::_compute_node_heights(GLTFState &state) {

	state.root_nodes.clear();
	for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); ++node_i) {
		GLTFNode *node = state.nodes[node_i];
		node->height = 0;

		GLTFNodeIndex current_i = node_i;
		while (current_i >= 0) {
			const GLTFNodeIndex parent_i = state.nodes[current_i]->parent;
			if (parent_i >= 0) {
				++node->height;
			}
			current_i = parent_i;
		}

		if (node->height == 0) {
			state.root_nodes.push_back(node_i);
		}
	}
}

static Vector<uint8_t> _parse_base64_uri(const String &uri) {

	int start = uri.find(",");
	ERR_FAIL_COND_V(start == -1, Vector<uint8_t>());

	CharString substr = uri.right(start + 1).ascii();

	int strlen = substr.length();

	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1 + 1);

	size_t len = 0;
	ERR_FAIL_COND_V(CryptoCore::b64_decode(buf.ptrw(), buf.size(), &len, (unsigned char *)substr.get_data(), strlen) != OK, Vector<uint8_t>());

	buf.resize(len);

	return buf;
}

Error EditorSceneImporterGLTF::_parse_buffers(GLTFState &state, const String &p_base_path) {

	if (!state.json.has("buffers"))
		return OK;

	const Array &buffers = state.json["buffers"];
	for (GLTFBufferIndex i = 0; i < buffers.size(); i++) {

		if (i == 0 && state.glb_data.size()) {
			state.buffers.push_back(state.glb_data);

		} else {
			const Dictionary &buffer = buffers[i];
			if (buffer.has("uri")) {

				Vector<uint8_t> buffer_data;
				String uri = buffer["uri"];

				if (uri.findn("data:application/octet-stream;base64") == 0) {
					//embedded data
					buffer_data = _parse_base64_uri(uri);
				} else {

					uri = p_base_path.plus_file(uri).replace("\\", "/"); //fix for windows
					buffer_data = FileAccess::get_file_as_array(uri);
					ERR_FAIL_COND_V(buffer.size() == 0, ERR_PARSE_ERROR);
				}

				ERR_FAIL_COND_V(!buffer.has("byteLength"), ERR_PARSE_ERROR);
				int byteLength = buffer["byteLength"];
				ERR_FAIL_COND_V(byteLength < buffer_data.size(), ERR_PARSE_ERROR);
				state.buffers.push_back(buffer_data);
			}
		}
	}

	print_verbose("glTF: Total buffers: " + itos(state.buffers.size()));

	return OK;
}

Error EditorSceneImporterGLTF::_parse_buffer_views(GLTFState &state) {

	ERR_FAIL_COND_V(!state.json.has("bufferViews"), ERR_FILE_CORRUPT);
	const Array &buffers = state.json["bufferViews"];
	for (GLTFBufferViewIndex i = 0; i < buffers.size(); i++) {

		const Dictionary &d = buffers[i];

		GLTFBufferView buffer_view;

		ERR_FAIL_COND_V(!d.has("buffer"), ERR_PARSE_ERROR);
		buffer_view.buffer = d["buffer"];
		ERR_FAIL_COND_V(!d.has("byteLength"), ERR_PARSE_ERROR);
		buffer_view.byte_length = d["byteLength"];

		if (d.has("byteOffset")) {
			buffer_view.byte_offset = d["byteOffset"];
		}

		if (d.has("byteStride")) {
			buffer_view.byte_stride = d["byteStride"];
		}

		if (d.has("target")) {
			const int target = d["target"];
			buffer_view.indices = target == ELEMENT_ARRAY_BUFFER;
		}

		state.buffer_views.push_back(buffer_view);
	}

	print_verbose("glTF: Total buffer views: " + itos(state.buffer_views.size()));

	return OK;
}

EditorSceneImporterGLTF::GLTFType EditorSceneImporterGLTF::_get_type_from_str(const String &p_string) {

	if (p_string == "SCALAR")
		return TYPE_SCALAR;

	if (p_string == "VEC2")
		return TYPE_VEC2;
	if (p_string == "VEC3")
		return TYPE_VEC3;
	if (p_string == "VEC4")
		return TYPE_VEC4;

	if (p_string == "MAT2")
		return TYPE_MAT2;
	if (p_string == "MAT3")
		return TYPE_MAT3;
	if (p_string == "MAT4")
		return TYPE_MAT4;

	ERR_FAIL_V(TYPE_SCALAR);
}

Error EditorSceneImporterGLTF::_parse_accessors(GLTFState &state) {

	ERR_FAIL_COND_V(!state.json.has("accessors"), ERR_FILE_CORRUPT);
	const Array &accessors = state.json["accessors"];
	for (GLTFAccessorIndex i = 0; i < accessors.size(); i++) {

		const Dictionary &d = accessors[i];

		GLTFAccessor accessor;

		ERR_FAIL_COND_V(!d.has("componentType"), ERR_PARSE_ERROR);
		accessor.component_type = d["componentType"];
		ERR_FAIL_COND_V(!d.has("count"), ERR_PARSE_ERROR);
		accessor.count = d["count"];
		ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
		accessor.type = _get_type_from_str(d["type"]);

		if (d.has("bufferView")) {
			accessor.buffer_view = d["bufferView"]; //optional because it may be sparse...
		}

		if (d.has("byteOffset")) {
			accessor.byte_offset = d["byteOffset"];
		}

		if (d.has("max")) {
			accessor.max = d["max"];
		}

		if (d.has("min")) {
			accessor.min = d["min"];
		}

		if (d.has("sparse")) {
			//eeh..

			const Dictionary &s = d["sparse"];

			ERR_FAIL_COND_V(!s.has("count"), ERR_PARSE_ERROR);
			accessor.sparse_count = s["count"];
			ERR_FAIL_COND_V(!s.has("indices"), ERR_PARSE_ERROR);
			const Dictionary &si = s["indices"];

			ERR_FAIL_COND_V(!si.has("bufferView"), ERR_PARSE_ERROR);
			accessor.sparse_indices_buffer_view = si["bufferView"];
			ERR_FAIL_COND_V(!si.has("componentType"), ERR_PARSE_ERROR);
			accessor.sparse_indices_component_type = si["componentType"];

			if (si.has("byteOffset")) {
				accessor.sparse_indices_byte_offset = si["byteOffset"];
			}

			ERR_FAIL_COND_V(!s.has("values"), ERR_PARSE_ERROR);
			const Dictionary &sv = s["values"];

			ERR_FAIL_COND_V(!sv.has("bufferView"), ERR_PARSE_ERROR);
			accessor.sparse_values_buffer_view = sv["bufferView"];
			if (sv.has("byteOffset")) {
				accessor.sparse_values_byte_offset = sv["byteOffset"];
			}
		}

		state.accessors.push_back(accessor);
	}

	print_verbose("glTF: Total accessors: " + itos(state.accessors.size()));

	return OK;
}

String EditorSceneImporterGLTF::_get_component_type_name(const uint32_t p_component) {

	switch (p_component) {
		case COMPONENT_TYPE_BYTE: return "Byte";
		case COMPONENT_TYPE_UNSIGNED_BYTE: return "UByte";
		case COMPONENT_TYPE_SHORT: return "Short";
		case COMPONENT_TYPE_UNSIGNED_SHORT: return "UShort";
		case COMPONENT_TYPE_INT: return "Int";
		case COMPONENT_TYPE_FLOAT: return "Float";
	}

	return "<Error>";
}

String EditorSceneImporterGLTF::_get_type_name(const GLTFType p_component) {

	static const char *names[] = {
		"float",
		"vec2",
		"vec3",
		"vec4",
		"mat2",
		"mat3",
		"mat4"
	};

	return names[p_component];
}

Error EditorSceneImporterGLTF::_decode_buffer_view(GLTFState &state, double *dst, const GLTFBufferViewIndex p_buffer_view, const int skip_every, const int skip_bytes, const int element_size, const int count, const GLTFType type, const int component_count, const int component_type, const int component_size, const bool normalized, const int byte_offset, const bool for_vertex) {

	const GLTFBufferView &bv = state.buffer_views[p_buffer_view];

	int stride = bv.byte_stride ? bv.byte_stride : element_size;
	if (for_vertex && stride % 4) {
		stride += 4 - (stride % 4); //according to spec must be multiple of 4
	}

	ERR_FAIL_INDEX_V(bv.buffer, state.buffers.size(), ERR_PARSE_ERROR);

	const uint32_t offset = bv.byte_offset + byte_offset;
	Vector<uint8_t> buffer = state.buffers[bv.buffer]; //copy on write, so no performance hit
	const uint8_t *bufptr = buffer.ptr();

	//use to debug
	print_verbose("glTF: type " + _get_type_name(type) + " component type: " + _get_component_type_name(component_type) + " stride: " + itos(stride) + " amount " + itos(count));
	print_verbose("glTF: accessor offset" + itos(byte_offset) + " view offset: " + itos(bv.byte_offset) + " total buffer len: " + itos(buffer.size()) + " view len " + itos(bv.byte_length));

	const int buffer_end = (stride * (count - 1)) + element_size;
	ERR_FAIL_COND_V(buffer_end > bv.byte_length, ERR_PARSE_ERROR);

	ERR_FAIL_COND_V((int)(offset + buffer_end) > buffer.size(), ERR_PARSE_ERROR);

	//fill everything as doubles

	for (int i = 0; i < count; i++) {

		const uint8_t *src = &bufptr[offset + i * stride];

		for (int j = 0; j < component_count; j++) {

			if (skip_every && j > 0 && (j % skip_every) == 0) {
				src += skip_bytes;
			}

			double d = 0;

			switch (component_type) {
				case COMPONENT_TYPE_BYTE: {
					int8_t b = int8_t(*src);
					if (normalized) {
						d = (double(b) / 128.0);
					} else {
						d = double(b);
					}
				} break;
				case COMPONENT_TYPE_UNSIGNED_BYTE: {
					uint8_t b = *src;
					if (normalized) {
						d = (double(b) / 255.0);
					} else {
						d = double(b);
					}
				} break;
				case COMPONENT_TYPE_SHORT: {
					int16_t s = *(int16_t *)src;
					if (normalized) {
						d = (double(s) / 32768.0);
					} else {
						d = double(s);
					}
				} break;
				case COMPONENT_TYPE_UNSIGNED_SHORT: {
					uint16_t s = *(uint16_t *)src;
					if (normalized) {
						d = (double(s) / 65535.0);
					} else {
						d = double(s);
					}

				} break;
				case COMPONENT_TYPE_INT: {
					d = *(int *)src;
				} break;
				case COMPONENT_TYPE_FLOAT: {
					d = *(float *)src;
				} break;
			}

			*dst++ = d;
			src += component_size;
		}
	}

	return OK;
}

int EditorSceneImporterGLTF::_get_component_type_size(const int component_type) {

	switch (component_type) {
		case COMPONENT_TYPE_BYTE: return 1; break;
		case COMPONENT_TYPE_UNSIGNED_BYTE: return 1; break;
		case COMPONENT_TYPE_SHORT: return 2; break;
		case COMPONENT_TYPE_UNSIGNED_SHORT: return 2; break;
		case COMPONENT_TYPE_INT: return 4; break;
		case COMPONENT_TYPE_FLOAT: return 4; break;
		default: {
			ERR_FAIL_V(0);
		}
	}
	return 0;
}

Vector<double> EditorSceneImporterGLTF::_decode_accessor(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	//spec, for reference:
	//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#data-alignment

	ERR_FAIL_INDEX_V(p_accessor, state.accessors.size(), Vector<double>());

	const GLTFAccessor &a = state.accessors[p_accessor];

	const int component_count_for_type[7] = {
		1, 2, 3, 4, 4, 9, 16
	};

	const int component_count = component_count_for_type[a.type];
	const int component_size = _get_component_type_size(a.component_type);
	ERR_FAIL_COND_V(component_size == 0, Vector<double>());
	int element_size = component_count * component_size;

	int skip_every = 0;
	int skip_bytes = 0;
	//special case of alignments, as described in spec
	switch (a.component_type) {
		case COMPONENT_TYPE_BYTE:
		case COMPONENT_TYPE_UNSIGNED_BYTE: {

			if (a.type == TYPE_MAT2) {
				skip_every = 2;
				skip_bytes = 2;
				element_size = 8; //override for this case
			}
			if (a.type == TYPE_MAT3) {
				skip_every = 3;
				skip_bytes = 1;
				element_size = 12; //override for this case
			}

		} break;
		case COMPONENT_TYPE_SHORT:
		case COMPONENT_TYPE_UNSIGNED_SHORT: {
			if (a.type == TYPE_MAT3) {
				skip_every = 6;
				skip_bytes = 4;
				element_size = 16; //override for this case
			}
		} break;
		default: {
		}
	}

	Vector<double> dst_buffer;
	dst_buffer.resize(component_count * a.count);
	double *dst = dst_buffer.ptrw();

	if (a.buffer_view >= 0) {

		ERR_FAIL_INDEX_V(a.buffer_view, state.buffer_views.size(), Vector<double>());

		const Error err = _decode_buffer_view(state, dst, a.buffer_view, skip_every, skip_bytes, element_size, a.count, a.type, component_count, a.component_type, component_size, a.normalized, a.byte_offset, p_for_vertex);
		if (err != OK)
			return Vector<double>();

	} else {
		//fill with zeros, as bufferview is not defined.
		for (int i = 0; i < (a.count * component_count); i++) {
			dst_buffer.write[i] = 0;
		}
	}

	if (a.sparse_count > 0) {
		// I could not find any file using this, so this code is so far untested
		Vector<double> indices;
		indices.resize(a.sparse_count);
		const int indices_component_size = _get_component_type_size(a.sparse_indices_component_type);

		Error err = _decode_buffer_view(state, indices.ptrw(), a.sparse_indices_buffer_view, 0, 0, indices_component_size, a.sparse_count, TYPE_SCALAR, 1, a.sparse_indices_component_type, indices_component_size, false, a.sparse_indices_byte_offset, false);
		if (err != OK)
			return Vector<double>();

		Vector<double> data;
		data.resize(component_count * a.sparse_count);
		err = _decode_buffer_view(state, data.ptrw(), a.sparse_values_buffer_view, skip_every, skip_bytes, element_size, a.sparse_count, a.type, component_count, a.component_type, component_size, a.normalized, a.sparse_values_byte_offset, p_for_vertex);
		if (err != OK)
			return Vector<double>();

		for (int i = 0; i < indices.size(); i++) {
			const int write_offset = int(indices[i]) * component_count;

			for (int j = 0; j < component_count; j++) {
				dst[write_offset + j] = data[i * component_count + j];
			}
		}
	}

	return dst_buffer;
}

PoolVector<int> EditorSceneImporterGLTF::_decode_accessor_as_ints(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	PoolVector<int> ret;

	if (attribs.size() == 0)
		return ret;

	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size();
	ret.resize(ret_size);
	{
		PoolVector<int>::Write w = ret.write();
		for (int i = 0; i < ret_size; i++) {
			w[i] = int(attribs_ptr[i]);
		}
	}
	return ret;
}

PoolVector<float> EditorSceneImporterGLTF::_decode_accessor_as_floats(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	PoolVector<float> ret;

	if (attribs.size() == 0)
		return ret;

	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size();
	ret.resize(ret_size);
	{
		PoolVector<float>::Write w = ret.write();
		for (int i = 0; i < ret_size; i++) {
			w[i] = float(attribs_ptr[i]);
		}
	}
	return ret;
}

PoolVector<Vector2> EditorSceneImporterGLTF::_decode_accessor_as_vec2(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	PoolVector<Vector2> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 2 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size() / 2;
	ret.resize(ret_size);
	{
		PoolVector<Vector2>::Write w = ret.write();
		for (int i = 0; i < ret_size; i++) {
			w[i] = Vector2(attribs_ptr[i * 2 + 0], attribs_ptr[i * 2 + 1]);
		}
	}
	return ret;
}

PoolVector<Vector3> EditorSceneImporterGLTF::_decode_accessor_as_vec3(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	PoolVector<Vector3> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 3 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size() / 3;
	ret.resize(ret_size);
	{
		PoolVector<Vector3>::Write w = ret.write();
		for (int i = 0; i < ret_size; i++) {
			w[i] = Vector3(attribs_ptr[i * 3 + 0], attribs_ptr[i * 3 + 1], attribs_ptr[i * 3 + 2]);
		}
	}
	return ret;
}

PoolVector<Color> EditorSceneImporterGLTF::_decode_accessor_as_color(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	PoolVector<Color> ret;

	if (attribs.size() == 0)
		return ret;

	const int type = state.accessors[p_accessor].type;
	ERR_FAIL_COND_V(!(type == TYPE_VEC3 || type == TYPE_VEC4), ret);
	int components;
	if (type == TYPE_VEC3) {
		components = 3;
	} else { // TYPE_VEC4
		components = 4;
	}

	ERR_FAIL_COND_V(attribs.size() % components != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size() / components;
	ret.resize(ret_size);
	{
		PoolVector<Color>::Write w = ret.write();
		for (int i = 0; i < ret_size; i++) {
			w[i] = Color(attribs_ptr[i * 4 + 0], attribs_ptr[i * 4 + 1], attribs_ptr[i * 4 + 2], components == 4 ? attribs_ptr[i * 4 + 3] : 1.0);
		}
	}
	return ret;
}
Vector<Quat> EditorSceneImporterGLTF::_decode_accessor_as_quat(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	Vector<Quat> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size() / 4;
	ret.resize(ret_size);
	{
		for (int i = 0; i < ret_size; i++) {
			ret.write[i] = Quat(attribs_ptr[i * 4 + 0], attribs_ptr[i * 4 + 1], attribs_ptr[i * 4 + 2], attribs_ptr[i * 4 + 3]).normalized();
		}
	}
	return ret;
}
Vector<Transform2D> EditorSceneImporterGLTF::_decode_accessor_as_xform2d(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	Vector<Transform2D> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
	ret.resize(attribs.size() / 4);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i][0] = Vector2(attribs[i * 4 + 0], attribs[i * 4 + 1]);
		ret.write[i][1] = Vector2(attribs[i * 4 + 2], attribs[i * 4 + 3]);
	}
	return ret;
}

Vector<Basis> EditorSceneImporterGLTF::_decode_accessor_as_basis(GLTFState &state, const GLTFAccessorIndex p_accessor, bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	Vector<Basis> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 9 != 0, ret);
	ret.resize(attribs.size() / 9);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i].set_axis(0, Vector3(attribs[i * 9 + 0], attribs[i * 9 + 1], attribs[i * 9 + 2]));
		ret.write[i].set_axis(1, Vector3(attribs[i * 9 + 3], attribs[i * 9 + 4], attribs[i * 9 + 5]));
		ret.write[i].set_axis(2, Vector3(attribs[i * 9 + 6], attribs[i * 9 + 7], attribs[i * 9 + 8]));
	}
	return ret;
}

Vector<Transform> EditorSceneImporterGLTF::_decode_accessor_as_xform(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {

	const Vector<double> attribs = _decode_accessor(state, p_accessor, p_for_vertex);
	Vector<Transform> ret;

	if (attribs.size() == 0)
		return ret;

	ERR_FAIL_COND_V(attribs.size() % 16 != 0, ret);
	ret.resize(attribs.size() / 16);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i].basis.set_axis(0, Vector3(attribs[i * 16 + 0], attribs[i * 16 + 1], attribs[i * 16 + 2]));
		ret.write[i].basis.set_axis(1, Vector3(attribs[i * 16 + 4], attribs[i * 16 + 5], attribs[i * 16 + 6]));
		ret.write[i].basis.set_axis(2, Vector3(attribs[i * 16 + 8], attribs[i * 16 + 9], attribs[i * 16 + 10]));
		ret.write[i].set_origin(Vector3(attribs[i * 16 + 12], attribs[i * 16 + 13], attribs[i * 16 + 14]));
	}
	return ret;
}

Error EditorSceneImporterGLTF::_parse_meshes(GLTFState &state) {

	if (!state.json.has("meshes"))
		return OK;

	Array meshes = state.json["meshes"];
	for (GLTFMeshIndex i = 0; i < meshes.size(); i++) {

		print_verbose("glTF: Parsing mesh: " + itos(i));
		Dictionary d = meshes[i];

		GLTFMesh mesh;
		mesh.mesh.instance();

		ERR_FAIL_COND_V(!d.has("primitives"), ERR_PARSE_ERROR);

		Array primitives = d["primitives"];
		const Dictionary &extras = d.has("extras") ? (Dictionary)d["extras"] : Dictionary();

		for (int j = 0; j < primitives.size(); j++) {

			Dictionary p = primitives[j];

			Array array;
			array.resize(Mesh::ARRAY_MAX);

			ERR_FAIL_COND_V(!p.has("attributes"), ERR_PARSE_ERROR);

			Dictionary a = p["attributes"];

			Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_TRIANGLES;
			if (p.has("mode")) {
				const int mode = p["mode"];
				ERR_FAIL_INDEX_V(mode, 7, ERR_FILE_CORRUPT);
				static const Mesh::PrimitiveType primitives2[7] = {
					Mesh::PRIMITIVE_POINTS,
					Mesh::PRIMITIVE_LINES,
					Mesh::PRIMITIVE_LINE_LOOP,
					Mesh::PRIMITIVE_LINE_STRIP,
					Mesh::PRIMITIVE_TRIANGLES,
					Mesh::PRIMITIVE_TRIANGLE_STRIP,
					Mesh::PRIMITIVE_TRIANGLE_FAN,
				};

				primitive = primitives2[mode];
			}

			ERR_FAIL_COND_V(!a.has("POSITION"), ERR_PARSE_ERROR);
			if (a.has("POSITION")) {
				array[Mesh::ARRAY_VERTEX] = _decode_accessor_as_vec3(state, a["POSITION"], true);
			}
			if (a.has("NORMAL")) {
				array[Mesh::ARRAY_NORMAL] = _decode_accessor_as_vec3(state, a["NORMAL"], true);
			}
			if (a.has("TANGENT")) {
				array[Mesh::ARRAY_TANGENT] = _decode_accessor_as_floats(state, a["TANGENT"], true);
			}
			if (a.has("TEXCOORD_0")) {
				array[Mesh::ARRAY_TEX_UV] = _decode_accessor_as_vec2(state, a["TEXCOORD_0"], true);
			}
			if (a.has("TEXCOORD_1")) {
				array[Mesh::ARRAY_TEX_UV2] = _decode_accessor_as_vec2(state, a["TEXCOORD_1"], true);
			}
			if (a.has("COLOR_0")) {
				array[Mesh::ARRAY_COLOR] = _decode_accessor_as_color(state, a["COLOR_0"], true);
			}
			if (!a.has("JOINTS_1") || !a.has("WEIGHTS_1")) {
				state.is_8_weights = false;
			}
			if (a.has("JOINTS_0") && !state.is_8_weights) {
				array[Mesh::ARRAY_BONES] = _decode_accessor_as_ints(state, a["JOINTS_0"], true);
			}
			if (a.has("JOINTS_1") && state.is_8_weights) {
				PoolVector<int> joints_0 = _decode_accessor_as_ints(state, a["JOINTS_0"], true);
				PoolVector<int> joints_1 = _decode_accessor_as_ints(state, a["JOINTS_1"], true);
				PoolVector<PoolVector<int> > joints_split;
				for (int32_t i = 0; i < joints_0.size(); i += 4) {
					PoolVector<int> joints;
					joints.resize(8);
					joints.write()[0] = joints_0.read()[i + 0];
					joints.write()[1] = joints_0.read()[i + 1];
					joints.write()[2] = joints_0.read()[i + 2];
					joints.write()[3] = joints_0.read()[i + 3];
					joints_split.push_back(joints);
				}
				int32_t joint_count = 0;
				for (int32_t i = 0; i < joints_split.size(); i++) {
					PoolVector<int> &joints = joints_split.write()[i];
					joints.resize(8);
					joints.write()[4] = joints_1.read()[joint_count + 0];
					joints.write()[5] = joints_1.read()[joint_count + 1];
					joints.write()[6] = joints_1.read()[joint_count + 2];
					joints.write()[7] = joints_1.read()[joint_count + 3];
					joint_count += 4;
				}
				PoolVector<int> final_joints;
				for (int32_t i = 0; i < joints_split.size(); i++) {
					final_joints.append_array(joints_split[i]);
				}
				array[Mesh::ARRAY_BONES] = final_joints;
			}
			if (a.has("WEIGHTS_0") && !state.is_8_weights) {
				PoolVector<float> weights_0 = _decode_accessor_as_floats(state, a["WEIGHTS_0"], true);
				{ //gltf does not seem to normalize the weights for some reason..
					int wc = weights_0.size();
					PoolVector<float>::Write w = weights_0.write();

					for (int k = 0; k < wc; k += 4) {
						float total = 0.0;
						total += w[k + 0];
						total += w[k + 1];
						total += w[k + 2];
						total += w[k + 3];
						if (total > 0.0) {
							w[k + 0] /= total;
							w[k + 1] /= total;
							w[k + 2] /= total;
							w[k + 3] /= total;
						}
					}
				}
				array[Mesh::ARRAY_WEIGHTS] = weights_0;
			}

			if (a.has("WEIGHTS_1") && state.is_8_weights) {
				PoolVector<float> weights_0 = _decode_accessor_as_floats(state, a["WEIGHTS_0"], true);
				PoolVector<float> weights_1 = _decode_accessor_as_floats(state, a["WEIGHTS_1"], true);

				PoolVector<PoolVector<float> > weights_split;
				for (int32_t i = 0; i < weights_0.size(); i += 4) {
					PoolVector<float> weights;
					weights.resize(8);
					weights.write()[0] = weights_0.read()[i + 0];
					weights.write()[1] = weights_0.read()[i + 1];
					weights.write()[2] = weights_0.read()[i + 2];
					weights.write()[3] = weights_0.read()[i + 3];
					weights_split.push_back(weights);
				}
				int32_t weight_count = 0;
				for (int32_t i = 0; i < weights_split.size(); i++) {
					PoolVector<float> &weights = weights_split.write()[i];
					weights.resize(8);
					weights.write()[4] = weights_1.read()[weight_count + 0];
					weights.write()[5] = weights_1.read()[weight_count + 1];
					weights.write()[6] = weights_1.read()[weight_count + 2];
					weights.write()[7] = weights_1.read()[weight_count + 3];
					weight_count += 4;
				}
				PoolVector<float> final_weights;
				for (int32_t i = 0; i < weights_split.size(); i++) {
					final_weights.append_array(weights_split[i]);
				}
				{ //gltf does not seem to normalize the weights for some reason..
					int wc = final_weights.size();
					PoolVector<float>::Write w = final_weights.write();

					for (int k = 0; k < wc; k += 8) {
						float total = 0.0;
						total += w[k + 0];
						total += w[k + 1];
						total += w[k + 2];
						total += w[k + 3];
						total += w[k + 4];
						total += w[k + 5];
						total += w[k + 6];
						total += w[k + 7];
						if (total > 0.0) {
							w[k + 0] /= total;
							w[k + 1] /= total;
							w[k + 2] /= total;
							w[k + 3] /= total;
							w[k + 4] /= total;
							w[k + 5] /= total;
							w[k + 6] /= total;
							w[k + 7] /= total;
						}
					}
				}
				array[Mesh::ARRAY_WEIGHTS] = final_weights;
			}

			if (p.has("indices")) {
				PoolVector<int> indices = _decode_accessor_as_ints(state, p["indices"], false);

				if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
					//swap around indices, convert ccw to cw for front face

					const int is = indices.size();
					const PoolVector<int>::Write w = indices.write();
					for (int k = 0; k < is; k += 3) {
						SWAP(w[k + 1], w[k + 2]);
					}
				}
				array[Mesh::ARRAY_INDEX] = indices;

			} else if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
				//generate indices because they need to be swapped for CW/CCW
				const PoolVector<Vector3> &vertices = array[Mesh::ARRAY_VERTEX];
				ERR_FAIL_COND_V(vertices.size() == 0, ERR_PARSE_ERROR);
				PoolVector<int> indices;
				const int vs = vertices.size();
				indices.resize(vs);
				{
					const PoolVector<int>::Write w = indices.write();
					for (int k = 0; k < vs; k += 3) {
						w[k] = k;
						w[k + 1] = k + 2;
						w[k + 2] = k + 1;
					}
				}
				array[Mesh::ARRAY_INDEX] = indices;
			}

			bool generated_tangents = false;
			Variant erased_indices;

			if (primitive == Mesh::PRIMITIVE_TRIANGLES && !a.has("TANGENT") && a.has("TEXCOORD_0") && a.has("NORMAL")) {
				//must generate mikktspace tangents.. ergh..
				Ref<SurfaceTool> st;
				st.instance();
				st->create_from_triangle_arrays(array);
				if (!p.has("targets")) {
					//morph targets should not be reindexed, as array size might differ
					//removing indices is the best bet here
					st->deindex();
					erased_indices = a[Mesh::ARRAY_INDEX];
					a[Mesh::ARRAY_INDEX] = Variant();
				}
				st->generate_tangents();
				array = st->commit_to_arrays();
				generated_tangents = true;
			}

			Array morphs;
			//blend shapes
			if (p.has("targets")) {
				print_verbose("glTF: Mesh has targets");
				const Array &targets = p["targets"];

				//ideally BLEND_SHAPE_MODE_RELATIVE since gltf2 stores in displacement
				//but it could require a larger refactor?
				mesh.mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);

				if (j == 0) {
					const Array &target_names = extras.has("targetNames") ? (Array)extras["targetNames"] : Array();
					for (int k = 0; k < targets.size(); k++) {
						const String name = k < target_names.size() ? (String)target_names[k] : String("morph_") + itos(k);
						mesh.mesh->add_blend_shape(name);
					}
				}

				for (int k = 0; k < targets.size(); k++) {

					const Dictionary &t = targets[k];

					Array array_copy;
					array_copy.resize(Mesh::ARRAY_MAX);

					for (int l = 0; l < Mesh::ARRAY_MAX; l++) {
						array_copy[l] = array[l];
					}

					array_copy[Mesh::ARRAY_INDEX] = Variant();

					if (t.has("POSITION")) {
						PoolVector<Vector3> varr = _decode_accessor_as_vec3(state, t["POSITION"], true);
						const PoolVector<Vector3> src_varr = array[Mesh::ARRAY_VERTEX];
						const int size = src_varr.size();
						ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
						{

							const int max_idx = varr.size();
							varr.resize(size);

							const PoolVector<Vector3>::Write w_varr = varr.write();
							const PoolVector<Vector3>::Read r_varr = varr.read();
							const PoolVector<Vector3>::Read r_src_varr = src_varr.read();
							for (int l = 0; l < size; l++) {
								if (l < max_idx) {
									w_varr[l] = r_varr[l] + r_src_varr[l];
								} else {
									w_varr[l] = r_src_varr[l];
								}
							}
						}
						array_copy[Mesh::ARRAY_VERTEX] = varr;
					}
					if (t.has("NORMAL")) {
						PoolVector<Vector3> narr = _decode_accessor_as_vec3(state, t["NORMAL"], true);
						const PoolVector<Vector3> src_narr = array[Mesh::ARRAY_NORMAL];
						int size = src_narr.size();
						ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
						{
							int max_idx = narr.size();
							narr.resize(size);

							const PoolVector<Vector3>::Write w_narr = narr.write();
							const PoolVector<Vector3>::Read r_narr = narr.read();
							const PoolVector<Vector3>::Read r_src_narr = src_narr.read();
							for (int l = 0; l < size; l++) {
								if (l < max_idx) {
									w_narr[l] = r_narr[l] + r_src_narr[l];
								} else {
									w_narr[l] = r_src_narr[l];
								}
							}
						}
						array_copy[Mesh::ARRAY_NORMAL] = narr;
					}
					if (t.has("TANGENT")) {
						const PoolVector<Vector3> tangents_v3 = _decode_accessor_as_vec3(state, t["TANGENT"], true);
						const PoolVector<float> src_tangents = array[Mesh::ARRAY_TANGENT];
						ERR_FAIL_COND_V(src_tangents.size() == 0, ERR_PARSE_ERROR);

						PoolVector<float> tangents_v4;

						{

							int max_idx = tangents_v3.size();

							int size4 = src_tangents.size();
							tangents_v4.resize(size4);
							const PoolVector<float>::Write w4 = tangents_v4.write();

							const PoolVector<Vector3>::Read r3 = tangents_v3.read();
							const PoolVector<float>::Read r4 = src_tangents.read();

							for (int l = 0; l < size4 / 4; l++) {

								if (l < max_idx) {
									w4[l * 4 + 0] = r3[l].x + r4[l * 4 + 0];
									w4[l * 4 + 1] = r3[l].y + r4[l * 4 + 1];
									w4[l * 4 + 2] = r3[l].z + r4[l * 4 + 2];
								} else {
									w4[l * 4 + 0] = r4[l * 4 + 0];
									w4[l * 4 + 1] = r4[l * 4 + 1];
									w4[l * 4 + 2] = r4[l * 4 + 2];
								}
								w4[l * 4 + 3] = r4[l * 4 + 3]; //copy flip value
							}
						}

						array_copy[Mesh::ARRAY_TANGENT] = tangents_v4;
					}

					if (generated_tangents) {
						Ref<SurfaceTool> st;
						st.instance();
						array_copy[Mesh::ARRAY_INDEX] = erased_indices; //needed for tangent generation, erased by deindex
						st->create_from_triangle_arrays(array_copy);
						st->deindex();
						st->generate_tangents();
						array_copy = st->commit_to_arrays();
					}

					morphs.push_back(array_copy);
				}
			}
			VisualServer::ArrayFormat format_flags = (VisualServer::ArrayFormat)0;
			if (state.is_8_weights) {
				format_flags = VisualServer::ARRAY_FLAG_USE_8_WEIGHTS;
			}
			//just add it
			mesh.mesh->add_surface_from_arrays(primitive, array, morphs, format_flags);

			if (p.has("material")) {
				const int material = p["material"];
				ERR_FAIL_INDEX_V(material, state.materials.size(), ERR_FILE_CORRUPT);
				const Ref<Material> &mat = state.materials[material];

				mesh.mesh->surface_set_material(mesh.mesh->get_surface_count() - 1, mat);
			}
		}

		if (d.has("weights")) {
			const Array &weights = d["weights"];
			ERR_FAIL_COND_V(mesh.mesh->get_blend_shape_count() != weights.size(), ERR_PARSE_ERROR);
			mesh.blend_weights.resize(weights.size());
			for (int j = 0; j < weights.size(); j++) {
				mesh.blend_weights.write[j] = weights[j];
			}
		}

		state.meshes.push_back(mesh);
	}

	print_verbose("glTF: Total meshes: " + itos(state.meshes.size()));

	return OK;
}

Error EditorSceneImporterGLTF::_parse_images(GLTFState &state, const String &p_base_path) {

	if (!state.json.has("images"))
		return OK;

	const Array &images = state.json["images"];
	for (int i = 0; i < images.size(); i++) {

		const Dictionary &d = images[i];

		String mimetype;
		if (d.has("mimeType")) {
			mimetype = d["mimeType"];
		}

		Vector<uint8_t> data;
		const uint8_t *data_ptr = NULL;
		int data_size = 0;

		if (d.has("uri")) {
			String uri = d["uri"];

			if (uri.findn("data:application/octet-stream;base64") == 0 ||
					uri.findn("data:" + mimetype + ";base64") == 0) {
				//embedded data
				data = _parse_base64_uri(uri);
				data_ptr = data.ptr();
				data_size = data.size();
			} else {

				uri = p_base_path.plus_file(uri).replace("\\", "/"); //fix for windows
				Ref<Texture> texture = ResourceLoader::load(uri);
				state.images.push_back(texture);
				continue;
			}
		}

		if (d.has("bufferView")) {
			const GLTFBufferViewIndex bvi = d["bufferView"];

			ERR_FAIL_INDEX_V(bvi, state.buffer_views.size(), ERR_PARAMETER_RANGE_ERROR);

			const GLTFBufferView &bv = state.buffer_views[bvi];

			const GLTFBufferIndex bi = bv.buffer;
			ERR_FAIL_INDEX_V(bi, state.buffers.size(), ERR_PARAMETER_RANGE_ERROR);

			ERR_FAIL_COND_V(bv.byte_offset + bv.byte_length > state.buffers[bi].size(), ERR_FILE_CORRUPT);

			data_ptr = &state.buffers[bi][bv.byte_offset];
			data_size = bv.byte_length;
		}

		ERR_FAIL_COND_V(mimetype == "", ERR_FILE_CORRUPT);

		if (mimetype.findn("png") != -1) {
			//is a png
			const Ref<Image> img = Image::_png_mem_loader_func(data_ptr, data_size);

			ERR_FAIL_COND_V(img.is_null(), ERR_FILE_CORRUPT);

			Ref<ImageTexture> t;
			t.instance();
			t->create_from_image(img);

			state.images.push_back(t);
			continue;
		}

		if (mimetype.findn("jpeg") != -1) {
			//is a jpg
			const Ref<Image> img = Image::_jpg_mem_loader_func(data_ptr, data_size);

			ERR_FAIL_COND_V(img.is_null(), ERR_FILE_CORRUPT);

			Ref<ImageTexture> t;
			t.instance();
			t->create_from_image(img);

			state.images.push_back(t);

			continue;
		}

		ERR_FAIL_V(ERR_FILE_CORRUPT);
	}

	print_verbose("Total images: " + itos(state.images.size()));

	return OK;
}

Error EditorSceneImporterGLTF::_parse_textures(GLTFState &state) {

	if (!state.json.has("textures"))
		return OK;

	const Array &textures = state.json["textures"];
	for (GLTFTextureIndex i = 0; i < textures.size(); i++) {

		const Dictionary &d = textures[i];

		ERR_FAIL_COND_V(!d.has("source"), ERR_PARSE_ERROR);

		GLTFTexture t;
		t.src_image = d["source"];
		state.textures.push_back(t);
	}

	return OK;
}

Ref<Texture> EditorSceneImporterGLTF::_get_texture(GLTFState &state, const GLTFTextureIndex p_texture) {
	ERR_FAIL_INDEX_V(p_texture, state.textures.size(), Ref<Texture>());
	const GLTFImageIndex image = state.textures[p_texture].src_image;

	ERR_FAIL_INDEX_V(image, state.images.size(), Ref<Texture>());

	return state.images[image];
}

Error EditorSceneImporterGLTF::_parse_materials(GLTFState &state) {

	if (!state.json.has("materials"))
		return OK;

	const Array &materials = state.json["materials"];
	for (GLTFMaterialIndex i = 0; i < materials.size(); i++) {

		const Dictionary &d = materials[i];

		Ref<SpatialMaterial> material;
		material.instance();
		if (d.has("name")) {
			material->set_name(d["name"]);
		}

		if (d.has("pbrMetallicRoughness")) {

			const Dictionary &mr = d["pbrMetallicRoughness"];
			if (mr.has("baseColorFactor")) {
				const Array &arr = mr["baseColorFactor"];
				ERR_FAIL_COND_V(arr.size() != 4, ERR_PARSE_ERROR);
				const Color c = Color(arr[0], arr[1], arr[2], arr[3]).to_srgb();

				material->set_albedo(c);
			}

			if (mr.has("baseColorTexture")) {
				const Dictionary &bct = mr["baseColorTexture"];
				if (bct.has("index")) {
					material->set_texture(SpatialMaterial::TEXTURE_ALBEDO, _get_texture(state, bct["index"]));
				}
				if (!mr.has("baseColorFactor")) {
					material->set_albedo(Color(1, 1, 1));
				}
			}

			if (mr.has("metallicFactor")) {
				material->set_metallic(mr["metallicFactor"]);
			} else {
				material->set_metallic(1.0);
			}

			if (mr.has("roughnessFactor")) {
				material->set_roughness(mr["roughnessFactor"]);
			} else {
				material->set_roughness(1.0);
			}

			if (mr.has("metallicRoughnessTexture")) {
				const Dictionary &bct = mr["metallicRoughnessTexture"];
				if (bct.has("index")) {
					const Ref<Texture> t = _get_texture(state, bct["index"]);
					material->set_texture(SpatialMaterial::TEXTURE_METALLIC, t);
					material->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_BLUE);
					material->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, t);
					material->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GREEN);
					if (!mr.has("metallicFactor")) {
						material->set_metallic(1);
					}
					if (!mr.has("roughnessFactor")) {
						material->set_roughness(1);
					}
				}
			}
		}

		if (d.has("normalTexture")) {
			const Dictionary &bct = d["normalTexture"];
			if (bct.has("index")) {
				material->set_texture(SpatialMaterial::TEXTURE_NORMAL, _get_texture(state, bct["index"]));
				material->set_feature(SpatialMaterial::FEATURE_NORMAL_MAPPING, true);
			}
			if (bct.has("scale")) {
				material->set_normal_scale(bct["scale"]);
			}
		}
		if (d.has("occlusionTexture")) {
			const Dictionary &bct = d["occlusionTexture"];
			if (bct.has("index")) {
				material->set_texture(SpatialMaterial::TEXTURE_AMBIENT_OCCLUSION, _get_texture(state, bct["index"]));
				material->set_ao_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_RED);
				material->set_feature(SpatialMaterial::FEATURE_AMBIENT_OCCLUSION, true);
			}
		}

		if (d.has("emissiveFactor")) {
			const Array &arr = d["emissiveFactor"];
			ERR_FAIL_COND_V(arr.size() != 3, ERR_PARSE_ERROR);
			const Color c = Color(arr[0], arr[1], arr[2]).to_srgb();
			material->set_feature(SpatialMaterial::FEATURE_EMISSION, true);

			material->set_emission(c);
		}

		if (d.has("emissiveTexture")) {
			const Dictionary &bct = d["emissiveTexture"];
			if (bct.has("index")) {
				material->set_texture(SpatialMaterial::TEXTURE_EMISSION, _get_texture(state, bct["index"]));
				material->set_feature(SpatialMaterial::FEATURE_EMISSION, true);
				material->set_emission(Color(0, 0, 0));
			}
		}

		if (d.has("doubleSided")) {
			const bool ds = d["doubleSided"];
			if (ds) {
				material->set_cull_mode(SpatialMaterial::CULL_DISABLED);
			}
		}

		if (d.has("alphaMode")) {
			const String &am = d["alphaMode"];
			if (am != "OPAQUE") {
				material->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
				material->set_depth_draw_mode(SpatialMaterial::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
			}
		}

		state.materials.push_back(material);
	}

	print_verbose("Total materials: " + itos(state.materials.size()));

	return OK;
}

EditorSceneImporterGLTF::GLTFNodeIndex EditorSceneImporterGLTF::_find_highest_node(GLTFState &state, const Vector<GLTFNodeIndex> &subset) {
	int heighest = -1;
	GLTFNodeIndex best_node = -1;

	for (int i = 0; i < subset.size(); ++i) {
		const GLTFNodeIndex node_i = subset[i];
		const GLTFNode *node = state.nodes[node_i];

		if (heighest == -1 || node->height < heighest) {
			heighest = node->height;
			best_node = node_i;
		}
	}

	return best_node;
}

bool EditorSceneImporterGLTF::_capture_nodes_in_skin(GLTFState &state, GLTFSkin &skin, const GLTFNodeIndex node_index) {

	bool found_joint = false;

	for (int i = 0; i < state.nodes[node_index]->children.size(); ++i) {
		found_joint |= _capture_nodes_in_skin(state, skin, state.nodes[node_index]->children[i]);
	}

	if (found_joint) {
		// Mark it if we happen to find another skins joint...
		if (state.nodes[node_index]->joint && skin.joints.find(node_index) < 0) {
			skin.joints.push_back(node_index);
		} else if (skin.non_joints.find(node_index) < 0) {
			skin.non_joints.push_back(node_index);
		}
	}

	if (skin.joints.find(node_index) > 0) {
		return true;
	}

	return false;
}

void EditorSceneImporterGLTF::_capture_nodes_for_multirooted_skin(GLTFState &state, GLTFSkin &skin) {

	DisjointSet<GLTFNodeIndex> disjoint_set;

	for (int i = 0; i < skin.joints.size(); ++i) {
		const GLTFNodeIndex node_index = skin.joints[i];
		const GLTFNodeIndex parent = state.nodes[node_index]->parent;
		disjoint_set.insert(node_index);

		if (skin.joints.find(parent) >= 0) {
			disjoint_set.create_union(parent, node_index);
		}
	}

	Vector<GLTFNodeIndex> roots;
	disjoint_set.get_representatives(roots);

	if (roots.size() <= 1) {
		return;
	}

	int maxHeight = -1;

	// Determine the max height rooted tree
	for (int i = 0; i < roots.size(); ++i) {
		const GLTFNodeIndex root = roots[i];

		if (maxHeight == -1 || state.nodes[root]->height < maxHeight) {
			maxHeight = state.nodes[root]->height;
		}
	}

	// Go up the tree till all of the multiple roots of the skin are at the same hierarchy level.
	// This sucks, but 99% of all game engines (not just Godot) would have this same issue.
	for (int i = 0; i < roots.size(); ++i) {

		GLTFNodeIndex current_node = roots[i];
		while (state.nodes[current_node]->height > maxHeight) {
			GLTFNodeIndex parent = state.nodes[current_node]->parent;

			if (state.nodes[parent]->joint && skin.joints.find(parent) < 0) {
				skin.joints.push_back(parent);
			} else if (skin.non_joints.find(parent) < 0) {
				skin.non_joints.push_back(parent);
			}

			current_node = parent;
		}

		// replace the roots
		roots.write[i] = current_node;
	}

	// Climb up the tree until they all have the same parent
	bool all_same;

	do {
		all_same = true;
		const GLTFNodeIndex first_parent = state.nodes[roots[0]]->parent;

		for (int i = 1; i < roots.size(); ++i) {
			all_same &= (first_parent == state.nodes[roots[i]]->parent);
		}

		if (!all_same) {
			for (int i = 0; i < roots.size(); ++i) {
				const GLTFNodeIndex current_node = roots[i];
				const GLTFNodeIndex parent = state.nodes[current_node]->parent;

				if (state.nodes[parent]->joint && skin.joints.find(parent) < 0) {
					skin.joints.push_back(parent);
				} else if (skin.non_joints.find(parent) < 0) {
					skin.non_joints.push_back(parent);
				}

				roots.write[i] = parent;
			}
		}

	} while (!all_same);
}

Error EditorSceneImporterGLTF::_expand_skin(GLTFState &state, GLTFSkin &skin) {

	_capture_nodes_for_multirooted_skin(state, skin);

	// Grab all nodes that lay in between skin joints/nodes
	DisjointSet<GLTFNodeIndex> disjoint_set;

	Vector<GLTFNodeIndex> all_skin_nodes;
	all_skin_nodes.append_array(skin.joints);
	all_skin_nodes.append_array(skin.non_joints);

	for (int i = 0; i < all_skin_nodes.size(); ++i) {
		const GLTFNodeIndex node_index = all_skin_nodes[i];
		const GLTFNodeIndex parent = state.nodes[node_index]->parent;
		disjoint_set.insert(node_index);

		if (all_skin_nodes.find(parent) >= 0) {
			disjoint_set.create_union(parent, node_index);
		}
	}

	Vector<GLTFNodeIndex> out_owners;
	disjoint_set.get_representatives(out_owners);

	Vector<GLTFNodeIndex> out_roots;

	for (int i = 0; i < out_owners.size(); ++i) {
		Vector<GLTFNodeIndex> set;
		disjoint_set.get_members(set, out_owners[i]);

		const GLTFNodeIndex root = _find_highest_node(state, set);
		ERR_FAIL_COND_V(root < 0, FAILED);
		out_roots.push_back(root);
	}

	out_roots.sort();

	for (int i = 0; i < out_roots.size(); ++i) {
		_capture_nodes_in_skin(state, skin, out_roots[i]);
	}

	skin.roots = out_roots;

	return OK;
}

Error EditorSceneImporterGLTF::_verify_skin(GLTFState &state, GLTFSkin &skin) {

	// This may seem duplicated from expand_skins, but this is really a sanity check! (so it kinda is)
	// In case additional interpolating logic is added to the skins, this will help ensure that you
	// do not cause it to self implode into a fiery blaze

	// We are going to re-calculate the root nodes and compare them to the ones saved in the skin,
	// then ensure the multiple trees (if they exist) are on the same sublevel

	// Grab all nodes that lay in between skin joints/nodes
	DisjointSet<GLTFNodeIndex> disjoint_set;

	Vector<GLTFNodeIndex> all_skin_nodes;
	all_skin_nodes.append_array(skin.joints);
	all_skin_nodes.append_array(skin.non_joints);

	for (int i = 0; i < all_skin_nodes.size(); ++i) {
		const GLTFNodeIndex node_index = all_skin_nodes[i];
		const GLTFNodeIndex parent = state.nodes[node_index]->parent;
		disjoint_set.insert(node_index);

		if (all_skin_nodes.find(parent) >= 0) {
			disjoint_set.create_union(parent, node_index);
		}
	}

	Vector<GLTFNodeIndex> out_owners;
	disjoint_set.get_representatives(out_owners);

	Vector<GLTFNodeIndex> out_roots;

	for (int i = 0; i < out_owners.size(); ++i) {
		Vector<GLTFNodeIndex> set;
		disjoint_set.get_members(set, out_owners[i]);

		const GLTFNodeIndex root = _find_highest_node(state, set);
		ERR_FAIL_COND_V(root < 0, FAILED);
		out_roots.push_back(root);
	}

	out_roots.sort();

	ERR_FAIL_COND_V(out_roots.size() == 0, FAILED);

	// Make sure the roots are the exact same (they better be)
	ERR_FAIL_COND_V(out_roots.size() != skin.roots.size(), FAILED);
	for (int i = 0; i < out_roots.size(); ++i) {
		ERR_FAIL_COND_V(out_roots[i] != skin.roots[i], FAILED);
	}

	// Single rooted skin? Perfectly ok!
	if (out_roots.size() == 1) {
		return OK;
	}

	// Make sure all parents of a multi-rooted skin are the SAME
	const GLTFNodeIndex parent = state.nodes[out_roots[0]]->parent;
	for (int i = 1; i < out_roots.size(); ++i) {
		if (state.nodes[out_roots[i]]->parent != parent) {
			return FAILED;
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_parse_skins(GLTFState &state) {

	if (!state.json.has("skins"))
		return OK;

	const Array &skins = state.json["skins"];

	// Create the base skins, and mark nodes that are joints
	for (int i = 0; i < skins.size(); i++) {

		const Dictionary &d = skins[i];

		GLTFSkin skin;

		ERR_FAIL_COND_V(!d.has("joints"), ERR_PARSE_ERROR);

		const Array &joints = d["joints"];

		if (d.has("inverseBindMatrices")) {
			skin.inverse_binds = _decode_accessor_as_xform(state, d["inverseBindMatrices"], false);
			ERR_FAIL_COND_V(skin.inverse_binds.size() != joints.size(), ERR_PARSE_ERROR);
		}

		for (int j = 0; j < joints.size(); j++) {
			const GLTFNodeIndex node = joints[j];
			ERR_FAIL_INDEX_V(node, state.nodes.size(), ERR_PARSE_ERROR);

			skin.joints.push_back(node);
			skin.joints_original.push_back(node);

			state.nodes[node]->joint = true;
		}

		if (d.has("name")) {
			skin.name = d["name"];
		}

		if (d.has("skeleton")) {
			skin.skin_root = d["skeleton"];
		}

		state.skins.push_back(skin);
	}

	for (GLTFSkinIndex i = 0; i < state.skins.size(); ++i) {
		GLTFSkin &skin = state.skins.write[i];

		// Expand the skin to capture all the extra non-joints that lie in between the actual joints,
		// and expand the hierarchy to ensure multi-rooted trees lie on the same height level
		ERR_FAIL_COND_V(_expand_skin(state, skin), ERR_PARSE_ERROR);
		ERR_FAIL_COND_V(_verify_skin(state, skin), ERR_PARSE_ERROR);
	}

	print_verbose("glTF: Total skins: " + itos(state.skins.size()));

	return OK;
}

Error EditorSceneImporterGLTF::_determine_skeletons(GLTFState &state) {

	// Using a disjoint set, we are going to potentially combine all skins that are actually branches
	// of a main skeleton, or treat skins defining the same set of nodes as ONE skeleton.
	// This is another unclear issue caused by the current glTF specification.

	DisjointSet<GLTFNodeIndex> skeleton_sets;

	for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
		const GLTFSkin &skin = state.skins[skin_i];

		Vector<GLTFNodeIndex> all_skin_nodes;
		all_skin_nodes.append_array(skin.joints);
		all_skin_nodes.append_array(skin.non_joints);

		for (int i = 0; i < all_skin_nodes.size(); ++i) {
			const GLTFNodeIndex node_index = all_skin_nodes[i];
			const GLTFNodeIndex parent = state.nodes[node_index]->parent;
			skeleton_sets.insert(node_index);

			if (all_skin_nodes.find(parent) >= 0) {
				skeleton_sets.create_union(parent, node_index);
			}
		}

		// We are going to connect the separate skin subtrees in each skin together
		// so that the final roots are entire sets of valid skin trees
		for (int i = 1; i < skin.roots.size(); ++i) {
			skeleton_sets.create_union(skin.roots[0], skin.roots[i]);
		}
	}

	{ // attempt to joint all touching subsets (siblings/parent are part of another skin)
		Vector<GLTFNodeIndex> groups_representatives;
		skeleton_sets.get_representatives(groups_representatives);

		Vector<GLTFNodeIndex> highest_group_members;
		Vector<Vector<GLTFNodeIndex> > groups;
		for (int i = 0; i < groups_representatives.size(); ++i) {
			Vector<GLTFNodeIndex> group;
			skeleton_sets.get_members(group, groups_representatives[i]);
			highest_group_members.push_back(_find_highest_node(state, group));
			groups.push_back(group);
		}

		for (int i = 0; i < highest_group_members.size(); ++i) {
			const GLTFNodeIndex node_i = highest_group_members[i];

			// Attach any siblings together (this needs to be done n^2/2 times)
			for (int j = i + 1; j < highest_group_members.size(); ++j) {
				const GLTFNodeIndex node_j = highest_group_members[j];

				// Even if they are siblings under the root! :)
				if (state.nodes[node_i]->parent == state.nodes[node_j]->parent) {
					skeleton_sets.create_union(node_i, node_j);
				}
			}

			// Attach any parenting going on together (we need to do this n^2 times)
			const GLTFNodeIndex node_i_parent = state.nodes[node_i]->parent;
			if (node_i_parent >= 0) {
				for (int j = 0; j < groups.size() && i != j; ++j) {
					const Vector<GLTFNodeIndex> &group = groups[j];

					if (group.find(node_i_parent) >= 0) {
						const GLTFNodeIndex node_j = highest_group_members[j];
						skeleton_sets.create_union(node_i, node_j);
					}
				}
			}
		}
	}

	// At this point, the skeleton groups should be finalized
	Vector<GLTFNodeIndex> skeleton_owners;
	skeleton_sets.get_representatives(skeleton_owners);

	// Mark all the skins actual skeletons, after we have merged them
	for (GLTFSkeletonIndex skel_i = 0; skel_i < skeleton_owners.size(); ++skel_i) {

		const GLTFNodeIndex skeleton_owner = skeleton_owners[skel_i];
		GLTFSkeleton skeleton;

		Vector<GLTFNodeIndex> skeleton_nodes;
		skeleton_sets.get_members(skeleton_nodes, skeleton_owner);

		for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
			GLTFSkin &skin = state.skins.write[skin_i];

			// If any of the the skeletons nodes exist in a skin, that skin now maps to the skeleton
			for (int i = 0; i < skeleton_nodes.size(); ++i) {
				GLTFNodeIndex skel_node_i = skeleton_nodes[i];
				if (skin.joints.find(skel_node_i) >= 0 || skin.non_joints.find(skel_node_i) >= 0) {
					skin.skeleton = skel_i;
					continue;
				}
			}
		}

		Vector<GLTFNodeIndex> non_joints;
		for (int i = 0; i < skeleton_nodes.size(); ++i) {
			const GLTFNodeIndex node_i = skeleton_nodes[i];

			if (state.nodes[node_i]->joint) {
				skeleton.joints.push_back(node_i);
			} else {
				non_joints.push_back(node_i);
			}
		}

		state.skeletons.push_back(skeleton);

		_reparent_non_joint_skeleton_subtrees(state, state.skeletons.write[skel_i], non_joints);
	}

	for (GLTFSkeletonIndex skel_i = 0; skel_i < state.skeletons.size(); ++skel_i) {
		GLTFSkeleton &skeleton = state.skeletons.write[skel_i];

		for (int i = 0; i < skeleton.joints.size(); ++i) {
			const GLTFNodeIndex node_i = skeleton.joints[i];
			GLTFNode *node = state.nodes[node_i];

			ERR_FAIL_COND_V(!node->joint, ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(node->skeleton >= 0, ERR_PARSE_ERROR);
			node->skeleton = skel_i;
		}

		ERR_FAIL_COND_V(_determine_skeleton_roots(state, skel_i), ERR_PARSE_ERROR);
	}

	return OK;
}

Error EditorSceneImporterGLTF::_reparent_non_joint_skeleton_subtrees(GLTFState &state, GLTFSkeleton &skeleton, const Vector<GLTFNodeIndex> &non_joints) {

	DisjointSet<GLTFNodeIndex> subtree_set;

	// Populate the disjoint set with ONLY non joints that are in the skeleton hierarchy (non_joints vector)
	// This way we can find any joints that lie in between joints, as the current glTF specification
	// mentions nothing about non-joints being in between joints of the same skin. Hopefully one day we
	// can remove this code.

	// skinD depicted here explains this issue:
	// https://github.com/KhronosGroup/glTF-Asset-Generator/blob/master/Output/Positive/Animation_Skin

	for (int i = 0; i < non_joints.size(); ++i) {
		const GLTFNodeIndex node_i = non_joints[i];

		subtree_set.insert(node_i);

		const GLTFNodeIndex parent_i = state.nodes[node_i]->parent;
		if (parent_i >= 0 && non_joints.find(parent_i) >= 0 && !state.nodes[parent_i]->joint) {
			subtree_set.create_union(parent_i, node_i);
		}
	}

	// Find all the non joint subtrees and re-parent them to a new "fake" joint

	Vector<GLTFNodeIndex> non_joint_subtree_roots;
	subtree_set.get_representatives(non_joint_subtree_roots);

	for (int root_i = 0; root_i < non_joint_subtree_roots.size(); ++root_i) {
		const GLTFNodeIndex subtree_root = non_joint_subtree_roots[root_i];

		Vector<GLTFNodeIndex> subtree_nodes;
		subtree_set.get_members(subtree_nodes, subtree_root);

		for (int subtree_i = 0; subtree_i < subtree_nodes.size(); ++subtree_i) {
			ERR_FAIL_COND_V(_reparent_to_fake_joint(state, skeleton, subtree_nodes[subtree_i]), FAILED);

			// We modified the tree, recompute all the heights
			_compute_node_heights(state);
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_reparent_to_fake_joint(GLTFState &state, GLTFSkeleton &skeleton, const GLTFNodeIndex node_index) {
	GLTFNode *node = state.nodes[node_index];

	// Can we just "steal" this joint if it is just a spatial node?
	if (node->skin < 0 && node->mesh < 0 && node->camera < 0) {
		node->joint = true;
		// Add the joint to the skeletons joints
		skeleton.joints.push_back(node_index);
		return OK;
	}

	GLTFNode *fake_joint = memnew(GLTFNode);
	const GLTFNodeIndex fake_joint_index = state.nodes.size();
	state.nodes.push_back(fake_joint);

	// We better not be a joint, or we messed up in our logic
	if (node->joint)
		return FAILED;

	fake_joint->translation = node->translation;
	fake_joint->rotation = node->rotation;
	fake_joint->scale = node->scale;
	fake_joint->xform = node->xform;
	fake_joint->joint = true;

	// We can use the exact same name here, because the joint will be inside a skeleton and not the scene
	fake_joint->name = node->name;

	// Clear the nodes transforms, since it will be parented to the fake joint
	node->translation = Vector3(0, 0, 0);
	node->rotation = Quat();
	node->scale = Vector3(1, 1, 1);
	node->xform = Transform();

	// Transfer the node children to the fake joint
	for (int child_i = 0; child_i < node->children.size(); ++child_i) {
		GLTFNode *child = state.nodes[node->children[child_i]];
		child->parent = fake_joint_index;
	}

	fake_joint->children = node->children;
	node->children.clear();

	// add the fake joint to the parent and remove the original joint
	if (node->parent >= 0) {
		GLTFNode *parent = state.nodes[node->parent];
		parent->children.erase(node_index);
		parent->children.push_back(fake_joint_index);
		fake_joint->parent = node->parent;
	}

	// Add the node to the fake joint
	fake_joint->children.push_back(node_index);
	node->parent = fake_joint_index;
	node->fake_joint_parent = fake_joint_index;

	// Add the fake joint to the skeletons joints
	skeleton.joints.push_back(fake_joint_index);

	// Replace skin_skeletons with fake joints if we must.
	for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
		GLTFSkin &skin = state.skins.write[skin_i];
		if (skin.skin_root == node_index) {
			skin.skin_root = fake_joint_index;
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_determine_skeleton_roots(GLTFState &state, const GLTFSkeletonIndex skel_i) {

	DisjointSet<GLTFNodeIndex> disjoint_set;

	for (GLTFNodeIndex i = 0; i < state.nodes.size(); ++i) {
		const GLTFNode *node = state.nodes[i];

		if (node->skeleton != skel_i) {
			continue;
		}

		disjoint_set.insert(i);

		if (node->parent >= 0 && state.nodes[node->parent]->skeleton == skel_i) {
			disjoint_set.create_union(node->parent, i);
		}
	}

	GLTFSkeleton &skeleton = state.skeletons.write[skel_i];

	Vector<GLTFNodeIndex> owners;
	disjoint_set.get_representatives(owners);

	Vector<GLTFNodeIndex> roots;

	for (int i = 0; i < owners.size(); ++i) {
		Vector<GLTFNodeIndex> set;
		disjoint_set.get_members(set, owners[i]);
		const GLTFNodeIndex root = _find_highest_node(state, set);
		ERR_FAIL_COND_V(root < 0, FAILED);
		roots.push_back(root);
	}

	roots.sort();

	skeleton.roots = roots;

	if (roots.size() == 0) {
		return FAILED;
	} else if (roots.size() == 1) {
		return OK;
	}

	// Check that the subtrees have the same parent root
	const GLTFNodeIndex parent = state.nodes[roots[0]]->parent;
	for (int i = 1; i < roots.size(); ++i) {
		if (state.nodes[roots[i]]->parent != parent) {
			return FAILED;
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_create_skeletons(GLTFState &state) {
	for (GLTFSkeletonIndex skel_i = 0; skel_i < state.skeletons.size(); ++skel_i) {

		GLTFSkeleton &gltf_skeleton = state.skeletons.write[skel_i];

		Skeleton *skeleton = memnew(Skeleton);
		gltf_skeleton.godot_skeleton = skeleton;

		// Make a unique name, no gltf node represents this skeleton
		skeleton->set_name(_gen_unique_name(state, "Skeleton"));

		List<GLTFNodeIndex> bones;

		for (int i = 0; i < gltf_skeleton.roots.size(); ++i) {
			bones.push_back(gltf_skeleton.roots[i]);
		}

		// Make the skeleton creation deterministic by going through the roots in
		// a sorted order, and DEPTH FIRST
		bones.sort();

		while (!bones.empty()) {
			const GLTFNodeIndex node_i = bones.front()->get();
			bones.pop_front();

			GLTFNode *node = state.nodes[node_i];
			ERR_FAIL_COND_V(node->skeleton != skel_i, FAILED);

			{ // Add all child nodes to the stack (deterministically)
				Vector<GLTFNodeIndex> child_nodes;
				for (int i = 0; i < node->children.size(); ++i) {
					const GLTFNodeIndex child_i = node->children[i];
					if (state.nodes[child_i]->skeleton == skel_i) {
						child_nodes.push_back(child_i);
					}
				}

				// Depth first insertion
				child_nodes.sort();
				for (int i = child_nodes.size() - 1; i >= 0; --i) {
					bones.push_front(child_nodes[i]);
				}
			}

			const int bone_index = skeleton->get_bone_count();

			if (node->name.empty()) {
				node->name = "bone";
			}

			node->name = _gen_unique_bone_name(state, skel_i, node->name);

			skeleton->add_bone(node->name);
			skeleton->set_bone_rest(bone_index, node->xform);
			skeleton->set_bone_pose(bone_index, node->xform);

			if (node->parent >= 0 && state.nodes[node->parent]->skeleton == skel_i) {
				const int bone_parent = skeleton->find_bone(state.nodes[node->parent]->name);
				ERR_FAIL_COND_V(bone_parent < 0, FAILED);
				skeleton->set_bone_parent(bone_index, skeleton->find_bone(state.nodes[node->parent]->name));
			}

			state.scene_nodes.insert(node_i, skeleton);
		}
	}

	ERR_FAIL_COND_V(_map_skin_joints_indices_to_skeleton_bone_indices(state), ERR_PARSE_ERROR);

	return OK;
}

Error EditorSceneImporterGLTF::_map_skin_joints_indices_to_skeleton_bone_indices(GLTFState &state) {
	for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
		GLTFSkin &skin = state.skins.write[skin_i];

		const GLTFSkeleton &skeleton = state.skeletons[skin.skeleton];

		for (int joint_index = 0; joint_index < skin.joints_original.size(); ++joint_index) {
			const GLTFNodeIndex node_i = skin.joints_original[joint_index];
			const GLTFNode *node = state.nodes[node_i];

			const int bone_index = skeleton.godot_skeleton->find_bone(node->name);
			ERR_FAIL_COND_V(bone_index < 0, FAILED);

			skin.joint_i_to_bone_i.insert(joint_index, bone_index);
		}
	}

	return OK;
}

Error EditorSceneImporterGLTF::_create_skins(GLTFState &state) {
	for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
		GLTFSkin &gltf_skin = state.skins.write[skin_i];

		Ref<Skin> skin;
		skin.instance();

		// Some skins don't have IBM's! What absolute monsters!
		const bool has_ibms = !gltf_skin.inverse_binds.empty();

		for (int joint_i = 0; joint_i < gltf_skin.joints_original.size(); ++joint_i) {
			int bone_i = gltf_skin.joint_i_to_bone_i[joint_i];

			if (has_ibms) {
				skin->add_bind(bone_i, gltf_skin.inverse_binds[joint_i]);
			} else {
				skin->add_bind(bone_i, Transform());
			}
		}

		gltf_skin.godot_skin = skin;
	}

	// Purge the duplicates!
	_remove_duplicate_skins(state);

	// Create unique names now, after removing duplicates
	for (GLTFSkinIndex skin_i = 0; skin_i < state.skins.size(); ++skin_i) {
		Ref<Skin> skin = state.skins[skin_i].godot_skin;
		if (skin->get_name().empty()) {
			// Make a unique name, no gltf node represents this skin
			skin->set_name(_gen_unique_name(state, "Skin"));
		}
	}

	return OK;
}

bool EditorSceneImporterGLTF::_skins_are_same(const Ref<Skin> &skin_a, const Ref<Skin> &skin_b) {
	if (skin_a->get_bind_count() != skin_b->get_bind_count()) {
		return false;
	}

	for (int i = 0; i < skin_a->get_bind_count(); ++i) {

		if (skin_a->get_bind_bone(i) != skin_b->get_bind_bone(i)) {
			return false;
		}

		Transform a_xform = skin_a->get_bind_pose(i);
		Transform b_xform = skin_b->get_bind_pose(i);

		if (a_xform != b_xform) {
			return false;
		}
	}

	return true;
}

void EditorSceneImporterGLTF::_remove_duplicate_skins(GLTFState &state) {
	for (int i = 0; i < state.skins.size(); ++i) {
		for (int j = i + 1; j < state.skins.size(); ++j) {
			const Ref<Skin> &skin_i = state.skins[i].godot_skin;
			const Ref<Skin> &skin_j = state.skins[j].godot_skin;

			if (_skins_are_same(skin_i, skin_j)) {
				// replace it and delete the old
				state.skins.write[j].godot_skin = skin_i;
			}
		}
	}
}

Error EditorSceneImporterGLTF::_parse_cameras(GLTFState &state) {

	if (!state.json.has("cameras"))
		return OK;

	const Array &cameras = state.json["cameras"];

	for (GLTFCameraIndex i = 0; i < cameras.size(); i++) {

		const Dictionary &d = cameras[i];

		GLTFCamera camera;
		ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
		const String &type = d["type"];
		if (type == "orthographic") {

			camera.perspective = false;
			if (d.has("orthographic")) {
				const Dictionary &og = d["orthographic"];
				camera.fov_size = og["ymag"];
				camera.zfar = og["zfar"];
				camera.znear = og["znear"];
			} else {
				camera.fov_size = 10;
			}

		} else if (type == "perspective") {

			camera.perspective = true;
			if (d.has("perspective")) {
				const Dictionary &ppt = d["perspective"];
				// GLTF spec is in radians, Godot's camera is in degrees.
				camera.fov_size = (double)ppt["yfov"] * 180.0 / Math_PI;
				camera.zfar = ppt["zfar"];
				camera.znear = ppt["znear"];
			} else {
				camera.fov_size = 10;
			}
		} else {
			ERR_FAIL_V_MSG(ERR_PARSE_ERROR, "Camera should be in 'orthographic' or 'perspective'");
		}

		state.cameras.push_back(camera);
	}

	print_verbose("glTF: Total cameras: " + itos(state.cameras.size()));

	return OK;
}

Error EditorSceneImporterGLTF::_parse_animations(GLTFState &state) {

	if (!state.json.has("animations"))
		return OK;

	const Array &animations = state.json["animations"];

	for (GLTFAnimationIndex i = 0; i < animations.size(); i++) {

		const Dictionary &d = animations[i];

		GLTFAnimation animation;

		if (!d.has("channels") || !d.has("samplers"))
			continue;

		Array channels = d["channels"];
		Array samplers = d["samplers"];

		if (d.has("name")) {
			animation.name = d["name"];
		}

		for (int j = 0; j < channels.size(); j++) {

			const Dictionary &c = channels[j];
			if (!c.has("target"))
				continue;

			const Dictionary &t = c["target"];
			if (!t.has("node") || !t.has("path")) {
				continue;
			}

			ERR_FAIL_COND_V(!c.has("sampler"), ERR_PARSE_ERROR);
			const int sampler = c["sampler"];
			ERR_FAIL_INDEX_V(sampler, samplers.size(), ERR_PARSE_ERROR);

			GLTFNodeIndex node = t["node"];
			String path = t["path"];

			ERR_FAIL_INDEX_V(node, state.nodes.size(), ERR_PARSE_ERROR);

			GLTFAnimation::Track *track = nullptr;

			if (!animation.tracks.has(node)) {
				animation.tracks[node] = GLTFAnimation::Track();
			}

			track = &animation.tracks[node];

			const Dictionary &s = samplers[sampler];

			ERR_FAIL_COND_V(!s.has("input"), ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(!s.has("output"), ERR_PARSE_ERROR);

			const int input = s["input"];
			const int output = s["output"];

			GLTFAnimation::Interpolation interp = GLTFAnimation::INTERP_LINEAR;
			if (s.has("interpolation")) {
				const String &in = s["interpolation"];
				if (in == "STEP") {
					interp = GLTFAnimation::INTERP_STEP;
				} else if (in == "LINEAR") {
					interp = GLTFAnimation::INTERP_LINEAR;
				} else if (in == "CATMULLROMSPLINE") {
					interp = GLTFAnimation::INTERP_CATMULLROMSPLINE;
				} else if (in == "CUBICSPLINE") {
					interp = GLTFAnimation::INTERP_CUBIC_SPLINE;
				}
			}

			const PoolVector<float> times = _decode_accessor_as_floats(state, input, false);
			if (path == "translation") {
				const PoolVector<Vector3> translations = _decode_accessor_as_vec3(state, output, false);
				track->translation_track.interpolation = interp;
				track->translation_track.times = Variant(times); //convert via variant
				track->translation_track.values = Variant(translations); //convert via variant
			} else if (path == "rotation") {
				const Vector<Quat> rotations = _decode_accessor_as_quat(state, output, false);
				track->rotation_track.interpolation = interp;
				track->rotation_track.times = Variant(times); //convert via variant
				track->rotation_track.values = rotations; //convert via variant
			} else if (path == "scale") {
				const PoolVector<Vector3> scales = _decode_accessor_as_vec3(state, output, false);
				track->scale_track.interpolation = interp;
				track->scale_track.times = Variant(times); //convert via variant
				track->scale_track.values = Variant(scales); //convert via variant
			} else if (path == "weights") {
				const PoolVector<float> weights = _decode_accessor_as_floats(state, output, false);

				ERR_FAIL_INDEX_V(state.nodes[node]->mesh, state.meshes.size(), ERR_PARSE_ERROR);
				const GLTFMesh *mesh = &state.meshes[state.nodes[node]->mesh];
				ERR_FAIL_COND_V(mesh->blend_weights.size() == 0, ERR_PARSE_ERROR);
				const int wc = mesh->blend_weights.size();

				track->weight_tracks.resize(wc);

				const int wlen = weights.size() / wc;
				PoolVector<float>::Read r = weights.read();
				for (int k = 0; k < wc; k++) { //separate tracks, having them together is not such a good idea
					GLTFAnimation::Channel<float> cf;
					cf.interpolation = interp;
					cf.times = Variant(times);
					Vector<float> wdata;
					wdata.resize(wlen);
					for (int l = 0; l < wlen; l++) {
						wdata.write[l] = r[l * wc + k];
					}

					cf.values = wdata;
					track->weight_tracks.write[k] = cf;
				}
			} else {
				WARN_PRINTS("Invalid path '" + path + "'.");
			}
		}

		state.animations.push_back(animation);
	}

	print_verbose("glTF: Total animations '" + itos(state.animations.size()) + "'.");

	return OK;
}

void EditorSceneImporterGLTF::_assign_scene_names(GLTFState &state) {

	for (int i = 0; i < state.nodes.size(); i++) {
		GLTFNode *n = state.nodes[i];

		// Any joints get unique names generated when the skeleton is made, unique to the skeleton
		if (n->skeleton >= 0)
			continue;

		if (n->name.empty()) {
			if (n->mesh >= 0) {
				n->name = "Mesh";
			} else if (n->camera >= 0) {
				n->name = "Camera";
			} else {
				n->name = "Node";
			}
		}

		n->name = _gen_unique_name(state, n->name);
	}
}

BoneAttachment *EditorSceneImporterGLTF::_generate_bone_attachment(GLTFState &state, Skeleton *skeleton, const GLTFNodeIndex node_index) {

	const GLTFNode *gltf_node = state.nodes[node_index];
	const GLTFNode *bone_node = state.nodes[gltf_node->parent];

	BoneAttachment *bone_attachment = memnew(BoneAttachment);
	print_verbose("glTF: Creating bone attachment for: " + gltf_node->name);

	ERR_FAIL_COND_V(!bone_node->joint, nullptr);

	bone_attachment->set_bone_name(bone_node->name);

	return bone_attachment;
}

MeshInstance *EditorSceneImporterGLTF::_generate_mesh_instance(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index) {
	const GLTFNode *gltf_node = state.nodes[node_index];

	ERR_FAIL_INDEX_V(gltf_node->mesh, state.meshes.size(), nullptr);

	MeshInstance *mi = memnew(MeshInstance);
	print_verbose("glTF: Creating mesh for: " + gltf_node->name);

	GLTFMesh &mesh = state.meshes.write[gltf_node->mesh];
	mi->set_mesh(mesh.mesh);

	if (mesh.mesh->get_name() == "") {
		mesh.mesh->set_name(gltf_node->name);
	}

	for (int i = 0; i < mesh.blend_weights.size(); i++) {
		mi->set("blend_shapes/" + mesh.mesh->get_blend_shape_name(i), mesh.blend_weights[i]);
	}

	return mi;
}

Camera *EditorSceneImporterGLTF::_generate_camera(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index) {
	const GLTFNode *gltf_node = state.nodes[node_index];

	ERR_FAIL_INDEX_V(gltf_node->camera, state.cameras.size(), nullptr);

	Camera *camera = memnew(Camera);
	print_verbose("glTF: Creating camera for: " + gltf_node->name);

	const GLTFCamera &c = state.cameras[gltf_node->camera];
	if (c.perspective) {
		camera->set_perspective(c.fov_size, c.znear, c.znear);
	} else {
		camera->set_orthogonal(c.fov_size, c.znear, c.znear);
	}

	return camera;
}

Spatial *EditorSceneImporterGLTF::_generate_spatial(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index) {
	const GLTFNode *gltf_node = state.nodes[node_index];

	Spatial *spatial = memnew(Spatial);
	print_verbose("glTF: Creating spatial for: " + gltf_node->name);

	return spatial;
}

void EditorSceneImporterGLTF::_generate_scene_node(GLTFState &state, Node *scene_parent, Spatial *scene_root, const GLTFNodeIndex node_index) {

	const GLTFNode *gltf_node = state.nodes[node_index];

	Spatial *current_node = nullptr;

	// Is our parent a skeleton
	Skeleton *active_skeleton = Object::cast_to<Skeleton>(scene_parent);

	if (gltf_node->skeleton >= 0) {
		Skeleton *skeleton = state.skeletons[gltf_node->skeleton].godot_skeleton;

		if (active_skeleton != skeleton) {
			ERR_FAIL_COND_MSG(active_skeleton != nullptr, "glTF: Generating scene detected direct parented Skeletons");

			// Add it to the scene if it has not already been added
			if (skeleton->get_parent() == nullptr) {
				scene_parent->add_child(skeleton);
				skeleton->set_owner(scene_root);
			}
		}

		active_skeleton = skeleton;
		current_node = skeleton;
	}

	// If we have an active skeleton, and the node is node skinned, we need to create a bone attachment
	if (current_node == nullptr && active_skeleton != nullptr && gltf_node->skin < 0) {
		BoneAttachment *bone_attachment = _generate_bone_attachment(state, active_skeleton, node_index);

		scene_parent->add_child(bone_attachment);
		bone_attachment->set_owner(scene_root);

		// There is no gltf_node that represent this, so just directly create a unique name
		bone_attachment->set_name(_gen_unique_name(state, "BoneAttachment"));

		// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
		// and attach it to the bone_attachment
		scene_parent = bone_attachment;
	}

	// We still have not managed to make a node
	if (current_node == nullptr) {
		if (gltf_node->mesh >= 0) {
			current_node = _generate_mesh_instance(state, scene_parent, node_index);
		} else if (gltf_node->camera >= 0) {
			current_node = _generate_camera(state, scene_parent, node_index);
		} else {
			current_node = _generate_spatial(state, scene_parent, node_index);
		}

		scene_parent->add_child(current_node);
		current_node->set_owner(scene_root);
		current_node->set_transform(gltf_node->xform);
		current_node->set_name(gltf_node->name);
	}

	state.scene_nodes.insert(node_index, current_node);

	for (int i = 0; i < gltf_node->children.size(); ++i) {
		_generate_scene_node(state, current_node, scene_root, gltf_node->children[i]);
	}
}

template <class T>
struct EditorSceneImporterGLTFInterpolate {

	T lerp(const T &a, const T &b, float c) const {

		return a + (b - a) * c;
	}

	T catmull_rom(const T &p0, const T &p1, const T &p2, const T &p3, float t) {

		const float t2 = t * t;
		const float t3 = t2 * t;

		return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4 * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	T bezier(T start, T control_1, T control_2, T end, float t) {
		/* Formula from Wikipedia article on Bezier curves. */
		const real_t omt = (1.0 - t);
		const real_t omt2 = omt * omt;
		const real_t omt3 = omt2 * omt;
		const real_t t2 = t * t;
		const real_t t3 = t2 * t;

		return start * omt3 + control_1 * omt2 * t * 3.0 + control_2 * omt * t2 * 3.0 + end * t3;
	}
};

// thank you for existing, partial specialization
template <>
struct EditorSceneImporterGLTFInterpolate<Quat> {

	Quat lerp(const Quat &a, const Quat &b, const float c) const {
		ERR_FAIL_COND_V(!a.is_normalized(), Quat());
		ERR_FAIL_COND_V(!b.is_normalized(), Quat());

		return a.slerp(b, c).normalized();
	}

	Quat catmull_rom(const Quat &p0, const Quat &p1, const Quat &p2, const Quat &p3, const float c) {
		ERR_FAIL_COND_V(!p1.is_normalized(), Quat());
		ERR_FAIL_COND_V(!p2.is_normalized(), Quat());

		return p1.slerp(p2, c).normalized();
	}

	Quat bezier(const Quat start, const Quat control_1, const Quat control_2, const Quat end, const float t) {
		ERR_FAIL_COND_V(!start.is_normalized(), Quat());
		ERR_FAIL_COND_V(!end.is_normalized(), Quat());

		return start.slerp(end, t).normalized();
	}
};

template <class T>
T EditorSceneImporterGLTF::_interpolate_track(const Vector<float> &p_times, const Vector<T> &p_values, const float p_time, const GLTFAnimation::Interpolation p_interp) {

	//could use binary search, worth it?
	int idx = -1;
	for (int i = 0; i < p_times.size(); i++) {
		if (p_times[i] > p_time)
			break;
		idx++;
	}

	EditorSceneImporterGLTFInterpolate<T> interp;

	switch (p_interp) {
		case GLTFAnimation::INTERP_LINEAR: {

			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.lerp(p_values[idx], p_values[idx + 1], c);

		} break;
		case GLTFAnimation::INTERP_STEP: {

			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			return p_values[idx];

		} break;
		case GLTFAnimation::INTERP_CATMULLROMSPLINE: {

			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[1 + p_times.size() - 1];
			}

			const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.catmull_rom(p_values[idx - 1], p_values[idx], p_values[idx + 1], p_values[idx + 3], c);

		} break;
		case GLTFAnimation::INTERP_CUBIC_SPLINE: {

			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[(p_times.size() - 1) * 3 + 1];
			}

			const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			const T from = p_values[idx * 3 + 1];
			const T c1 = from + p_values[idx * 3 + 2];
			const T to = p_values[idx * 3 + 4];
			const T c2 = to + p_values[idx * 3 + 3];

			return interp.bezier(from, c1, c2, to, c);

		} break;
	}

	ERR_FAIL_V(p_values[0]);
}

void EditorSceneImporterGLTF::_import_animation(GLTFState &state, AnimationPlayer *ap, const GLTFAnimationIndex index, const int bake_fps) {

	const GLTFAnimation &anim = state.animations[index];

	String name = anim.name;
	if (name.empty()) {
		// No node represent these, and they are not in the hierarchy, so just make a unique name
		name = _gen_unique_name(state, "Animation");
	}

	Ref<Animation> animation;
	animation.instance();
	animation->set_name(name);

	float length = 0;

	for (Map<int, GLTFAnimation::Track>::Element *E = anim.tracks.front(); E; E = E->next()) {

		const GLTFAnimation::Track &track = E->get();
		//need to find the path
		NodePath node_path;

		GLTFNodeIndex node_index = E->key();
		if (state.nodes[node_index]->fake_joint_parent >= 0) {
			// Should be same as parent
			node_index = state.nodes[node_index]->fake_joint_parent;
		}

		const GLTFNode *node = state.nodes[E->key()];

		if (node->skeleton >= 0) {
			const Skeleton *sk = Object::cast_to<Skeleton>(state.scene_nodes.find(node_index)->get());
			ERR_FAIL_COND(sk == nullptr);

			const String path = ap->get_parent()->get_path_to(sk);
			const String bone = node->name;
			node_path = path + ":" + bone;
		} else {
			node_path = ap->get_parent()->get_path_to(state.scene_nodes.find(node_index)->get());
		}

		for (int i = 0; i < track.rotation_track.times.size(); i++) {
			length = MAX(length, track.rotation_track.times[i]);
		}
		for (int i = 0; i < track.translation_track.times.size(); i++) {
			length = MAX(length, track.translation_track.times[i]);
		}
		for (int i = 0; i < track.scale_track.times.size(); i++) {
			length = MAX(length, track.scale_track.times[i]);
		}

		for (int i = 0; i < track.weight_tracks.size(); i++) {
			for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
				length = MAX(length, track.weight_tracks[i].times[j]);
			}
		}

		if (track.rotation_track.values.size() || track.translation_track.values.size() || track.scale_track.values.size()) {
			//make transform track
			int track_idx = animation->get_track_count();
			animation->add_track(Animation::TYPE_TRANSFORM);
			animation->track_set_path(track_idx, node_path);
			//first determine animation length

			const float increment = 1.0 / float(bake_fps);
			float time = 0.0;

			Vector3 base_pos;
			Quat base_rot;
			Vector3 base_scale = Vector3(1, 1, 1);

			if (!track.rotation_track.values.size()) {
				base_rot = state.nodes[E->key()]->rotation.normalized();
			}

			if (!track.translation_track.values.size()) {
				base_pos = state.nodes[E->key()]->translation;
			}

			if (!track.scale_track.values.size()) {
				base_scale = state.nodes[E->key()]->scale;
			}

			bool last = false;
			while (true) {

				Vector3 pos = base_pos;
				Quat rot = base_rot;
				Vector3 scale = base_scale;

				if (track.translation_track.times.size()) {
					pos = _interpolate_track<Vector3>(track.translation_track.times, track.translation_track.values, time, track.translation_track.interpolation);
				}

				if (track.rotation_track.times.size()) {
					rot = _interpolate_track<Quat>(track.rotation_track.times, track.rotation_track.values, time, track.rotation_track.interpolation);
				}

				if (track.scale_track.times.size()) {
					scale = _interpolate_track<Vector3>(track.scale_track.times, track.scale_track.values, time, track.scale_track.interpolation);
				}

				if (node->skeleton >= 0) {

					Transform xform;
					xform.basis.set_quat_scale(rot, scale);
					xform.origin = pos;

					const Skeleton *skeleton = state.skeletons[node->skeleton].godot_skeleton;
					const int bone_idx = skeleton->find_bone(node->name);
					xform = skeleton->get_bone_rest(bone_idx).affine_inverse() * xform;

					rot = xform.basis.get_rotation_quat();
					rot.normalize();
					scale = xform.basis.get_scale();
					pos = xform.origin;
				}

				animation->transform_track_insert_key(track_idx, time, pos, rot, scale);

				if (last) {
					break;
				}
				time += increment;
				if (time >= length) {
					last = true;
					time = length;
				}
			}
		}

		for (int i = 0; i < track.weight_tracks.size(); i++) {
			ERR_CONTINUE(node->mesh < 0 || node->mesh >= state.meshes.size());
			const GLTFMesh &mesh = state.meshes[node->mesh];
			const String prop = "blend_shapes/" + mesh.mesh->get_blend_shape_name(i);

			const String blend_path = String(node_path) + ":" + prop;

			const int track_idx = animation->get_track_count();
			animation->add_track(Animation::TYPE_VALUE);
			animation->track_set_path(track_idx, blend_path);

			// Only LINEAR and STEP (NEAREST) can be supported out of the box by Godot's Animation,
			// the other modes have to be baked.
			GLTFAnimation::Interpolation gltf_interp = track.weight_tracks[i].interpolation;
			if (gltf_interp == GLTFAnimation::INTERP_LINEAR || gltf_interp == GLTFAnimation::INTERP_STEP) {
				animation->track_set_interpolation_type(track_idx, gltf_interp == GLTFAnimation::INTERP_STEP ? Animation::INTERPOLATION_NEAREST : Animation::INTERPOLATION_LINEAR);
				for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
					const float t = track.weight_tracks[i].times[j];
					const float w = track.weight_tracks[i].values[j];
					animation->track_insert_key(track_idx, t, w);
				}
			} else {
				// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
				const float increment = 1.0 / float(bake_fps);
				float time = 0.0;
				bool last = false;
				while (true) {
					_interpolate_track<float>(track.weight_tracks[i].times, track.weight_tracks[i].values, time, gltf_interp);
					if (last) {
						break;
					}
					time += increment;
					if (time >= length) {
						last = true;
						time = length;
					}
				}
			}
		}
	}

	animation->set_length(length);

	ap->add_animation(name, animation);
}

void EditorSceneImporterGLTF::_process_mesh_instances(GLTFState &state, Spatial *scene_root) {
	for (GLTFNodeIndex node_i = 0; node_i < state.nodes.size(); ++node_i) {
		const GLTFNode *node = state.nodes[node_i];

		if (node->skin >= 0 && node->mesh >= 0) {
			const GLTFSkinIndex skin_i = node->skin;

			Map<GLTFNodeIndex, Node *>::Element *mi_element = state.scene_nodes.find(node_i);
			MeshInstance *mi = Object::cast_to<MeshInstance>(mi_element->get());
			ERR_FAIL_COND(mi == nullptr);

			const GLTFSkeletonIndex skel_i = state.skins[node->skin].skeleton;
			const GLTFSkeleton &gltf_skeleton = state.skeletons[skel_i];
			Skeleton *skeleton = gltf_skeleton.godot_skeleton;
			ERR_FAIL_COND(skeleton == nullptr);

			mi->get_parent()->remove_child(mi);
			skeleton->add_child(mi);
			mi->set_owner(scene_root);

			mi->set_skin(state.skins[skin_i].godot_skin);
			mi->set_skeleton_path(mi->get_path_to(skeleton));
			mi->set_transform(Transform());
		}
	}
}

Spatial *EditorSceneImporterGLTF::_generate_scene(GLTFState &state, const int p_bake_fps) {

	Spatial *root = memnew(Spatial);

	// scene_name is already unique
	root->set_name(state.scene_name);

	for (int i = 0; i < state.root_nodes.size(); ++i) {
		_generate_scene_node(state, root, root, state.root_nodes[i]);
	}

	_process_mesh_instances(state, root);

	if (state.animations.size()) {
		AnimationPlayer *ap = memnew(AnimationPlayer);
		ap->set_name("AnimationPlayer");
		root->add_child(ap);
		ap->set_owner(root);

		for (int i = 0; i < state.animations.size(); i++) {
			_import_animation(state, ap, i, p_bake_fps);
		}
	}

	return root;
}

Node *EditorSceneImporterGLTF::import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err) {

	GLTFState state;

	if (p_path.to_lower().ends_with("glb")) {
		//binary file
		//text file
		Error err = _parse_glb(p_path, state);
		if (err)
			return NULL;
	} else {
		//text file
		Error err = _parse_json(p_path, state);
		if (err)
			return NULL;
	}

	ERR_FAIL_COND_V(!state.json.has("asset"), NULL);

	Dictionary asset = state.json["asset"];

	ERR_FAIL_COND_V(!asset.has("version"), NULL);

	String version = asset["version"];

	state.major_version = version.get_slice(".", 0).to_int();
	state.minor_version = version.get_slice(".", 1).to_int();

	if (p_flags & EditorSceneImporter::IMPORT_ANIMATION_8_WEIGHTS) {
		state.is_8_weights = true;
	}

	/* STEP 0 PARSE SCENE */
	Error err = _parse_scenes(state);
	if (err != OK)
		return NULL;

	/* STEP 1 PARSE NODES */
	err = _parse_nodes(state);
	if (err != OK)
		return NULL;

	/* STEP 2 PARSE BUFFERS */
	err = _parse_buffers(state, p_path.get_base_dir());
	if (err != OK)
		return NULL;

	/* STEP 3 PARSE BUFFER VIEWS */
	err = _parse_buffer_views(state);
	if (err != OK)
		return NULL;

	/* STEP 4 PARSE ACCESSORS */
	err = _parse_accessors(state);
	if (err != OK)
		return NULL;

	/* STEP 5 PARSE IMAGES */
	err = _parse_images(state, p_path.get_base_dir());
	if (err != OK)
		return NULL;

	/* STEP 6 PARSE TEXTURES */
	err = _parse_textures(state);
	if (err != OK)
		return NULL;

	/* STEP 7 PARSE TEXTURES */
	err = _parse_materials(state);
	if (err != OK)
		return NULL;

	/* STEP 9 PARSE SKINS */
	err = _parse_skins(state);
	if (err != OK)
		return NULL;

	/* STEP 10 DETERMINE SKELETONS */
	err = _determine_skeletons(state);
	if (err != OK)
		return NULL;

	/* STEP 11 CREATE SKELETONS */
	err = _create_skeletons(state);
	if (err != OK)
		return NULL;

	/* STEP 12 CREATE SKINS */
	err = _create_skins(state);
	if (err != OK)
		return NULL;

	/* STEP 13 PARSE MESHES (we have enough info now) */
	err = _parse_meshes(state);
	if (err != OK)
		return NULL;

	/* STEP 14 PARSE CAMERAS */
	err = _parse_cameras(state);
	if (err != OK)
		return NULL;

	/* STEP 15 PARSE ANIMATIONS */
	err = _parse_animations(state);
	if (err != OK)
		return NULL;

	/* STEP 16 ASSIGN SCENE NAMES */
	_assign_scene_names(state);

	/* STEP 17 MAKE SCENE! */
	Spatial *scene = _generate_scene(state, p_bake_fps);

	return scene;
}

Ref<Animation> EditorSceneImporterGLTF::import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps) {

	return Ref<Animation>();
}

EditorSceneImporterGLTF::EditorSceneImporterGLTF() {
}
