module;
#include <cstddef>//offsetof

export module Engine.Core.Vertex;

import std;

import Engine.Core.GLM;
import Engine.Core.Volk;

namespace Vyroda
{
  /*
    format:
    float : VK_FORMAT_R32_SFLOAT
    vec2 : VK_FORMAT_R32G32_SFLOAT
    vec3 : VK_FORMAT_R32G32B32_SFLOAT
    vec4 : VK_FORMAT_R32G32B32A32_SFLOAT

    float alignas(4)
    vec2 alignas(8)
    vec3/vec4/mat4 alignas(16)
    */
  export struct VertexBones
  {
    std::int32_t bone_ids[4];        // 16 bytes
    float   bone_weights[4];    // 16 bytes

    glm::vec3 original_pos;     // 12 bytes
    
    float total_weight;         // 4 bytes
  };

  export struct Vertex
  {
    alignas(16) glm::vec4 tangent;
    alignas(16) glm::vec4 color;
    alignas(16) glm::vec4 normal;
    alignas(8) glm::vec2 texture_coord;

    static VkVertexInputBindingDescription getPositionBindingDescription()
    {
      VkVertexInputBindingDescription bindingDescription{};
      bindingDescription.binding = 0;
      bindingDescription.stride = sizeof(glm::vec3);
      bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      return bindingDescription;
    }

    static VkVertexInputAttributeDescription getPositionAttributeDescription()
    {
      VkVertexInputAttributeDescription attributeDescription{};
      attributeDescription.binding = 0;
      attributeDescription.location = 2;
      attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescription.offset = 0;

      return attributeDescription;
    }

    static std::array<VkVertexInputBindingDescription, 2> getBindingDescriptions()
    {
      std::array<VkVertexInputBindingDescription, 2> bindingDescriptions{};
      // Binding 0: Position stream (12 bytes per vertex)
      bindingDescriptions[0].binding = 0;
      bindingDescriptions[0].stride = sizeof(glm::vec3);
      bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      // Binding 1: Shading attribute stream (64 bytes per vertex)
      bindingDescriptions[1].binding = 1;
      bindingDescriptions[1].stride = sizeof(Vertex);
      bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      return bindingDescriptions;
    }

    static std::array<VkVertexInputAttributeDescription,  5> getAttributeDescriptions()
    {
      std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};
      attributeDescriptions[0].binding = 1;
      attributeDescriptions[0].location = 0;
      attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      attributeDescriptions[0].offset = offsetof(Vertex, tangent);

      // Location 1: Color (vec4)
      attributeDescriptions[1].binding = 1;
      attributeDescriptions[1].location = 1;
      attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      attributeDescriptions[1].offset = offsetof(Vertex, color);

      // Location 2: Position (vec3, Binding 0)
      attributeDescriptions[2].binding = 0;
      attributeDescriptions[2].location = 2;
      attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescriptions[2].offset = 0;

      // Location 3: Normal (vec4)
      attributeDescriptions[3].binding = 1;
      attributeDescriptions[3].location = 3;
      attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      attributeDescriptions[3].offset = offsetof(Vertex, normal);

      // Location 4: Texture Coord (vec2)
      attributeDescriptions[4].binding = 1;
      attributeDescriptions[4].location = 4;
      attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
      attributeDescriptions[4].offset = offsetof(Vertex, texture_coord);

      return attributeDescriptions;
    }
  };
}
