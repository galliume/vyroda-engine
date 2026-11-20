module;

//@todo find a way to encapsulate this in json.cppm
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

module Engine.Managers.TextureManager;

import std;

import Engine.Component.Texture;

import Engine.Core.Json;
import Engine.Core.KTX;
import Engine.Core.Logger;
import Engine.Core.PlpTypedef;
import Engine.Core.StbImage;
import Engine.Core.Volk;

import Engine.Managers.ConfigManagerLocator;

import Engine.Renderer;

namespace Vyroda
{
  std::vector<std::array<float, 3>> TextureManager::addNormalMapTexture(std::string const& name)
  {
    if (!_textures.contains(name)) {
      Logger::trace("Texture {} does not exists, can't create normal map", name);
      return {};
    }

    Texture& original_texture = _textures[name];
    auto const path = original_texture.getPath();

    if (!original_texture.getNormalMap().empty()) {
      return original_texture.getNormalMap();
    }

    int tex_width = 0, tex_height = 0, tex_channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    if (!pixels) {
      Logger::error("failed to load texture image %s", name);
      return {};
    }

    std::vector<std::array<float, 3>> normal_map;

    for (int y = 0; y < tex_height; ++y) {

      int ym1 = (y - 1) & (tex_height - 1);
      int yp1 = (y + 1) & (tex_height - 1);

      unsigned char* center_row = pixels + y * tex_width;
      unsigned char* upper_row = pixels + ym1 * tex_width;
      unsigned char* lower_row = pixels + yp1 * tex_width;

      for (int x = 0; x < tex_width; ++x) {
        int xm1 = (x - 1) & (tex_width - 1);
        int xp1 = (x + 1) & (tex_width - 1);

        float dx = (center_row[xp1] - center_row[xm1]) * 0.5f;
        float dy = (lower_row[x] - upper_row[x]) * 0.5f;

        float nz = 1.0f / std::sqrt(dx * dx + dy * dy + 1.0f);
        float nx = std::fmin(std::fmax(-dx * nz, -1.0f), 1.0f);
        float ny = std::fmin(std::fmax(-dy * nz, -1.0f), 1.0f);

        std::array<float, 3>d{ nx, ny, nz };
        normal_map.emplace_back(d);
      }
    }

    original_texture.setNormalMap(std::move(normal_map));

    return original_texture.getNormalMap();
  }

  TextureManager::TextureManager(Renderer const& renderer)
  {
    _cmd_pool = renderer.getAPI().createCommandPool(true);
  }

  namespace
  {
    ktx_transcode_fmt_e transcodeFormatFor(TEXTURE_TYPE const texture_type) noexcept
    {
      switch (texture_type) {
        case TEXTURE_TYPE::NORMAL:
          return KTX_TTF_BC5_RG;
        case TEXTURE_TYPE::AO:
        case TEXTURE_TYPE::HEIGHT:
          return KTX_TTF_BC4_R;
        case TEXTURE_TYPE::MR:
        case TEXTURE_TYPE::EMISSIVE:
        case TEXTURE_TYPE::DIFFUSE:
        default:
          return KTX_TTF_BC7_RGBA;
      }
    }
  }

  void TextureManager::addSkyBox(
    std::string const& skybox_name,
    std::vector<std::string> const&,
    Renderer const& renderer)
  {
    auto const root_path { ConfigManagerLocator::get()->rootPath() };
    std::string const path { root_path + "/assets/texture/" + skybox_name + ".ktx2" };

    if (!std::filesystem::exists(path)) {
      Logger::error(
        "Missing cooked skybox '{}'. Run: cmake --build --preset windows-release --target cook_assets",
        path);
      return;
    }

    addKTXTexture(skybox_name, path, VK_IMAGE_ASPECT_COLOR_BIT, KTX_TTF_BC1_RGB, renderer, true);
  }

  void TextureManager::addKTXTexture(
    std::string const& name,
    std::string const& path,
    VkImageAspectFlags const aspect_flags,
    ktx_transcode_fmt_e const transcoding,
    Renderer const& renderer,
    bool const is_public)
  {
    if (!std::filesystem::exists(path.c_str())) {
      Logger::critical("texture file {} does not exits.", path);
      //return;
    }

    if (0 != _textures.count(name.c_str())) {
      //Logger::trace("Texture {} already imported", name);
      return;
    }

    _paths.insert({ name, path });

    ktxTexture2 *ktx_texture { nullptr };
    ktx_error_code_e result = ktxTexture_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, (ktxTexture**)&ktx_texture);

    if (result != KTX_SUCCESS || ktx_texture == nullptr) {
      Logger::warn("Error while loading KTX file: {} error: {}", path, ktxErrorString(result));
      return;
    }

    if (ktxTexture2_NeedsTranscoding(ktx_texture)) {
      result = ktxTexture2_TranscodeBasis(ktx_texture, transcoding, 0);
    }

    if (result != KTX_SUCCESS) {
      Logger::warn("Error while transcoding KTX file: {} error: {}", path, ktxErrorString(result));
      destroyKtxTexture(ktx_texture);
      return;
    }
    
