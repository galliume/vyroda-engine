/**
 * @file   Basic.cpp
 * @author  <Guillaume PERCEPIED>
 * @date   Sun Mar  8 19:39:22 2026
 * 
 * @brief  Most generic renderable component 
 * 
 */
module Engine.Renderer.Vulkan.Basic;

import std;

import Engine.Component.Mesh;

import Engine.Core.Constants;
import Engine.Core.Logger;
import Engine.Core.MaterialTypes;
import Engine.Core.GLM;
import Engine.Core.MeshTypes;
import Engine.Core.PlpTypedef;
import Engine.Core.Volk;

import Engine.Renderer.RendererComponentTypes;
import Engine.Renderer.VulkanRenderer;

namespace Vyroda
{
  void Basic::operator()(
    Renderer & renderer,
    Mesh & mesh,
    RendererContext const& render_context)
  {
    stage_flag_bits = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    if (!mesh.isDirty()) return;

    auto cmd_pool { renderer.getAPI().createCommandPool() };

    if (mesh.getUniformBuffers().empty()) {
      Buffer uniformBuffer { renderer.getAPI().createUniformBuffers(1, renderer.getCurrentFrameIndex()) };
      mesh.getUniformBuffers().emplace_back(std::move(uniformBuffer));

      for (std::size_t i{ 0 }; i < mesh.getData()->_ubos.size(); i++) {
        std::ranges::for_each(mesh.getData()->_ubos.at(i), [&](auto& ubo) {
          ubo.projection = renderer.getPerspective();
        });
      }
    }

    auto const& data { mesh.getData() };
    data->_texture_index = 0;

    if (data->_vertex_buffer.buffer == VK_NULL_HANDLE) {
      data->_vertex_buffer = renderer.getAPI().createVertexBuffer(data->_vertices, renderer.getCurrentFrameIndex());
    } else {
      //suppose we have to update data
      {
        data->_is_dirty = true;
        renderer.updateVertexBuffer(data, renderer.getCurrentFrameIndex());
      }
    }

    if (!data->_indices.empty()) {
     data->_indices_buffer = renderer.getAPI().createIndexBuffer(data->_indices, renderer.getCurrentFrameIndex());
    }

    //@todo store and share data per similar meshes
    auto const& mat { mesh.getMaterials().at(data->_material_id) };

    if (mesh.getStorageBuffers()->empty()) {
      Material material{};
      material.base_color = mat.base_color;
      material.ambient = mat.ambient;
      material.diffuse = mat.diffuse;
      material.specular = mat.specular;
      material.transmittance = mat.transmittance;
      material.mre_factor = mat.mre_factor;
      material.emissive_color = mat.emissive_color;

      material.shi_ior_diss = glm::vec3(
        mat.shininess,
        mat.ior,
        mat.dissolve);

      material.alpha = glm::vec3(mat.alpha_mode, mat.alpha_cut_off, 1.0);

      //@todo needed to modify assimp sources glTF2Asset.h textureTransformSupported = true (preview ?)
      material.ambient_translation = mat.ambient_translation;
      material.ambient_scale = mat.ambient_scale;
      material.ambient_rotation = { mat.ambient_rotation.x, mat.ambient_rotation.y, 1.0 };

      material.normal_translation = mat.normal_translation;
      material.normal_scale = mat.normal_scale;
      material.normal_rotation = { mat.normal_rotation.x, mat.normal_rotation.y, 1.0 };

      material.diffuse_translation = mat.diffuse_translation;
      material.diffuse_scale = mat.diffuse_scale;
      material.diffuse_rotation = { mat.diffuse_rotation.x, mat.diffuse_rotation.y, 1.0 };

      material.emissive_translation = mat.emissive_translation;
      material.emissive_scale = mat.emissive_scale;
      material.emissive_rotation = { mat.emissive_rotation.x, mat.emissive_rotation.y, 1.0 };

      material.mr_translation = mat.mr_translation;
      material.mr_scale = mat.mr_scale;
      material.mr_rotation = { mat.mr_rotation.x, mat.mr_rotation.y, 1.0 };

      material.strength = { mat.normal_strength, mat.occlusion_strength, 0.0 };//x normal strength, y occlusion strength

      //Logger::debug("ambient {}",  material.ambient.r);
      //Logger::debug("diffuse {}",  material.diffuse.r);
      //Logger::debug("specular {}",  material.specular.r);
      //Logger::debug("shi_ior_diss {} {}", material.shi_ior_diss.x, material.shi_ior_diss.y);

      ObjectBuffer objectBuffer{};
      objectBuffer.material = material;

      auto storageBuffer{ renderer.getAPI().createStorageBuffers(objectBuffer, renderer.getCurrentFrameIndex()) };

      mesh.setObjectBuffer(objectBuffer);
      mesh.addStorageBuffer(storageBuffer);
      mesh.setHasBufferStorage();
    }

    for (std::size_t i{ 0 }; i < mesh.getUniformBuffers().size(); ++i) {

      auto& ubos{ mesh.getUniformBuffers().at(i) };
      auto& ubos_data{ mesh.getData()->_ubos.at(i) };

      renderer.getAPI().updateUniformBuffer(ubos, &ubos_data, renderer.getCurrentFrameIndex());
    }

   if (mesh.getDescSet() == VK_NULL_HANDLE) {
     createDescriptorSet(renderer, mesh, render_context);
   }
    
    mesh.setIsDirty(false);
    vkDestroyCommandPool(renderer.getDevice(), cmd_pool, nullptr);
  }

