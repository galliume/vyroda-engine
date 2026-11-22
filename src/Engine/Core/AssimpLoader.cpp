module;

#include <assimp/config.h>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

module Engine.Core.AssimpLoader;

import std;

import Engine.Core.GLM;
import Engine.Core.MeshOptimizer;
import Engine.Core.VMeshCache;

namespace Vyroda
{
  //helper from learnopengl
  static inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
  {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
  }

  static inline glm::vec3 GetGLMVec(const aiVector3D& vec)
  {
    return glm::vec3(vec.x, vec.y, vec.z);
  }

  static inline glm::quat GetGLMQuat(const aiQuaternion& pOrientation)
  {
    return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
  }
  //

  static void countNodeInstances(
    aiNode const* node,
    std::size_t& total_instances)
  {
    total_instances += node->mNumMeshes;
    for (std::uint32_t i{ 0 }; i < node->mNumChildren; ++i) {
      countNodeInstances(node->mChildren[i], total_instances);
    }
  }

  static VMeshGeometry extractAiMesh(
    aiMesh const* mesh,
    aiScene const* scene,
    bool const flip_Y,
    std::unordered_map<std::string, Bone>* bones_map,
    std::uint32_t* global_bone_count)
  {
    VMeshGeometry out{};
    out.base_name = mesh->mName.C_Str();
    out.material_ID = mesh->mMaterialIndex;
    out.bbox_min = glm::vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
    out.bbox_max = glm::vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);

    out.positions.reserve(mesh->mNumVertices);
    out.vertices.reserve(mesh->mNumVertices);

    bool const has_normals { mesh->HasNormals() };
    bool const has_tangents_and_bitangents { mesh->HasTangentsAndBitangents() };
    bool const has_tangents_only { !has_tangents_and_bitangents && mesh->mTangents != nullptr };
    bool const has_uvs { mesh->mNumUVComponents[0] > 0 };

    std::uint32_t color_set_count { 0 };
    std::array<std::uint32_t, AI_MAX_NUMBER_OF_COLOR_SETS> active_color_sets{};
    for (std::uint32_t x{ 0 }; x < AI_MAX_NUMBER_OF_COLOR_SETS; ++x) {
      if (mesh->HasVertexColors(x)) {
        active_color_sets[color_set_count++] = x;
      }
    }

