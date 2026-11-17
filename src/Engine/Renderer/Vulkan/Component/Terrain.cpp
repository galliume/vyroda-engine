module Engine.Renderer.Vulkan.Terrain;

import std;

import Engine.Component.Mesh;
import Engine.Component.Texture;

import Engine.Core.Constants;
import Engine.Core.Logger;
import Engine.Core.MaterialTypes;
import Engine.Core.GLM;
import Engine.Core.MeshTypes;
import Engine.Core.PlpTypedef;
import Engine.Core.Vertex;
import Engine.Core.Volk;

import Engine.Renderer.RendererComponentTypes;
import Engine.Renderer.VulkanRenderer;

namespace Vyroda
{
  struct Texture_Region
  {
    int lowest_height;
    int optimal_height;
    int heighest_height;
  };

  void Terrain::operator()(
    Renderer & renderer,
    Mesh & mesh,
    RendererContext const& render_context)
  {
     stage_flag_bits =
      VK_SHADER_STAGE_VERTEX_BIT
      | VK_SHADER_STAGE_FRAGMENT_BIT
      | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
      | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;


    if (!mesh.isDirty()) return;

    std::vector<Vertex> vertices {};
    std::int32_t const width { static_cast<std::int32_t>( render_context.terrain->getWidth()) };
    std::int32_t const height { static_cast<std::int32_t>( render_context.terrain->getHeight()) };
    std::int32_t const rez { 20 };
    float const fRez { static_cast<float>(rez) };
    glm::vec4 const zeroVec4 { 0.0f, 0.0f, 0.0f, 0.0f };
    glm::vec4 const defaultNormal { 1.0f, 1.0f, 0.0f, 0.0f };

    for(std::int32_t i { 0 }; i < rez - 1; i++) {
      for(std::int32_t j { 0 }; j < rez - 1; j++) {

        float const y { 0.0f };

        // Vertex v1
        Vertex v{
            zeroVec4, // tangent
            zeroVec4, // color
            { -width/2.0f + width*i/fRez, y, -height/2.0f + height*j/fRez, 1.0f }, // pos
            defaultNormal, // normal
            { i / fRez, j / fRez } // texture_coord
        };

        // Vertex v2
        Vertex v2{
            zeroVec4,
            zeroVec4,
            { -width/2.0f + width*(i+1)/fRez, y, -height/2.0f + height*j/fRez, 1.0f },
            defaultNormal,
            { (i+1) / fRez, j / fRez }
        };

        // Vertex v3
        Vertex v3{
            zeroVec4,
            zeroVec4,
            { -width/2.0f + width*i/fRez, y, -height/2.0f + height*(j+1)/fRez, 1.0f },
            defaultNormal,
            { i / fRez, (j+1) / fRez }
        };

        // Vertex v4
        Vertex v4{
            zeroVec4,
            zeroVec4,
            { -width/2.0f + width*(i+1)/fRez, y, -height/2.0f + height*(j+1)/fRez, 1.0f },
            defaultNormal,
            { (i+1) / fRez, (j+1) / fRez }
        };

        vertices.push_back(v);
        vertices.push_back(v2);
        vertices.push_back(v3);
        vertices.push_back(v4);
      }
    }
    mesh.setName("__terrain__");
    auto data = mesh.getData();
    auto commandPool = renderer.getAPI().createCommandPool();

    std::vector<UniformBufferObject> ubos{};
    ubos.reserve(1);
    UniformBufferObject ubo;
    ubo.model = glm::mat4(1.0f);
    ubo.projection = renderer.getPerspective();
    ubos.push_back(ubo);

    data->_vertices = vertices;
    if (data->_vertex_buffer.buffer == VK_NULL_HANDLE) {
      Logger::debug("create vertex buffer: {}", "terrain");
      data->_vertex_buffer = renderer.getAPI().createVertexBuffer(vertices, renderer.getCurrentFrameIndex());
    }

    data->_texture_index = 0;
    data->_ubos.resize(1);
    data->_ubos[0] = ubos;

    mesh.getData()->_ubos_offset.emplace_back(1);
    mesh.getUniformBuffers().emplace_back(renderer.getAPI().createUniformBuffers(1, renderer.getCurrentFrameIndex()));

    for (std::size_t i{ 0 }; i < mesh.getData()->_ubos.size(); i++) {
      std::ranges::for_each(mesh.getData()->_ubos.at(i), [&](auto& data_ubo) {
        data_ubo.projection = renderer.getPerspective();
      });
    }

    vkDestroyCommandPool(renderer.getDevice(), commandPool, nullptr);

    if (!mesh.getData()->_ubos.empty()) {
      renderer.getAPI().updateUniformBuffer(mesh.getUniformBuffers().at(0), &mesh.getData()->_ubos.at(0), renderer.getCurrentFrameIndex());
    }

    createDescriptorSet(renderer, mesh, render_context);
    mesh.setIsDirty(false);
  }

