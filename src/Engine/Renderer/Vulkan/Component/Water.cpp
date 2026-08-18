module Engine.Renderer.Vulkan.Water;

import std;

import Engine.Component.Mesh;
import Engine.Component.Texture;

import Engine.Core.Constants;
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

void Water::operator()(
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

    std::vector<Vertex> vertices;
    int const width{ static_cast<int>(render_context.terrain->getWidth()) };
    int const height{ static_cast<int>(render_context.terrain->getHeight()) };

    int const rez{ 50 };
    int index{ 0 };
    float const fRez { static_cast<float>(rez) };
    glm::vec4 const tangentDefault{ 1.0f, 0.0f, 0.0f, 1.0f };
    glm::vec4 const normalDefault{ 0.0f, 1.0f, 0.0f, 0.0f };


    for(auto i = 0; i < rez - 1; i++) {
      for(auto j = 0; j < rez - 1; j++) {

        index = i + j;
        float const y{ 0.0f };

        // Vertex v1
        Vertex v{
            tangentDefault,                      // tangent (vec4 - 16 bytes)
            { (float)index, 0.0f, 0.0f, 0.0f },  // color (vec4 - 16 bytes)
            { -width/2.0f + width*i/fRez, y, -height/2.0f + height*j/fRez, 1.0f }, // pos (vec3)
            normalDefault,                       // normal (vec3)
            { i / fRez, j / fRez }              // texture_coord (vec2)
        };

        // Vertex v2
        Vertex v2{
            tangentDefault,
            { (float)index, 0.0f, 0.0f, 0.0f },
            { -width/2.0f + width*(i+1)/fRez, y, -height/2.0f + height*j/fRez, 1.0f },
            normalDefault,
            { (i+1) / fRez, j / fRez }
        };

        // Vertex v3
        Vertex v3{
            tangentDefault,
            { (float)index, 0.0f, 0.0f, 0.0f },
            { -width/2.0f + width*i/fRez, y, -height/2.0f + height*(j+1)/fRez, 1.0f },
            normalDefault,
            { i / fRez, (j+1) / fRez }
        };

        // Vertex v4
        Vertex v4{
            tangentDefault,
            { (float)index, 0.0f, 0.0f, 0.0f },
            { -width/2.0f + width*(i+1)/fRez, y, -height/2.0f + height*(j+1)/fRez, 1.0f },
            normalDefault,
            { (i+1) / fRez, (j+1) / fRez }
        };

        vertices.push_back(v);
        vertices.push_back(v2);
        vertices.push_back(v3);
        vertices.push_back(v4);
      }
    }
    mesh.setName("__water__");
    auto const& data = mesh.getData();
    auto cmd_pool = renderer.getAPI().createCommandPool();

    std::vector<UniformBufferObject> ubos{};
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.projection = renderer.getPerspective();
    ubos.push_back(ubo);

    data->_ubos.push_back(ubos);
    data->_vertices = vertices;
    data->_vertex_buffer = renderer.getAPI().createVertexBuffer(vertices, renderer.getCurrentFrameIndex());
    data->_texture_index = 0;

    auto& mat { mesh.getMaterials().at(0) };
    mat.alpha_mode = 2.0;//BLEND

    mesh.getData()->_ubos_offset.emplace_back(1);
    mesh.getUniformBuffers().emplace_back(renderer.getAPI().createUniformBuffers(1, renderer.getCurrentFrameIndex()));

    for (std::size_t i{ 0 }; i < mesh.getData()->_ubos.size(); i++) {
      std::ranges::for_each(mesh.getData()->_ubos.at(i), [&](auto& data_ubo) {
        data_ubo.projection = renderer.getPerspective();
      });
    }

    vkDestroyCommandPool(renderer.getDevice(), cmd_pool, nullptr);

    if (!mesh.getData()->_ubos.empty()) {
      renderer.getAPI().updateUniformBuffer(mesh.getUniformBuffers().at(0), &mesh.getData()->_ubos.at(0), renderer.getCurrentFrameIndex());
    }

    if (!renderer.getAPI().hasDescSet(mesh.getName())) {
      createDescriptorSet(renderer, mesh, render_context);
    }

    mesh.setIsDirty(false);
  }

  void Water::createDescriptorSet(
    Renderer & renderer,
    Mesh & mesh,
    RendererContext const& render_context)
  {

    Texture tex { render_context.textures->at(VYR_EMPTY)};

    tex.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      0));

    if (tex.getWidth() == 0) {
      tex = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_normal{ render_context.textures->at(VYR_WATER_NORMAL_1) };
      texture_normal.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      1));

    if (texture_normal.getWidth() == 0) {
      texture_normal = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_normal2{ render_context.textures->at(VYR_WATER_NORMAL_2) };
      texture_normal2.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      1));

    if (texture_normal2.getWidth() == 0) {
      texture_normal2 = render_context.textures->at(VYR_EMPTY);
    }

    render_context.skybox->setSampler(renderer.getAPI().createKTXSampler(
    TextureWrapMode::CLAMP_TO_EDGE,
    TextureWrapMode::CLAMP_TO_EDGE,
    render_context.skybox->getMipLevels()));

    std::vector<VkDescriptorImageInfo> image_infos{};
    image_infos.emplace_back(tex.getSampler(), tex.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(renderer.getDepthSamplers(), renderer.getDepthImageViews(), VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
    image_infos.emplace_back(texture_normal.getSampler(), texture_normal.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(texture_normal2.getSampler(), texture_normal2.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_infos.emplace_back(renderer.getCurrentSampler(), renderer.getCurrentImageView(), VK_IMAGE_LAYOUT_GENERAL);
    
    render_context.skybox->setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::CLAMP_TO_EDGE,
      TextureWrapMode::CLAMP_TO_EDGE,
      0));

    std::vector<VkDescriptorImageInfo> env_infos{};
    env_infos.emplace_back(render_context.skybox->getSampler(), render_context.skybox->getImageView(), VK_IMAGE_LAYOUT_GENERAL );

    std::vector<VkDescriptorImageInfo> csm_image_info{};
    csm_image_info.emplace_back(renderer.getCSMSamplers(), renderer.getCSMImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> depth_map_image_info{};
    depth_map_image_info.emplace_back(renderer.getDepthMapSamplers(), renderer.getDepthMapImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    renderer.getAPI().createDescriptorSets(mesh.getName(), renderer.getPipeline(mesh.getShaderName()), 1);

    auto light_buffer {render_context.light_buffer};

    renderer.getAPI().updateDescriptorSets(
      mesh.getName(),
      image_infos,
      depth_map_image_info,
      env_infos,
      light_buffer,
      csm_image_info);
  }
}