    for (std::uint32_t v{ 0 }; v < mesh->mNumVertices; ++v) {
      aiVector3D const& pos { mesh->mVertices[v] };
      out.positions.emplace_back(pos.x, pos.y, pos.z);

      Vertex vertex{};
      if (has_normals) {
        aiVector3D const& normal { mesh->mNormals[v] };
        vertex.normal = { normal.x, normal.y, normal.z, 1.0f };
      } else {
        vertex.normal = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
      }

      if (has_tangents_and_bitangents) {
        glm::vec3 const nn { vertex.normal };
        glm::vec3 const tt { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
        glm::vec3 const bb { mesh->mBitangents[v].x, mesh->mBitangents[v].y, mesh->mBitangents[v].z };
        float const ww { (glm::dot(glm::cross(nn, tt), bb) < 0.0f) ? -1.0f : 1.0f };
        vertex.tangent = glm::vec4(tt, ww);
      } else if (has_tangents_only) {
        vertex.tangent = glm::vec4(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z, 1.0f);
      } else {
        vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      }

      if (has_uvs) {
        aiVector3D const& texture_coord { mesh->mTextureCoords[0][v] };
        vertex.texture_coord = { texture_coord.x, flip_Y ? -texture_coord.y : texture_coord.y };
      }

      if (color_set_count == 0) {
        vertex.color = glm::vec4(1.0f);
      } else {
        glm::vec4 color{ 0.0f };
        for (std::uint32_t c{ 0 }; c < color_set_count; ++c) {
          auto const& v_color { mesh->mColors[active_color_sets[c]][v] };
          color += glm::vec4(v_color.r, v_color.g, v_color.b, v_color.a);
        }
        vertex.color = color / static_cast<float>(color_set_count);
      }

      out.vertices.emplace_back(vertex);
    }

    out.indices.reserve(static_cast<std::size_t>(mesh->mNumFaces) * 3u);
    for (std::uint32_t f{ 0 }; f < mesh->mNumFaces; ++f) {
      aiFace const& face { mesh->mFaces[f] };
      for (std::uint32_t j{ 0 }; j < face.mNumIndices; ++j) {
        out.indices.push_back(face.mIndices[j]);
      }
    }

    if (mesh->HasBones() && bones_map != nullptr && global_bone_count != nullptr) {
      out.vertices_bones.resize(mesh->mNumVertices, {});
      std::unordered_map<std::uint32_t, std::vector<std::pair<std::uint32_t, float>>> vertex_weight_map{};

      for (std::uint32_t b{ 0 }; b < mesh->mNumBones; ++b) {
        aiBone const* bone { mesh->mBones[b] };
        std::string const bone_name { bone->mName.C_Str() };
        aiNode const* bone_node { scene->mRootNode->FindNode(bone_name.c_str()) };
        if (bone_node == nullptr) {
          continue;
        }

        std::string const node_name { bone_node->mName.C_Str() };
        if (auto const existing { bones_map->find(node_name) }; existing != bones_map->end()) {
          std::uint32_t const existing_id { existing->second.id };
          for (std::uint32_t w{ 0 }; w < bone->mNumWeights; ++w) {
            aiVertexWeight const& aiWeight { bone->mWeights[w] };
            vertex_weight_map[aiWeight.mVertexId].emplace_back(existing_id, aiWeight.mWeight);
          }
          continue;
        }

        auto const bone_id { (*global_bone_count)++ };

        Bone bone_data{};
        bone_data.id = bone_id;
        bone_data.transform = ConvertMatrixToGLMFormat(bone_node->mTransformation);
        bone_data.offset_matrix = ConvertMatrixToGLMFormat(bone->mOffsetMatrix);
        bone_data.name = node_name;
        if (bone_node->mParent != nullptr) {
          bone_data.parent_name = bone_node->mParent->mName.C_Str();
        }
        bone_data.weights.reserve(bone->mNumWeights);

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(
          bone_data.transform,
          bone_data.t_pose_scale,
          bone_data.t_pose_rotation,
          bone_data.t_pose_position,
          skew,
          perspective);

        for (std::uint32_t w{ 0 }; w < bone->mNumWeights; ++w) {
          aiVertexWeight const& aiWeight { bone->mWeights[w] };
          bone_data.weights.emplace_back(aiWeight.mVertexId, aiWeight.mWeight);
          vertex_weight_map[aiWeight.mVertexId].emplace_back(bone_id, aiWeight.mWeight);
        }

        for (std::size_t z{ 0 }; z < bone_node->mNumChildren; ++z) {
          aiNode* child { bone_node->mChildren[z] };
          if (child) {
            bone_data.children.emplace_back(child->mName.C_Str());
          }
        }

        glm::mat4 t_pose { 1.0f };
        aiNode const* current { bone_node };
        while (current) {
          t_pose = ConvertMatrixToGLMFormat(current->mTransformation) * t_pose;
          current = current->mParent;
        }
        bone_data.t_pose = t_pose;
        (*bones_map)[node_name] = std::move(bone_data);
      }

      for (auto& [vertex_id, data_vertex] : vertex_weight_map) {
        auto const& pos { out.positions.at(vertex_id) };
        VertexBones vertex_bones{};
        vertex_bones.original_pos = pos;

        std::sort(data_vertex.begin(), data_vertex.end(),
          [](auto const& a, auto const& b) { return a.second > b.second; });

        float total_weight{ 0.0f };
        for (std::size_t y{ 0 }; y < 4 && y < data_vertex.size(); ++y) {
          vertex_bones.bone_ids[y] = static_cast<std::int32_t>(data_vertex[y].first);
          vertex_bones.bone_weights[y] = data_vertex[y].second;
          total_weight += data_vertex[y].second;
        }

        if (total_weight > 0.0f) {
          for (std::size_t y{ 0 }; y < 4; ++y) {
            vertex_bones.bone_weights[y] /= total_weight;
            vertex_bones.total_weight += vertex_bones.bone_weights[y];
          }
        }
        out.vertices_bones[vertex_id] = std::move(vertex_bones);
      }
    }

    if (!out.indices.empty() && !out.positions.empty() && (out.indices.size() % 3u == 0u)) {
      std::size_t const index_count { out.indices.size() };
      std::size_t vertex_count { out.positions.size() };

      // Weld identical vertices across split streams (replaces Assimp's slow JoinIdenticalVertices)
      constexpr std::size_t kVertexActiveSize { sizeof(glm::vec4) * 3u + sizeof(glm::vec2) };
      std::array<meshopt_Stream, 3> const streams {{
        { out.positions.data(), sizeof(glm::vec3), sizeof(glm::vec3) },
        { out.vertices.data(), kVertexActiveSize, sizeof(Vertex) },
        { out.vertices_bones.data(), sizeof(VertexBones), sizeof(VertexBones) }
      }};
      std::size_t const stream_count { out.vertices_bones.empty() ? 2u : 3u };

      std::vector<std::uint32_t> weld_remap(vertex_count);
      std::size_t const welded_vertices {
        meshopt_generateVertexRemapMulti(
          weld_remap.data(),
          out.indices.data(),
          index_count,
          vertex_count,
          streams.data(),
          stream_count)
      };

      if (welded_vertices < vertex_count) {
        meshopt_remapIndexBuffer(
          out.indices.data(),
          out.indices.data(),
          index_count,
          weld_remap.data());

        std::vector<glm::vec3> welded_positions(welded_vertices);
        meshopt_remapVertexBuffer(
          welded_positions.data(),
          out.positions.data(),
          vertex_count,
          sizeof(glm::vec3),
          weld_remap.data());
        out.positions = std::move(welded_positions);

        std::vector<Vertex> welded_vertices_buf(welded_vertices);
        meshopt_remapVertexBuffer(
          welded_vertices_buf.data(),
          out.vertices.data(),
          vertex_count,
          sizeof(Vertex),
          weld_remap.data());
        out.vertices = std::move(welded_vertices_buf);

        if (!out.vertices_bones.empty()) {
          std::vector<VertexBones> welded_bones(welded_vertices);
          meshopt_remapVertexBuffer(
            welded_bones.data(),
            out.vertices_bones.data(),
            vertex_count,
            sizeof(VertexBones),
            weld_remap.data());
          out.vertices_bones = std::move(welded_bones);
        }

        vertex_count = welded_vertices;
      }

      meshopt_optimizeVertexCache(
        out.indices.data(),
        out.indices.data(),
        index_count,
        vertex_count);

      meshopt_optimizeOverdraw(
        out.indices.data(),
        out.indices.data(),
        index_count,
        &out.positions[0].x,
        vertex_count,
        sizeof(glm::vec3),
        1.05f);

      std::vector<std::uint32_t> remap(vertex_count);
      std::size_t const unique_vertices {
        meshopt_optimizeVertexFetchRemap(
          remap.data(),
          out.indices.data(),
          index_count,
          vertex_count)
      };

      meshopt_remapIndexBuffer(
        out.indices.data(),
        out.indices.data(),
        index_count,
        remap.data());

      std::vector<glm::vec3> remapped_positions(unique_vertices);
      meshopt_remapVertexBuffer(
        remapped_positions.data(),
        out.positions.data(),
        vertex_count,
        sizeof(glm::vec3),
        remap.data());
      out.positions = std::move(remapped_positions);

      std::vector<Vertex> remapped_vertices(unique_vertices);
      meshopt_remapVertexBuffer(
        remapped_vertices.data(),
        out.vertices.data(),
        vertex_count,
        sizeof(Vertex),
        remap.data());
      out.vertices = std::move(remapped_vertices);

      if (!out.vertices_bones.empty()) {
        std::vector<VertexBones> remapped_bones(unique_vertices);
        meshopt_remapVertexBuffer(
          remapped_bones.data(),
          out.vertices_bones.data(),
          vertex_count,
          sizeof(VertexBones),
          remap.data());
        out.vertices_bones = std::move(remapped_bones);
      }
    }

    return out;
  }

