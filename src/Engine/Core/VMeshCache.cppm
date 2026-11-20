export module Engine.Core.VMeshCache;

import std;

import Engine.Core.AnimationTypes;
import Engine.Core.BoneTypes;
import Engine.Core.Constants;
import Engine.Core.GLM;
import Engine.Core.MaterialTypes;
import Engine.Core.Vertex;

namespace Vyroda
{
  export struct VMeshGeometry
  {
    std::string base_name{};
    std::uint32_t material_ID{ 0 };
    glm::vec3 bbox_min{};
    glm::vec3 bbox_max{};
    std::vector<glm::vec3> positions{};
    std::vector<Vertex> vertices{};
    std::vector<VertexBones> vertices_bones{};
    std::vector<std::uint32_t> indices{};
  };

  export struct VMeshInstance
  {
    std::uint32_t geometry_idx{ 0 };
    std::string name{};
    glm::mat4 transform_matrix{ 1.0f };
    glm::mat4 local_transform{ 1.0f };
    glm::mat4 inverse_transform_matrix{ 1.0f };
  };

  export struct VMeshAsset
  {
    std::vector<material_t> materials{};
    std::vector<Animation> animations{};
    std::unordered_map<std::string, std::vector<std::vector<Position>>> positions{};
    std::unordered_map<std::string, std::vector<std::vector<Rotation>>> rotations{};
    std::unordered_map<std::string, std::vector<std::vector<Scale>>> scales{};
    std::unordered_map<std::string, Bone> bones_map{};
    std::string root_bone_name{};
    std::vector<VMeshGeometry> geometries{};
    std::vector<VMeshInstance> instances{};
  };

  export class VMeshCache
  {
  public:
    static std::filesystem::path cachePathFor(std::filesystem::path const& src_path);

    static bool tryLoad(
      std::filesystem::path const& src_path,
      std::string const& root_path,
      bool const flip_Y,
      VMeshAsset& out_asset);

    static bool save(
      std::filesystem::path const& src_path,
      std::string const& root_path,
      bool const flip_Y,
      VMeshAsset const& asset);
  };
}
