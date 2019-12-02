/*************************************************************************/
/*  editor_scene_saver_gltf.cpp                                          */
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

#include "editor_scene_exporter_gltf.h"
#include "core/object.h"
#include "core/project_settings.h"
#include "core/vector.h"
#include "editor/saver/resource_saver_scene.h"
#include "gltf_document.h"
#include "modules/csg/csg_shape.h"
#include "modules/gridmap/grid_map.h"
#include "scene/3d/mesh_instance.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/check_box.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

Error EditorSceneExporterGLTF::_generate_gltf_scene(const String p_path, Spatial *p_root_node) {
	Vector<MeshInstance *> mesh_items;
	_find_all_mesh_instances(mesh_items, p_root_node, p_root_node);

	Vector<CSGShape *> csg_items;
	_find_all_csg_roots(csg_items, p_root_node, p_root_node);

	Vector<GridMap *> grid_map_items;
	_find_all_gridmaps(grid_map_items, p_root_node, p_root_node);

	Vector<MeshInfo> meshes;
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		MeshInfo mesh_info;
		mesh_info.mesh = mesh_items[i]->get_mesh();
		mesh_info.transform = mesh_items[i]->get_transform();
		mesh_info.name = mesh_items[i]->get_name();
		mesh_info.original_node = mesh_items[i];
		for (int32_t j = 0; j < mesh_items[i]->get_surface_material_count(); j++) {
			mesh_info.materials.push_back(mesh_items[i]->get_surface_material(j));
		}
		meshes.push_back(mesh_info);
	}
	for (int32_t i = 0; i < csg_items.size(); i++) {
		Ref<Mesh> mesh = csg_items[i]->get_calculated_mesh();
		MeshInfo mesh_info;
		mesh_info.mesh = mesh;
		mesh_info.transform = csg_items[i]->get_transform();
		mesh_info.name = csg_items[i]->get_name();
		mesh_info.original_node = csg_items[i];
		meshes.push_back(mesh_info);
	}
	for (int32_t i = 0; i < grid_map_items.size(); i++) {
		Array cells = grid_map_items[i]->get_used_cells();
		for (int32_t k = 0; k < cells.size(); k++) {
			Vector3 cell_location = cells[k];
			int32_t cell = grid_map_items[i]->get_cell_item(cell_location.x, cell_location.y, cell_location.z);
			MeshInfo mesh_info;
			mesh_info.mesh = grid_map_items[i]->get_mesh_library()->get_item_mesh(cell);
			Transform cell_xform;
			cell_xform.basis.set_orthogonal_index(grid_map_items[i]->get_cell_item_orientation(cell_location.x, cell_location.y, cell_location.z));
			cell_xform.basis.scale(Vector3(grid_map_items[i]->get_cell_scale(), grid_map_items[i]->get_cell_scale(), grid_map_items[i]->get_cell_scale()));
			cell_xform.set_origin(grid_map_items[i]->map_to_world(cell_location.x, cell_location.y, cell_location.z));
			mesh_info.transform = cell_xform * grid_map_items[i]->get_transform();
			mesh_info.name = grid_map_items[i]->get_mesh_library()->get_item_name(cell);
			mesh_info.original_node = csg_items[i];
			meshes.push_back(mesh_info);
		}
	}

	for (int32_t i = 0; i < meshes.size(); i++) {
		MeshInstance *mi = memnew(MeshInstance);
		mi->set_mesh(meshes[i].mesh);
		for (int32_t j = 0; j < meshes[i].materials.size(); j++) {
			mi->set_surface_material(j, meshes[i].materials[j]);
		}
		mi->set_name(meshes[i].name);
		mi->set_transform(meshes[i].transform);
		meshes[i].original_node->replace_by(mi);
	}

	// Vector<aiMesh *> assimp_meshes;
	// Vector<aiMaterial *> assimp_materials;
	// aiNode *assimp_root_node = NULL;
	// _generate_node(p_root_node, num_meshes, assimp_root_node, assimp_root_node, assimp_meshes, assimp_materials);
	// r_scene.mRootNode = assimp_root_node;
	// r_scene.mMeshes = new aiMesh *[num_meshes];
	// for (int32_t i = 0; i < assimp_meshes.size(); i++) {
	// r_scene.mMeshes[i] = assimp_meshes[i];
	// }
	// r_scene.mMaterials = new aiMaterial *[assimp_materials.size()]();
	// r_scene.mNumMaterials = assimp_materials.size();
	// for (uint32_t i = 0; i < r_scene.mNumMaterials; i++) {
	// 	r_scene.mMaterials[i] = assimp_materials[i];
	// }
	// r_scene.mNumMaterials = assimp_materials.size();
	// r_scene.mNumMeshes = num_meshes;

	Ref<GLTFDocument> gltf_document;
	gltf_document.instance();

	GLTFDocument::GLTFState state;

	{
		const int scene_root = 0;
		state.root_nodes.push_back(scene_root);
		gltf_document->_convert_scene_node(state, p_root_node, p_root_node, scene_root, scene_root);
		gltf_document->_convert_mesh_instances(state, p_root_node);

		if (state.animations.size()) {
			Node *node = p_root_node->find_node("AnimationPlayer");
			AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(node);
			if (ap) {
				for (int i = 0; i < state.animations.size(); i++) {
					gltf_document->_convert_animation(state, ap, i);
				}
			}
		}
	}
	{
		if (p_path.to_lower().ends_with("glb")) {
			//binary file
			//text file
			// Error err = _serialize_glb(p_path, *state);
			// if (err)
			// 	return FAILED;
		} else {
			//text file
			Error err = gltf_document->_serialize_json(p_path, state);
			if (err)
				return FAILED;
		}
	}

	// void EditorSceneExporterGLTF::_generate_node(Node *p_node, size_t &num_meshes, /*aiNode *&p_assimp_current_node, aiNode *&p_assimp_root, */Vector<aiMesh *> &assimp_meshes, Vector<aiMaterial *> &assimp_materials) {
	// 	String node_name = p_node->get_name();
	// 	// const std::wstring w_node_name = node_name.c_str();
	// 	// const std::string s_node_name(w_node_name.begin(), w_node_name.end());
	// 	// p_assimp_current_node = new aiNode();
	// 	// p_assimp_current_node->mName = s_node_name;
	// 	if (Object::cast_to<MeshInstance>(p_node)) {
	// 		MeshInstance *mi = Object::cast_to<MeshInstance>(p_node);
	// 		Ref<SurfaceTool> st;
	// 		st.instance();
	// 		st->begin(Mesh::PRIMITIVE_TRIANGLES);
	// 		// p_assimp_current_node->mNumMeshes = mi->get_mesh()->get_surface_count();
	// 		// p_assimp_current_node->mMeshes = new uint32_t[mi->get_mesh()->get_surface_count()];
	// 		// p_assimp_current_node->mTransformation = _convert_assimp_transform(Object::cast_to<Spatial>(p_node)->get_transform());
	// 		for (int32_t j = 0; j < mi->get_mesh()->get_surface_count(); j++) {
	// 			// p_assimp_current_node->mMeshes[j] = num_meshes + j;
	// 			st->create_from(mi->get_mesh(), j);
	// 			st->index();
	// 			Array mesh_arr = st->commit_to_arrays();

	// 			PoolVector3Array vertices = mesh_arr[Mesh::ARRAY_VERTEX];
	// 			PoolVector3Array normals = mesh_arr[Mesh::ARRAY_NORMAL];
	// 			PoolVector2Array uv1s = mesh_arr[Mesh::ARRAY_TEX_UV];
	// 			PoolVector2Array uv2s = mesh_arr[Mesh::ARRAY_TEX_UV2];
	// 			PoolColorArray tangents = mesh_arr[Mesh::ARRAY_FORMAT_TANGENT];
	// 			PoolIntArray indices = mesh_arr[Mesh::ARRAY_INDEX];
	// 			PoolIntArray bones = mesh_arr[Mesh::ARRAY_BONES];
	// 			PoolRealArray weights = mesh_arr[Mesh::ARRAY_WEIGHTS];

	// 			// aiMesh *mesh = new aiMesh();
	// 			// assimp_meshes.push_back(mesh);
	// 			// // Todo remove wstring and string
	// 			// String name = String(mi->get_name()) + itos(j);
	// 			// const std::wstring w_name = name.c_str();
	// 			// const std::string s_name(w_name.begin(), w_name.end());
	// 			// mesh->mName = s_name;
	// 			// mesh->mVertices = new aiVector3D[vertices.size()]();
	// 			// mesh->mNormals = new aiVector3D[vertices.size()]();
	// 			// mesh->mNumVertices = vertices.size();

	// 			NodePath skeleton_path = mi->get_skeleton_path();
	// 			Node *node = mi->get_node_or_null(skeleton_path);
	// 			Skeleton *skeleton = Node::cast_to<Skeleton>(node);
	// 			// TODO FIX Skinning
	// 			skeleton = nullptr;
	// 			if (skeleton) {
	// 				struct VertexWeights {
	// 					Vector<int32_t> vertex_idx;
	// 					Vector<real_t> weights;
	// 				};
	// 				Map<String, VertexWeights>
	// 						bone_weights;
	// 				Ref<Skin> skin = mi->get_skin();
	// 				Map<int32_t, String> idx_to_bone_name;

	// 				for (int32_t i = 0; i < skin->get_bind_count(); i++) {
	// 					int32_t skeleton_idx = skin->get_bind_bone(i);
	// 					String bone_name = skeleton->get_bone_name(skeleton_idx);
	// 					idx_to_bone_name.insert(skeleton_idx, bone_name);
	// 				}
	// 				int32_t vertex_idx = 0;
	// 				for (int32_t k = 0; k < bones.size(); k += 4) {
	// 					String bone_name = idx_to_bone_name[bones[k]];
	// 					VertexWeights vertex_weights;
	// 					vertex_weights.vertex_idx.push_back(bones[k + 0]);
	// 					vertex_weights.vertex_idx.push_back(bones[k + 1]);
	// 					vertex_weights.vertex_idx.push_back(bones[k + 2]);
	// 					vertex_weights.vertex_idx.push_back(bones[k + 3]);
	// 					vertex_weights.weights.push_back(weights[k + 0]);
	// 					vertex_weights.weights.push_back(weights[k + 1]);
	// 					vertex_weights.weights.push_back(weights[k + 2]);
	// 					vertex_weights.weights.push_back(weights[k + 3]);
	// 					bone_weights.insert(bone_name, vertex_weights);
	// 					vertex_idx++;
	// 				}

	// 				// mesh->mBones = new aiBone *[bone_weights.size()]();
	// 				for (int32_t i = 0; i < bone_weights.size(); i++) {
	// 					int32_t skeleton_idx = skin->get_bind_bone(i);
	// 					String bone_name = skeleton->get_bone_name(skeleton_idx);
	// 					// std::wstring ws = bone_name.c_str();
	// 					// std::string s(ws.begin(), ws.end());
	// 					// aiString assimp_bone_name;
	// 					// aiBone *bone = new aiBone();
	// 					// mesh->mBones[i] = bone;
	// 					// bone->mName.Set(s);
	// 					// Map<String, VertexWeights>::Element *E = bone_weights.find(bone_name);
	// 					// if (!E) {
	// 					// 	continue;
	// 					// }
	// 					// bone->mOffsetMatrix = _convert_assimp_transform(skin->get_bind_pose(i));
	// 					// mesh->mNumBones += 1;
	// 					// bone->mNumWeights = E->get().vertex_idx.size();
	// 					// bone->mWeights = new aiVertexWeight[bone->mNumWeights]();
	// 					// for (int32_t j = 0; j < bone->mNumWeights; j++) {
	// 					// 	bone->mWeights[j].mVertexId = bone_weights.find(bone_name)->get().vertex_idx[j];
	// 					// 	bone->mWeights[j].mWeight = bone_weights.find(bone_name)->get().weights[j];
	// 					// }
	// 				}
	// 			}

	// 			if (uv1s.size()) {
	// 				// mesh->mTextureCoords[0] = new aiVector3D[vertices.size()];
	// 				// if (uv2s.size()) {
	// 				// 	mesh->mTextureCoords[1] = new aiVector3D[vertices.size()];
	// 				// }
	// 				// mesh->mNumUVComponents[0] = 2;
	// 			}

	// 			if (mi->get_mesh()->get_faces().size()) {
	// 				// mesh->mFaces = new aiFace[mi->get_mesh()->get_faces().size()]();
	// 			}

	// 			for (int32_t k = 0; k < indices.size() / 3; k++) {
	// 				// aiFace face;
	// 				// face.mNumIndices = 3;
	// 				// face.mIndices = new unsigned int[3];
	// 				// face.mIndices[0] = indices[k * 3 + 2];
	// 				// face.mIndices[1] = indices[k * 3 + 1];
	// 				// face.mIndices[2] = indices[k * 3 + 0];
	// 				// mesh->mFaces[k] = face;
	// 			}
	// 			if (indices.size()) {
	// 				// mesh->mNumFaces = indices.size() / 3;
	// 			}

	// 			for (int32_t k = 0; k < vertices.size(); k++) {
	// 				// mesh->mVertices[k] = aiVector3D(vertices[k].x, vertices[k].y, vertices[k].z);
	// 			}
	// 			for (int32_t k = 0; k < normals.size(); k++) {
	// 				// mesh->mNormals[k] = aiVector3D(normals[k].x, normals[k].y, normals[k].z);
	// 			}
	// 			for (int32_t k = 0; k < uv1s.size(); k++) {
	// 				// mesh->mTextureCoords[0][k] = aiVector3D(uv1s[k].x, 1.0f - uv1s[k].y, 0);
	// 			}
	// 			for (int32_t k = 0; k < uv2s.size(); k++) {
	// 				// mesh->mTextureCoords[1][k] = aiVector3D(uv2s[k].x, 1.0f - uv2s[k].y, 0);
	// 			}
	// 			// aiMaterial *assimp_mat = new aiMaterial();
	// 			// mesh->mMaterialIndex = assimp_materials.size();
	// 			// assimp_materials.push_back(assimp_mat);
	// 			// mesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
	// 		}
	// 		num_meshes += mi->get_mesh()->get_surface_count();
	// 	} else if (Object::cast_to<Spatial>(p_node)) {
	// 		// p_assimp_current_node->mTransformation = _convert_assimp_transform(Object::cast_to<Spatial>(p_node)->get_transform());
	// 	}
	// 	// aiNode **children = new aiNode *[p_node->get_child_count()]();
	// 	// p_assimp_current_node->addChildren(p_node->get_child_count(), children);
	// 	// for (int32_t i = 0; i < p_node->get_child_count(); i++) {
	// 	// 	_generate_node(p_node->get_child(i), num_meshes, p_assimp_current_node->mChildren[i], p_assimp_root, assimp_meshes, assimp_materials);
	// 	// }
	// }

	return OK;
}