  static void collectNodeInstances(
    aiNode const* node,
    std::vector<VMeshGeometry> const& geometries,
    std::vector<VMeshInstance>& out_instances,
    glm::mat4 const& global_transform,
    glm::mat4 const& inverse_global_transform)
  {
    auto const local_transform { ConvertMatrixToGLMFormat(node->mTransformation) };
    auto const transform_matrix { global_transform * local_transform };

    for (std::uint32_t i{ 0 }; i < node->mNumMeshes; ++i) {
      std::uint32_t const mesh_idx { node->mMeshes[i] };
      out_instances.emplace_back(VMeshInstance{
        .geometry_idx = mesh_idx,
        .name = geometries[mesh_idx].base_name + std::to_string(i),
        .transform_matrix = transform_matrix,
        .local_transform = local_transform,
        .inverse_transform_matrix = inverse_global_transform
      });
    }

    for (std::uint32_t i{ 0 }; i < node->mNumChildren; ++i) {
      collectNodeInstances(
        node->mChildren[i],
        geometries,
        out_instances,
        transform_matrix,
        inverse_global_transform);
    }
  }

  template <typename CallbackT>
  static void dispatchVMeshAsset(
    VMeshAsset& asset,
    std::string const& texture_prefix,
    CallbackT& callback)
  {
    std::vector<std::uint32_t> remaining_refs(asset.geometries.size(), 0u);
    for (auto const& inst : asset.instances) {
      ++remaining_refs[inst.geometry_idx];
    }

    std::uint64_t id { asset.instances.size() };
    for (auto& inst : asset.instances) {
      --id;
      auto& geom { asset.geometries[inst.geometry_idx] };
      bool const move_last { --remaining_refs[inst.geometry_idx] == 0u };

      PlpMeshData mesh_data{};
      mesh_data.id = id;
      mesh_data.name = std::move(inst.name);
      mesh_data.texture_prefix = texture_prefix;
      mesh_data.material_ID = geom.material_ID;
      mesh_data.bbox_min = geom.bbox_min;
      mesh_data.bbox_max = geom.bbox_max;
      mesh_data.transform_matrix = inst.transform_matrix;
      mesh_data.local_transform = inst.local_transform;
      mesh_data.inverse_transform_matrix = inst.inverse_transform_matrix;

      if (move_last) {
        mesh_data.positions = std::move(geom.positions);
        mesh_data.vertices = std::move(geom.vertices);
        mesh_data.vertices_bones = std::move(geom.vertices_bones);
        mesh_data.indices = std::move(geom.indices);
      } else {
        mesh_data.positions = geom.positions;
        mesh_data.vertices = geom.vertices;
        mesh_data.vertices_bones = geom.vertices_bones;
        mesh_data.indices = geom.indices;
      }

      if (!asset.bones_map.empty()) {
        mesh_data.bones = asset.bones_map;
        mesh_data.root_bone_name = asset.root_bone_name;
      }

      callback(
        std::move(mesh_data),
        asset.materials,
        asset.animations,
        asset.positions,
        asset.rotations,
        asset.scales);
    }
  }

  void AssimpLoader::loadData(
        std::string const & path,
        bool const flip_Y,
        std::function<void(
          PlpMeshData&& _data,
          std::vector<material_t> const& materials,
          std::vector<Animation> const& animations,
          std::unordered_map<std::string, std::vector<std::vector<Position>>> const& positions,
          std::unordered_map<std::string, std::vector<std::vector<Rotation>>> const& rotations,
          std::unordered_map<std::string, std::vector<std::vector<Scale>>> const& scales)> callback)
  {
    using Clock = std::chrono::steady_clock;
    auto const ms_since = [](Clock::time_point const a, Clock::time_point const b) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };
    auto const t_start { Clock::now() };