  void Basic::createDescriptorSet(
    Renderer & renderer,
    Mesh & mesh,
    RendererContext const& render_context)
  {
    auto const& mat { mesh.getMaterials().at(mesh.getData()->_material_id) };

    auto const & main_texture_name {
      (!mesh.getData()->_base_color.empty()
      && mesh.getData()->_base_color != VYR_EMPTY)
        ? mesh.getData()->_base_color
        : mesh.getData()->_textures.at(0)
    };

    Texture tex { getTexture(render_context, main_texture_name) };
    tex.setSampler(renderer.getAPI().createKTXSampler(
      mat.texture_diffuse_wrap_mode_u,
      mat.texture_diffuse_wrap_mode_v,
      tex.getMipLevels()));

    Texture alpha { getTexture(render_context, mesh.getData()->_alpha) };
    alpha.setSampler(renderer.getAPI().createKTXSampler(
      mat.texture_alpha_wrap_mode_u,
      mat.texture_alpha_wrap_mode_v,
      alpha.getMipLevels()));

    Texture texture_bump{ getTexture(render_context, mesh.getData()->_bump_map) };
      texture_bump.setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::WRAP,
      TextureWrapMode::WRAP,
      1));

    if (texture_bump.getWidth() == 0) {
      texture_bump = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_specular{ getTexture(render_context, mesh.getData()->_specular_map)};
    texture_specular.setSampler(renderer.getAPI().createKTXSampler(
    mat.texture_specular_wrap_mode_u,
    mat.texture_specular_wrap_mode_v,
    texture_specular.getMipLevels()));

    if (texture_specular.getWidth() == 0) {
      texture_specular = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_metal_roughness { getTexture(render_context, mesh.getData()->_metal_roughness) };
    texture_metal_roughness.setSampler(renderer.getAPI().createKTXSampler(
    mat.texture_metal_roughness_wrap_mode_u,
    mat.texture_metal_roughness_wrap_mode_v,
    texture_metal_roughness.getMipLevels()));

    if (texture_metal_roughness.getWidth() == 0) {
      texture_metal_roughness = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_emissive { getTexture(render_context, mesh.getData()->_emissive) };
    texture_emissive.setSampler(renderer.getAPI().createKTXSampler(
    mat.texture_emissive_wrap_mode_u,
    mat.texture_emissive_wrap_mode_v,
    texture_emissive.getMipLevels()));

    if (texture_emissive.getWidth() == 0) {
      texture_emissive = render_context.textures->at(VYR_EMPTY);
    }

    Texture texture_ao { getTexture(render_context, mesh.getData()->_ao) };
    texture_ao.setSampler(renderer.getAPI().createKTXSampler(
    mat.texture_ao_wrap_mode_u,
    mat.texture_ao_wrap_mode_v,
    texture_ao.getMipLevels()));

    if (texture_ao.getWidth() == 0) {
      texture_ao = render_context.textures->at(VYR_EMPTY);
    }

    std::vector<VkDescriptorImageInfo> image_info{};
    image_info.reserve(7);
    image_info.emplace_back(tex.getSampler(), tex.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(alpha.getSampler(), alpha.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(texture_bump.getSampler(), texture_bump.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(texture_specular.getSampler(), texture_specular.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(texture_metal_roughness.getSampler(), texture_metal_roughness.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(texture_emissive.getSampler(), texture_emissive.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(texture_ao.getSampler(), texture_ao.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    image_info.emplace_back(tex.getSampler(), tex.getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> env_info{};

    render_context.skybox->setSampler(renderer.getAPI().createKTXSampler(
      TextureWrapMode::CLAMP_TO_EDGE,
      TextureWrapMode::CLAMP_TO_EDGE,
      0));

    env_info.emplace_back(render_context.skybox->getSampler(), render_context.skybox->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> depth_map_image_info{};
    depth_map_image_info.emplace_back(renderer.getDepthMapSamplers(), renderer.getDepthMapImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    std::vector<VkDescriptorImageInfo> csm_image_info{};
    csm_image_info.emplace_back(renderer.getCSMSamplers(), renderer.getCSMImageViews(), VK_IMAGE_LAYOUT_GENERAL);

    if (!renderer.getAPI().hasDescSet(mesh.getName())) {
       renderer.getAPI().createDescriptorSets(mesh.getName(), renderer.getPipeline(mesh.getShaderName()), 1);
          
      auto light_buffer {render_context.light_buffer};
    
      renderer.getAPI().updateDescriptorSets(
        mesh.getName(),                  
        image_info,
        depth_map_image_info,
        env_info,
        light_buffer,
        csm_image_info);
    }
  }

  Texture const& Basic::getTexture(
    RendererContext const& render_context,
    std::string const& name) const
  {
    auto tex_name { name };
    auto const& it { render_context.textures->find(name) };

    if (it == render_context.textures->end()) {
      //Logger::warn("Cannot load texture : {}", name);
      return render_context.textures->at(VYR_ERROR);
    }

    return it->second;
  }
}