  void Terrain::createDescriptorSet(
    Renderer & renderer,
    Mesh & mesh,
    RendererContext const& render_context)
  {
    auto& height_map { render_context.terrain };

    height_map->setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    //@todo fix this ugly fix. Needs a real asset unique ID
    Texture ground { render_context.textures->at(VYR_GROUND) };

    ground.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (ground.getWidth() == 0) {
      ground = render_context.textures->at(VYR_EMPTY);
    }

    Texture grass { render_context.textures->at(VYR_GRASS) };

    grass.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (grass.getWidth() == 0) {
      grass = render_context.textures->at(VYR_EMPTY);
    }

    Texture snow { render_context.textures->at(VYR_SNOW) };

    snow.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (snow.getWidth() == 0) {
      snow = render_context.textures->at(VYR_EMPTY);
    }

    Texture sand { render_context.textures->at(VYR_SAND) };

    sand.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (sand.getWidth() == 0) {
      sand = render_context.textures->at(VYR_EMPTY);
    }

    Texture low_noise { render_context.textures->at(VYR_LOW_NOISE) };

    low_noise.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (low_noise.getWidth() == 0) {
      low_noise = render_context.textures->at(VYR_EMPTY);
    }

    Texture hi_noise { render_context.textures->at(VYR_HI_NOISE) };

    hi_noise.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (hi_noise.getWidth() == 0) {
      hi_noise = render_context.textures->at(VYR_EMPTY);
    }

    std::vector<VkDescriptorImageInfo> image_infos{};
    image_infos.emplace_back(height_map->getSampler(), height_map->getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(ground.getSampler(), ground.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(grass.getSampler(), grass.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(snow.getSampler(), snow.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(sand.getSampler(), sand.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(hi_noise.getSampler(), hi_noise.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(low_noise.getSampler(), low_noise.getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    render_context.terrain->setSampler(renderer.getAPI().createKTXSampler(
    TextureWrapMode::CLAMP_TO_EDGE,
    TextureWrapMode::CLAMP_TO_EDGE,
    render_context.terrain->getMipLevels()));

    std::vector<VkDescriptorImageInfo> env_image_infos{};
    env_image_infos.emplace_back(render_context.terrain->getSampler(), render_context.terrain->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> csm_image_info{};
    csm_image_info.emplace_back(renderer.getCSMSamplers(), renderer.getCSMImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> depth_map_image_info{};
    depth_map_image_info.emplace_back(renderer.getDepthMapSamplers(), renderer.getDepthMapImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    render_context.skybox->setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::CLAMP_TO_EDGE,
      TextureWrapMode::CLAMP_TO_EDGE,
      0));

    std::vector<VkDescriptorImageInfo> env_infos{};
    env_infos.emplace_back(render_context.skybox->getSampler(), render_context.skybox->getImageView(), VK_IMAGE_LAYOUT_GENERAL );

    if (!renderer.getAPI().hasDescSet(mesh.getName())) {
      renderer.getAPI().createDescriptorSets(mesh.getName(), renderer.getPipeline(mesh.getShaderName()) , 1);

      auto light_buffer { render_context.light_buffer };

      renderer.getAPI().updateDescriptorSets(
        mesh.getName(),
        image_infos,
        depth_map_image_info,
        env_infos,
        light_buffer,
        csm_image_info);
      }
  }
}