void EditorSceneExporterGLTF::_find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner) {
	MeshInstance *mi = Object::cast_to<MeshInstance>(p_current_node);
	if (mi != NULL) {
		r_items.push_back(mi);
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_mesh_instances(r_items, p_current_node->get_child(i), p_owner);
	}
}

void EditorSceneExporterGLTF::_find_all_gridmaps(Vector<GridMap *> &r_items, Node *p_current_node, const Node *p_owner) {
	GridMap *gridmap = Object::cast_to<GridMap>(p_current_node);
	if (gridmap != NULL) {
		r_items.push_back(gridmap);
		return;
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_gridmaps(r_items, p_current_node->get_child(i), p_owner);
	}
}

void EditorSceneExporterGLTF::_find_all_csg_roots(Vector<CSGShape *> &r_items, Node *p_current_node, const Node *p_owner) {
	CSGShape *csg = Object::cast_to<CSGShape>(p_current_node);
	if (csg != NULL && csg->is_root_shape()) {
		r_items.push_back(csg);
		return;
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_csg_roots(r_items, p_current_node->get_child(i), p_owner);
	}
}

// void EditorSceneExporterGLTF::_set_gltf_materials(Ref<SpatialMaterial> &mat, aiMaterial *assimp_mat) {
// 	if (mat.is_null()) {
// 		return;
// 	}
// 	Ref<Texture> tex = mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO);
// 	if (tex.is_valid()) {
// 		NodePath path = tex->get_path();
// 		String global_path = ProjectSettings::get_singleton()->globalize_path(path);
// 		// const std::wstring w_global_path = global_path.c_str();
// 		// std::string s_global_path(w_global_path.begin(), w_global_path.end());
// 		// aiString uri(s_global_path);
// 		// assimp_mat->AddProperty(&uri, AI_MATKEY_TEXTURE_DIFFUSE(0));
// 	}
// 	// aiColor4D albedo_color;
// 	// albedo_color.r = mat->get_albedo().r;
// 	// albedo_color.g = mat->get_albedo().g;
// 	// albedo_color.b = mat->get_albedo().b;
// 	// albedo_color.a = mat->get_albedo().a;
// 	// assimp_mat->AddProperty(&albedo_color, 1, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR);
// }

void EditorSceneExporterGLTF::export_gltf2(const String p_file, Node *p_root_node) {
	const String file = ProjectSettings::get_singleton()->globalize_path(p_file);
	Spatial *spatial = Object::cast_to<Spatial>(p_root_node);
	_generate_gltf_scene(file, spatial);
}