    std::filesystem::path const file_name{ path };
    auto const texture_prefix { file_name.stem().string() + "_" };
    auto const root_path { file_name.parent_path().string() + "/" };

    VMeshAsset cached_asset{};
    if (VMeshCache::tryLoad(file_name, root_path, flip_Y, cached_asset)) {
      auto const t_cache { Clock::now() };
      auto const geom_count { cached_asset.geometries.size() };
      auto const inst_count { cached_asset.instances.size() };
      dispatchVMeshAsset(cached_asset, texture_prefix, callback);
      auto const t_done { Clock::now() };
      Logger::info(
        "VMeshCache HIT [{}] ({} meshes / {} instances): Load={}ms, Callbacks+Tex={}ms, Total={}ms",
        file_name.filename().string(),
        geom_count,
        inst_count,
        ms_since(t_start, t_cache),
        ms_since(t_cache, t_done),
        ms_since(t_start, t_done));
      return;
    }

    Assimp::Importer importer;

    auto flags {
        aiProcess_Triangulate
      | aiProcess_GenSmoothNormals
      | aiProcess_CalcTangentSpace
      | aiProcess_FlipWindingOrder
      | aiProcess_FlipUVs
      | aiProcess_GenBoundingBoxes
    };

    const aiScene* scene = importer.ReadFile(path, flags);
    auto const t_read { Clock::now() };

    if (nullptr == scene) {
      Logger::error("Error while importing file {}: {}", path, importer.GetErrorString());
      return;
    }

    std::vector<material_t> materials{};
    materials.reserve(scene->mNumMaterials);

    if (scene->HasMaterials()) {

      for (std::size_t i{ 0 }; i < scene->mNumMaterials; ++i) {

        auto const& mat = scene->mMaterials[i];

        material_t material{};

        aiString texture_file;
        mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), texture_file);
        //if(auto texture = scene->GetEmbeddedTexture(texture_file.C_Str())) {
          //returned pointer is not null, read texture from memory
        //}
        material.name = mat->GetName().C_Str();