    VkCommandBuffer cmd_buffer {};
    VkImage texture_image {};
    {
      std::lock_guard<std::mutex> guard(_mutex_cmd_pool);
      cmd_buffer = renderer.getAPI().allocateCommandBuffers(_cmd_pool, 1, false)[0];
      texture_image = renderer.getAPI().createKTXImage(cmd_buffer, ktx_texture, _cmd_pool);
    }
    
    VkImageView texture_imageview { renderer.getAPI().createKTXImageView(ktx_texture, texture_image, aspect_flags) };

    Texture texture;
    texture.setName(name);
    texture.setImage(texture_image);
    texture.setImageView(texture_imageview);
    texture.setMipLevels(ktx_texture->numLevels);
    texture.setWidth(static_cast<std::uint32_t>(ktx_texture->baseWidth));
    texture.setHeight(static_cast<std::uint32_t>(ktx_texture->baseHeight));
    texture.setIsPublic(is_public);
    texture.setPath(path);

    destroyKtxTexture(ktx_texture);

    _textures.emplace(name, texture);
  }

  void TextureManager::clear()
  {
    _textures.clear();
    _texture_config.clear();
  }

  std::function<void(std::latch& count_down)> TextureManager::load(Renderer const& renderer)
  {

    auto const root_path { ConfigManagerLocator::get()->rootPath() };
    return [this, &renderer, root_path](std::latch& count_down) {
      
      for (auto& [key, texture_data] : _texture_config["textures"].items()) {
        auto const path { root_path + "/" + texture_data.at("path").get<std::string>() };
        add(key, path, VK_IMAGE_ASPECT_COLOR_BIT, TEXTURE_TYPE::DIFFUSE, renderer);
      }

      for (auto& [key, texture_data] : _texture_config["terrain"].items()) {
        setTerrainName(key);
        auto const path { root_path + "/" + texture_data.at("path").get<std::string>() };
        add(key, path, VK_IMAGE_ASPECT_COLOR_BIT, TEXTURE_TYPE::HEIGHT, renderer);
      }

      for (auto& [key, texture_data] : _texture_config["normal"].items()) {
        auto const path { root_path + "/" + texture_data.at("path").get<std::string>() };
        add(key, path, VK_IMAGE_ASPECT_COLOR_BIT, TEXTURE_TYPE::NORMAL, renderer);
      }

      for (auto& [key, texture_data] : _texture_config["water"].items()) {
        setWaterName(key);
        auto const path { root_path + "/" + texture_data.at("path").get<std::string>() };
        add(_water_name, path, VK_IMAGE_ASPECT_COLOR_BIT, TEXTURE_TYPE::DIFFUSE, renderer);
      }
      count_down.count_down();
    };
  }

  void TextureManager::add(
    std::string const& name,
    std::string const& file_path,
    VkImageAspectFlags const aspect_flags,
    TEXTURE_TYPE texture_type,
    Renderer const& renderer)
  {
    if (_textures.contains(name)) {
      return;
    }

    std::filesystem::path file_name_ktx { file_path };
    file_name_ktx.replace_extension("ktx2");

    if (!std::filesystem::exists(file_name_ktx)) {
      Logger::warn(
        "Missing cooked texture '{}'. Run: cmake --build --preset windows-release --target cook_assets",
        file_name_ktx.string());
      return;
    }

    addKTXTexture(name, file_name_ktx.string(), aspect_flags, transcodeFormatFor(texture_type), renderer, true);
  }

  void TextureManager::addBatch(
    std::span<TextureLoadRequest const> requests,
    Renderer const& renderer)
  {
    if (requests.empty()) {
      return;
    }

    using Clock = std::chrono::steady_clock;
    auto const t_start { Clock::now() };

    struct DecodedJob
    {
      std::string name{};
      std::filesystem::path ktx_path{};
      VkImageAspectFlags aspect_flags{ VK_IMAGE_ASPECT_COLOR_BIT };
      ktx_transcode_fmt_e transcoding{ KTX_TTF_BC7_RGBA };
      ktxTexture2* ktx_texture{ nullptr };
    };

    std::vector<DecodedJob> jobs{};
    jobs.reserve(requests.size());
    std::unordered_set<std::string> seen{};
    seen.reserve(requests.size());

    for (auto const& req : requests) {
      if (req.name.empty() || _textures.contains(req.name) || !seen.insert(req.name).second) {
        continue;
      }

      std::filesystem::path ktx_path { req.file_path };
      ktx_path.replace_extension("ktx2");

      if (!std::filesystem::exists(ktx_path)) {
        Logger::warn(
          "Missing cooked texture '{}'. Run: cmake --build --preset windows-release --target cook_assets",
          ktx_path.string());
        continue;
      }

      jobs.emplace_back(DecodedJob{
        .name = req.name,
        .ktx_path = std::move(ktx_path),
        .aspect_flags = req.aspect_flags,
        .transcoding = transcodeFormatFor(req.texture_type),
        .ktx_texture = nullptr
      });
    }

    if (jobs.empty()) {
      return;
    }

    auto const decode_one = [](DecodedJob& job) {
      ktxTexture2* ktx_tex { nullptr };
      std::string const ktx_path_str { job.ktx_path.string() };
      ktx_error_code_e result {
        ktxTexture_CreateFromNamedFile(
          ktx_path_str.c_str(),
          KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
          reinterpret_cast<ktxTexture**>(&ktx_tex))
      };

      if (result != KTX_SUCCESS || ktx_tex == nullptr) {
        Logger::warn("Error while loading KTX file: {} error: {}", ktx_path_str, ktxErrorString(result));
        return;
      }

      if (ktxTexture2_NeedsTranscoding(ktx_tex)) {
        result = ktxTexture2_TranscodeBasis(ktx_tex, job.transcoding, 0);
        if (result != KTX_SUCCESS) {
          Logger::warn("Error while transcoding KTX file: {} error: {}", ktx_path_str, ktxErrorString(result));
          destroyKtxTexture(ktx_tex);
          return;
        }
      }

      job.ktx_texture = ktx_tex;
    };

    unsigned int const hw_threads { std::max(1u, std::thread::hardware_concurrency()) };
    if (jobs.size() >= 2u && hw_threads > 1u) {
      unsigned int const worker_count {
        static_cast<unsigned int>(std::min<std::size_t>(hw_threads, jobs.size()))
      };
      std::atomic<std::size_t> next_idx { 0 };
      std::vector<std::thread> workers{};
      workers.reserve(worker_count);
      for (unsigned int t { 0 }; t < worker_count; ++t) {
        workers.emplace_back([&]() {
          while (true) {
            std::size_t const idx { next_idx.fetch_add(1u, std::memory_order_relaxed) };
            if (idx >= jobs.size()) {
              break;
            }
            decode_one(jobs[idx]);
          }
        });
      }
      for (auto& w : workers) {
        w.join();
      }
    } else {
      for (auto& job : jobs) {
        decode_one(job);
      }
    }

    auto const t_decoded { Clock::now() };

    std::vector<std::size_t> valid_indices{};
    std::vector<ktxTexture2*> ktx_ptrs{};
    valid_indices.reserve(jobs.size());
    ktx_ptrs.reserve(jobs.size());
    for (std::size_t i { 0 }; i < jobs.size(); ++i) {
      if (jobs[i].ktx_texture != nullptr) {
        valid_indices.push_back(i);
        ktx_ptrs.push_back(jobs[i].ktx_texture);
      }
    }

    std::vector<VkImage> uploaded_images{};
    if (!ktx_ptrs.empty()) {
      std::lock_guard<std::mutex> guard(_mutex_cmd_pool);
      uploaded_images = renderer.getAPI().createKTXImagesBatch(ktx_ptrs, _cmd_pool);
    }

    for (std::size_t k { 0 }; k < valid_indices.size(); ++k) {
      auto& job { jobs[valid_indices[k]] };
      VkImage const img { uploaded_images[k] };
      VkImageView const view {
        renderer.getAPI().createKTXImageView(job.ktx_texture, img, job.aspect_flags)
      };

      Texture texture{};
      texture.setName(job.name);
      texture.setImage(img);
      texture.setImageView(view);
      texture.setMipLevels(job.ktx_texture->numLevels);
      texture.setWidth(static_cast<std::uint32_t>(job.ktx_texture->baseWidth));
      texture.setHeight(static_cast<std::uint32_t>(job.ktx_texture->baseHeight));
      texture.setIsPublic(true);
      texture.setPath(job.ktx_path.string());

      _paths.insert({ job.name, job.ktx_path.string() });
      _textures.emplace(job.name, std::move(texture));
      destroyKtxTexture(job.ktx_texture);
      job.ktx_texture = nullptr;
    }

    auto const t_done { Clock::now() };
    Logger::info(
      "TextureManager::addBatch ({} textures): Decode+Transcode={}ms, GPUUpload={}ms, Total={}ms",
      valid_indices.size(),
      std::chrono::duration_cast<std::chrono::milliseconds>(t_decoded - t_start).count(),
      std::chrono::duration_cast<std::chrono::milliseconds>(t_done - t_decoded).count(),
      std::chrono::duration_cast<std::chrono::milliseconds>(t_done - t_start).count());
  }

  std::function<void(std::latch& count_down)> TextureManager::loadSkybox(
    std::string_view skybox,
    Renderer const& renderer
  )
  {
    _skybox_name = skybox;
    auto const root_path { ConfigManagerLocator::get()->rootPath() };

    return [this, &renderer, root_path](std::latch& count_down) {
      std::vector<std::string>skybox_images;

      for (auto& texture : _texture_config["skybox"][_skybox_name].items()) {
        skybox_images.emplace_back(root_path + "/" + texture.value().get<std::string>());
      }

      addSkyBox(_skybox_name, skybox_images, renderer);
      count_down.arrive_and_wait();
    };
  }
}