        aiColor4D baseColor(0.f);
        if (mat->Get(AI_MATKEY_BASE_COLOR, baseColor) == aiReturn_SUCCESS) {
          material.base_color = { baseColor.r, baseColor.g, baseColor.b, baseColor.a };
        }
        aiColor4D ambientColor(0.f);
        if (mat->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor) == aiReturn_SUCCESS) {
          material.ambient = { ambientColor.r, ambientColor.g, ambientColor.b, ambientColor.a };
        }
        aiColor4D diffuseColor(0.f);
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == aiReturn_SUCCESS) {
          material.diffuse = { diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a };
        }
        aiColor4D specularColor(0.f);
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == aiReturn_SUCCESS) {
          material.specular = { specularColor.r, specularColor.g, specularColor.b, specularColor.a };
        }
        aiColor3D transmittanceColor(0.f);
        if (mat->Get(AI_MATKEY_COLOR_TRANSPARENT, transmittanceColor) == aiReturn_SUCCESS) {
          material.transmittance = { transmittanceColor.r, transmittanceColor.g, transmittanceColor.b };
        }
        float e_factor;
        if (mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, e_factor) == aiReturn_SUCCESS) {
          material.mre_factor.z = e_factor;
        }
        float ior{ 1.f };
        if (mat->Get(AI_MATKEY_REFRACTI, ior) == aiReturn_SUCCESS) {
          material.ior = ior;
        }
        float dissolve{ 1.f };
        if (mat->Get(AI_MATKEY_OPACITY, dissolve) == aiReturn_SUCCESS) {
          material.dissolve = dissolve;
        }

        aiString alpha_mode;
        aiString mask{ "MASK" };
        aiString opaque{ "OPAQUE" };
        aiString blend{ "BLEND" };
        material.alpha_mode = 0.0f;//OPAQUE
        if (mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) == aiReturn_SUCCESS) {
          float mode{ 0.0f };//OPAQUE by default

          if (alpha_mode == mask) mode = 1.0f;
          else if (alpha_mode == blend) mode = 2.0f;

          material.alpha_mode = mode;
        }
        float alpha_cut_off;
        if (mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alpha_cut_off) == aiReturn_SUCCESS) {
          material.alpha_cut_off = alpha_cut_off;
        }
        float m;
        if (mat->Get(AI_MATKEY_METALLIC_FACTOR, m) == aiReturn_SUCCESS) {
          material.mre_factor.x = m;
        }
        float r;
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, r) == aiReturn_SUCCESS) {
          material.mre_factor.y = r;
        }
        float transmission_strength{ 0.0f };
        if (mat->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission_strength) == aiReturn_SUCCESS) {
          material.transmission_strength = transmission_strength;
        }
        float ao_strength{ 1.0 };
        if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_LIGHTMAP, 0), ao_strength) == aiReturn_SUCCESS) {
          material.occlusion_strength = ao_strength;
        }
        float glossiness{ 1.0 };
        if (mat->Get(AI_MATKEY_SHININESS_STRENGTH, glossiness) == aiReturn_SUCCESS) {
          material.shininess = glossiness;
        }


        if (mat->GetTextureCount(aiTextureType_AMBIENT) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_AMBIENT, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_ambient = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_ambient_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_AMBIENT, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_ambient_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_AMBIENT, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_ambient_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
          aiUVTransform transform{};
          if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_AMBIENT, 0), transform) == aiReturn_SUCCESS) {
              material.ambient_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
              material.ambient_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
              material.ambient_rotation = glm::vec2(transform.mRotation, 1.0);
          }
        }
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_diffuse = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_diffuse_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_DIFFUSE, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_diffuse_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_DIFFUSE, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_diffuse_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
          aiUVTransform transform{};
          if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE, 0), transform) == aiReturn_SUCCESS) {
            material.diffuse_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
            material.diffuse_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
            material.diffuse_rotation = glm::vec2(transform.mRotation, 1.0);
          }
        }
        if (mat->GetTextureCount(aiTextureType_SPECULAR) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_SPECULAR, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_specular = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_specular_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_SPECULAR, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_specular_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_SPECULAR, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_specular_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
        }
        if (mat->GetTextureCount(aiTextureType_SHININESS) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_SHININESS, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_specular_highlight = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_specular_highlight_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_SHININESS, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_specular_highlight_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_SHININESS, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_specular_highlight_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
        }

        bool const is_obj {std::filesystem::path(path).extension() == ".obj"};
        bool const is_fbx {std::filesystem::path(path).extension() == ".fbx"};

        if (is_obj || is_fbx) {
          if (mat->GetTextureCount(aiTextureType_HEIGHT) > 0) {
            aiString texture_path;
            aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
            aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

            if (mat->GetTexture(aiTextureType_HEIGHT, 0, &texture_path) == aiReturn_SUCCESS) {
                material.name_texture_bump = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
                material.name_texture_bump_path = root_path + texture_path.C_Str();
            }
            if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_HEIGHT, 0), wrap_mode_u) == aiReturn_SUCCESS) {
              material.texture_bump_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
            }
            if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_HEIGHT, 0), wrap_mode_v) == aiReturn_SUCCESS) {
              material.texture_bump_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
            }
            aiUVTransform transform{};
            if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_HEIGHT, 0), transform) == aiReturn_SUCCESS) {
              material.normal_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
              material.normal_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
              material.normal_rotation = glm::vec2(transform.mRotation, 1.0);
            }
          }
          material.double_sided = true;
        } else {
          if (mat->GetTextureCount(aiTextureType_NORMALS) > 0) {
            aiString texture_path;
            aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
            aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

            if (mat->GetTexture(aiTextureType_NORMALS, 0, &texture_path) == aiReturn_SUCCESS) {
                material.name_texture_bump = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
                material.name_texture_bump_path = root_path + texture_path.C_Str();
            }
            if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_NORMALS, 0), wrap_mode_u) == aiReturn_SUCCESS) {
              material.texture_bump_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
            }
            if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_NORMALS, 0), wrap_mode_v) == aiReturn_SUCCESS) {
              material.texture_bump_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
            }
            aiUVTransform transform{};
            if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_NORMALS, 0), transform) == aiReturn_SUCCESS) {
              material.normal_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
              material.normal_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
              material.normal_rotation = glm::vec2(transform.mRotation, 1.0);
            }
            //float strength{ 1.0 };
            //if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_NORMALS, 0), strength) == aiReturn_SUCCESS) {
            //  material.normal_strength = strength;
            //}
          }
        }
        
        if (mat->GetTextureCount(aiTextureType_OPACITY) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_OPACITY, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_alpha = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_alpha_path = root_path + texture_path.C_Str();
              if (material.alpha_mode < 0.5f) {
                material.alpha_mode = 1.0f;
                material.alpha_cut_off = 0.5f;
              }
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_OPACITY, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_alpha_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_OPACITY, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_alpha_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
        }
        if (mat->GetTextureCount(aiTextureType_UNKNOWN) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_metal_roughness = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_metal_roughness_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_UNKNOWN, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_metal_roughness_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_UNKNOWN, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_metal_roughness_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
          aiUVTransform transform{};
          if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_UNKNOWN, 0), transform) == aiReturn_SUCCESS) {
            material.mr_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
            material.mr_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
            material.mr_rotation = glm::vec2(transform.mRotation, 1.0);
          }
        }
        if (mat->GetTextureCount(aiTextureType_EMISSIVE) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_EMISSIVE, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_emissive = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_emissive_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_EMISSIVE, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_emissive_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_EMISSIVE, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_emissive_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
          aiUVTransform transform{};
          if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_EMISSIVE, 0), transform) == aiReturn_SUCCESS) {
            material.emissive_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
            material.emissive_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
            material.emissive_rotation = glm::vec2(transform.mRotation, 1.0);
          }
        }

        aiColor4D emissive_color(1.f);
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_color) == aiReturn_SUCCESS) {
          material.emissive_color = { emissive_color.r, emissive_color.g, emissive_color.b, emissive_color.a };
        }

        if (mat->GetTextureCount(aiTextureType_LIGHTMAP) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_LIGHTMAP, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_ao = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_ao_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_LIGHTMAP, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_ao_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_LIGHTMAP, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_ao_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
        }

        if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_base_color = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_base_color_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_BASE_COLOR, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_base_color_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_BASE_COLOR, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_base_color_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
        }
        if (mat->GetTextureCount(aiTextureType_TRANSMISSION) > 0) {
          aiString texture_path;
          aiTextureMapMode wrap_mode_u { aiTextureMapMode_Clamp };
          aiTextureMapMode wrap_mode_v { aiTextureMapMode_Clamp };

          if (mat->GetTexture(aiTextureType_TRANSMISSION, 0, &texture_path) == aiReturn_SUCCESS) {
              material.name_texture_transmission = AssimpLoader::cleanName(texture_path.C_Str(), texture_prefix);
              material.name_texture_transmission_path = root_path + texture_path.C_Str();
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_U(aiTextureType_TRANSMISSION, 0), wrap_mode_u) == aiReturn_SUCCESS) {
            material.texture_transmission_wrap_mode_u = getTextureWrapMode(wrap_mode_u);
          }
          if (mat->Get(AI_MATKEY_MAPPINGMODE_V(aiTextureType_TRANSMISSION, 0), wrap_mode_v) == aiReturn_SUCCESS) {
            material.texture_transmission_wrap_mode_v = getTextureWrapMode(wrap_mode_v);
          }
          aiUVTransform transform{};
          if (mat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_TRANSMISSION, 0), transform) == aiReturn_SUCCESS) {
            material.transmission_translation = glm::vec3(transform.mTranslation.x, transform.mTranslation.y, 1.0);
            material.transmission_scale = glm::vec3(transform.mScaling.x, transform.mScaling.y, 1.0);
            material.transmission_rotation = glm::vec2(transform.mRotation, 1.0);
          }
        }


        //int illumModel;
        //if (mat->Get(AI_MATKEY_ILLUM, illumModel) == aiReturn_SUCCESS) {
        //  material.illum = material.illum;
        //}
        int isDoubleSided { 0 };
        if (mat->Get(AI_MATKEY_TWOSIDED, isDoubleSided) == aiReturn_SUCCESS) {
          material.double_sided = (isDoubleSided == 1) ? true : false;
        }
        float glossiness_factor { 0.0f };
        if (mat->Get(AI_MATKEY_GLOSSINESS_FACTOR, glossiness_factor) == aiReturn_SUCCESS) {
          material.base_color = material.diffuse;
          bool const has_spec_tex { !material.name_texture_specular.empty() };
          if (has_spec_tex) {
            material.mre_factor.x = 0.0f;
            material.mre_factor.y = std::clamp(1.0f - glossiness_factor * 0.35f, 0.45f, 0.95f);
          } else {
            float const max_spec { std::max({ material.specular.r, material.specular.g, material.specular.b }) };
            material.mre_factor.x = (max_spec > 0.25f)
              ? std::clamp((max_spec - 0.04f) / 0.96f, 0.0f, 1.0f)
              : 0.0f;
            material.mre_factor.y = std::clamp(1.0f - glossiness_factor, 0.04f, 1.0f);
          }
        }
        materials.emplace_back(material);
      }
    }

    std::vector<Animation> animations{};
    std::unordered_map<std::string, std::vector<std::vector<Rotation>>> rotations{};
    std::unordered_map<std::string, std::vector<std::vector<Position>>> positions{};
    std::unordered_map<std::string, std::vector<std::vector<Scale>>> scales{};

    animations.reserve(scene->mNumAnimations);
    rotations.reserve(scene->mNumAnimations);
    positions.reserve(scene->mNumAnimations);
    scales.reserve(scene->mNumAnimations);

    for (std::uint32_t i{ 0 }; i < scene->mNumAnimations; i++) {

      aiAnimation const* animation { scene->mAnimations[i] };

      auto const ticks_per_s = (animation->mTicksPerSecond > 0) ? animation->mTicksPerSecond : 25.0;
      animations.emplace_back(i, animation->mName.C_Str(), animation->mDuration, ticks_per_s);

      for (std::uint32_t j{ 0 }; j < animation->mNumChannels; j++) {

        auto const* node{ animation->mChannels[j] };
        auto const node_name{ node->mNodeName.C_Str() };

        std::vector<Rotation>rots{};
        rots.reserve(node->mNumRotationKeys);

        for (std::uint32_t r{ 0 }; r < node->mNumRotationKeys; r++) {

          auto const& rotation_key{ node->mRotationKeys[r] };
          auto const interpolation{ getInterpolation(rotation_key.mInterpolation) };

          rots.emplace_back(Rotation{ {r, i, rotation_key.mTime, interpolation}, GetGLMQuat(rotation_key.mValue) });
        }
        // auto rot_duplicate = rots.front();
        // rot_duplicate.id = static_cast<std::uint32_t>(rots.size()) + 1ull;
        // rot_duplicate.time = animation->mDuration;
        // rots.emplace_back(rot_duplicate);
        rotations[node_name].push_back(rots);

        std::vector<Position> pos{};
        pos.reserve(node->mNumPositionKeys);

        for (std::uint32_t p{ 0 }; p < node->mNumPositionKeys; p++) {
          auto const& pos_key = node->mPositionKeys[p];
          auto const interpolation{ getInterpolation(pos_key.mInterpolation) };

          pos.emplace_back(Position{ { p, i, pos_key.mTime, interpolation }, GetGLMVec(pos_key.mValue) });
        }
        // auto pos_duplicate = pos.front();
        // pos_duplicate.id = static_cast<std::uint32_t>(pos.size()) + 1ull;
        // pos_duplicate.time = animation->mDuration;
        // pos.emplace_back(pos_duplicate);
        positions[node_name].push_back(pos);
        std::vector<Scale> sc{};
        sc.reserve(node->mNumScalingKeys);

        for (std::uint32_t s{ 0 }; s < node->mNumScalingKeys; s++) {
          auto const& scale_key = node->mScalingKeys[s];
          auto const interpolation{ getInterpolation(scale_key.mInterpolation) };

          sc.emplace_back(Scale{ {s, i, scale_key.mTime, interpolation }, GetGLMVec(scale_key.mValue) });
        }
        
        // auto sc_duplicate = sc.front();
        // sc_duplicate.id = static_cast<std::uint32_t>(sc.size()) + 1ull;
        // sc_duplicate.time = animation->mDuration;
        // sc.emplace_back(sc_duplicate);
        scales[node_name].push_back(sc);
      }
    }

    glm::mat4 const global_transform { ConvertMatrixToGLMFormat(scene->mRootNode->mTransformation) };
    auto const inverse_global_transform { glm::inverse(global_transform) };

    bool has_any_bones { false };
    for (std::uint32_t m{ 0 }; m < scene->mNumMeshes; ++m) {
      if (scene->mMeshes[m]->HasBones()) {
        has_any_bones = true;
        break;
      }
    }
    bool const has_skeleton { scene->HasAnimations() || has_any_bones };

    std::string root_bone_name{};
    std::unordered_map<std::string, Bone> bones_map{};
    std::uint32_t global_bone_count { 0 };

    if (has_skeleton) {
      std::unordered_set<std::string> bone_names{};
      getBoneHierarchy(scene->mRootNode, bone_names);
      auto const* root_bone { AssimpLoader::FindRootBone(scene, scene->mRootNode, bone_names) };
      if (root_bone != nullptr) {
        root_bone_name = root_bone->mName.C_Str();
      }
    }

    auto const t_meta { Clock::now() };

    // Extract and meshopt-optimize each unique aiMesh ONCE (in parallel for static scenes)
    std::vector<VMeshGeometry> processed_meshes(scene->mNumMeshes);
    if (scene->mNumMeshes > 0) {
      unsigned int const hw_threads { std::max(1u, std::thread::hardware_concurrency()) };
      bool const run_parallel { !has_any_bones && scene->mNumMeshes >= 4u && hw_threads > 1u };
      if (run_parallel) {
        unsigned int const worker_count { std::min<unsigned int>(hw_threads, scene->mNumMeshes) };
        std::atomic<std::uint32_t> next_mesh{ 0 };
        std::vector<std::thread> workers{};
        workers.reserve(worker_count);
        for (unsigned int t{ 0 }; t < worker_count; ++t) {
          workers.emplace_back([&]() {
            while (true) {
              std::uint32_t const idx { next_mesh.fetch_add(1u, std::memory_order_relaxed) };
              if (idx >= scene->mNumMeshes) {
                break;
              }
              processed_meshes[idx] = extractAiMesh(scene->mMeshes[idx], scene, flip_Y, nullptr, nullptr);
            }
          });
        }
        for (auto& worker : workers) {
          worker.join();
        }
      } else {
        for (std::uint32_t m{ 0 }; m < scene->mNumMeshes; ++m) {
          processed_meshes[m] = extractAiMesh(scene->mMeshes[m], scene, flip_Y, &bones_map, &global_bone_count);
        }
      }
    }

    if (has_skeleton) {
      addWeightlessBones(scene->mRootNode, bones_map, global_bone_count);
    }
    auto const t_extract { Clock::now() };

    std::size_t total_instances { 0 };
    countNodeInstances(scene->mRootNode, total_instances);

    std::vector<VMeshInstance> instances{};
    instances.reserve(total_instances);
    collectNodeInstances(
      scene->mRootNode,
      processed_meshes,
      instances,
      global_transform,
      inverse_global_transform);

    std::unordered_set<std::string> used_mesh_names{};
    used_mesh_names.reserve(instances.size());

    std::uint64_t id{ instances.size() };
    for (auto& inst : instances) {
      --id;
      if (!used_mesh_names.insert(inst.name).second) {
        std::string unique_name { inst.name + "#" + std::to_string(id) };
        while (!used_mesh_names.insert(unique_name).second) {
          unique_name += "_";
        }
        inst.name = std::move(unique_name);
      }
    }
    auto const t_collect { Clock::now() };

    VMeshAsset asset{
      .materials = std::move(materials),
      .animations = std::move(animations),
      .positions = std::move(positions),
      .rotations = std::move(rotations),
      .scales = std::move(scales),
      .bones_map = std::move(bones_map),
      .root_bone_name = std::move(root_bone_name),
      .geometries = std::move(processed_meshes),
      .instances = std::move(instances)
    };

    VMeshCache::save(file_name, root_path, flip_Y, asset);
    auto const t_save { Clock::now() };

    dispatchVMeshAsset(asset, texture_prefix, callback);
    auto const t_done { Clock::now() };

    Logger::info(
      "AssimpLoader [{}] ({} meshes / {} instances): ReadFile={}ms, Meta={}ms, Extract+meshopt={}ms, Collect={}ms, SaveVMesh={}ms, Callbacks+Tex={}ms, Total={}ms",
      file_name.filename().string(),
      scene->mNumMeshes,
      total_instances,
      ms_since(t_start, t_read),
      ms_since(t_read, t_meta),
      ms_since(t_meta, t_extract),
      ms_since(t_extract, t_collect),
      ms_since(t_collect, t_save),
      ms_since(t_save, t_done),
      ms_since(t_start, t_done));
  }

  std::string const AssimpLoader::cleanName(std::string const & name, std::string const& prefix)
  {
    std::string cleaned{};

    if (name.size() > 0) {
      std::size_t lastindex = name.find_last_of(".");
      cleaned = name.substr(0, lastindex);

      std::replace(cleaned.begin(), cleaned.end(), '\\', '_');
      std::replace(cleaned.begin(), cleaned.end(), '/', '_');
    }

    //Logger::debug("asset: {} -> {}", name, cleaned);

    return prefix + cleaned;
  }

  TextureWrapMode AssimpLoader::getTextureWrapMode(aiTextureMapMode const wrap_mode)
  {
    switch (wrap_mode)
    {
    case aiTextureMapMode_Clamp:
      return TextureWrapMode::CLAMP_TO_EDGE;
    case aiTextureMapMode_Mirror:
      return TextureWrapMode::MIRROR_REPEAT;
    case aiTextureMapMode_Wrap:
      return TextureWrapMode::WRAP;
    case _aiTextureMapMode_Force32Bit:
    case aiTextureMapMode_Decal:
      return TextureWrapMode::WRAP;
    default:
      return TextureWrapMode::WRAP;
    }
  }

  AnimInterpolation AssimpLoader::getInterpolation(aiAnimInterpolation const assimp_interpolation)
  {
    AnimInterpolation interpolation{ AnimInterpolation::STEP };
    switch (assimp_interpolation) {
    case aiAnimInterpolation_Linear :
      interpolation = AnimInterpolation::LINEAR;
      break;
    case aiAnimInterpolation_Spherical_Linear :
      interpolation = AnimInterpolation::SPHERICAL_LINEAR;
      break;
    case aiAnimInterpolation_Cubic_Spline:
      interpolation = AnimInterpolation::CUBIC_SPLINE;
      break;
    case aiAnimInterpolation_Step:
    case _aiAnimInterpolation_Force32Bit:
      interpolation = AnimInterpolation::LINEAR;
      break;
    default:
      interpolation = AnimInterpolation::LINEAR;
    }
    return interpolation;
  }

  aiNode const* AssimpLoader::FindRootBone(
    aiScene const * scene,
    aiNode const* node,
    std::unordered_set<std::string> const& bone_names)
    {
      if (bone_names.find(node->mName.C_Str()) != bone_names.end()) {
        aiNode const* current = node;
        while (current->mParent && 
               current->mParent != scene->mRootNode && 
               std::string(current->mParent->mName.C_Str()).find("RootNode") == std::string::npos) 
        {
            current = current->mParent;
        }
        return current;
    }

    for (std::uint32_t i = 0; i < node->mNumChildren; ++i) {
        aiNode const* found = FindRootBone(scene, node->mChildren[i], bone_names);
        if (found) return found;
    }

    return nullptr;
  }

  void AssimpLoader::getBoneHierarchy(aiNode* node, std::unordered_set<std::string>& bone_names)
  {
    bone_names.insert(node->mName.C_Str());
    
    for (std::size_t i { 0 }; i < node->mNumChildren; ++i) {
      getBoneHierarchy(node->mChildren[i], bone_names);
    }
  }

  void AssimpLoader::addWeightlessBones(aiNode const * node, std::unordered_map<std::string, Bone>& bones_map, std::uint32_t global_bone_count)
  {
    std::string const & name { node->mName.C_Str() };

    if (bones_map.find(name) == bones_map.end()) {
      Bone bone_data{};
      bone_data.name = name;
      bone_data.id = global_bone_count++;//mmh
      bone_data.offset_matrix = glm::mat4(1.0f);
      bone_data.transform = ConvertMatrixToGLMFormat(node->mTransformation);
      bone_data.is_weightless = true;

       glm::vec3 skew;
          glm::vec4 perspective;

        glm::decompose(
          bone_data.transform,
          bone_data.t_pose_scale,
          bone_data.t_pose_rotation,
          bone_data.t_pose_position,
          skew,
          perspective);

      if (node->mParent) 
          bone_data.parent_name = node->mParent->mName.C_Str();

      for (std::uint32_t i = 0; i < node->mNumChildren; i++) {
          bone_data.children.push_back(node->mChildren[i]->mName.C_Str());
      }
      bones_map[name] = bone_data;
    }

    for (std::uint32_t i { 0 }; i < node->mNumChildren; i++) {
      addWeightlessBones(node->mChildren[i], bones_map, global_bone_count);
    }
  }
}
