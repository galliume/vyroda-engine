#include <nlohmann/json.hpp>

import std;

import Engine.Core.AnimationTypes;
import Engine.Core.AssimpLoader;
import Engine.Core.Json;
import Engine.Core.Logger;
import Engine.Core.MaterialTypes;
import Engine.Core.PlpTypedef;
import Engine.Core.VMeshCache;

namespace
{
  struct CookerOptions
  {
    std::filesystem::path root { std::filesystem::current_path() };
    std::string level{};
    std::string mesh{};
    unsigned int jobs { std::max(1u, std::thread::hardware_concurrency()) };
    bool force { false };
  };

  struct MeshCookJob
  {
    std::filesystem::path path{};
    bool flip_y { false };
  };

  enum class CookTextureType : std::uint8_t
  {
    DIFFUSE,
    NORMAL,
    MR,
    EMISSIVE,
    AO,
    HEIGHT
  };

  struct TextureCookJob
  {
    std::filesystem::path src_path{};
    CookTextureType type { CookTextureType::DIFFUSE };
  };

  struct SkyboxCookJob
  {
    std::string name{};
    std::vector<std::filesystem::path> faces{};
    std::filesystem::path out_ktx{};
  };

  void printUsage(std::string_view const exe)
  {
    std::println(
      "Usage: {} [--root <dir>] [--level <name>] [--mesh <path>] [--jobs <N>] [--force]",
      exe);
  }

  std::optional<CookerOptions> parseArgs(int const argc, char const* const* const argv)
  {
    CookerOptions opts{};
    auto const args { std::span<char const* const>(argv, static_cast<std::size_t>(argc)) };

    for (std::size_t i { 1u }; i < args.size(); ++i) {
      std::string_view const arg { args[i] };

      if (arg == "-h" || arg == "--help") {
        printUsage(args[0]);
        return std::nullopt;
      }
      if (arg == "--force") {
        opts.force = true;
      } else if (arg == "--root" && (i + 1u) < args.size()) {
        opts.root = args[++i];
      } else if (arg == "--level" && (i + 1u) < args.size()) {
        opts.level = args[++i];
      } else if (arg == "--mesh" && (i + 1u) < args.size()) {
        opts.mesh = args[++i];
      } else if (arg == "--jobs" && (i + 1u) < args.size()) {
        opts.jobs = static_cast<unsigned int>(std::max(1, std::stoi(args[++i])));
      } else {
        std::println(std::cerr, "Unknown or incomplete argument: {}", arg);
        printUsage(args[0]);
        return std::nullopt;
      }
    }

    return opts;
  }

  std::vector<std::filesystem::path> collectLevelFiles(CookerOptions const& opts)
  {
    std::vector<std::filesystem::path> files{};
    auto const levels_dir { opts.root / "config" / "levels" };
    std::error_code ec{};

    if (!opts.level.empty()) {
      auto level_path { levels_dir / opts.level };
      if (level_path.extension() != ".json") {
        level_path.replace_extension(".json");
      }
      if (std::filesystem::exists(level_path, ec)) {
        files.emplace_back(std::move(level_path));
      } else {
        Vyroda::Logger::warn("Level config not found: {}", level_path.string());
      }
      return files;
    }

    if (!std::filesystem::exists(levels_dir, ec)) {
      Vyroda::Logger::warn("Levels directory not found: {}", levels_dir.string());
      return files;
    }

    for (auto const& entry : std::filesystem::directory_iterator(levels_dir, ec)) {
      if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
        files.emplace_back(entry.path());
      }
    }
    std::ranges::sort(files);
    return files;
  }

  std::vector<MeshCookJob> discoverMeshes(CookerOptions const& opts)
  {
    std::vector<MeshCookJob> jobs{};
    std::unordered_set<std::string> seen{};
    std::error_code ec{};

    auto add_job = [&](std::filesystem::path const& raw_path, bool const flip_y) {
      auto const full {
        (raw_path.is_absolute() ? raw_path : (opts.root / raw_path)).lexically_normal()
      };
      if (!std::filesystem::exists(full, ec)) {
        Vyroda::Logger::warn("Mesh file not found, skipping: {}", full.string());
        return;
      }
      std::string const key { full.generic_string() + (flip_y ? "|1" : "|0") };
      if (seen.insert(key).second) {
        jobs.emplace_back(MeshCookJob{ .path = full, .flip_y = flip_y });
      }
    };

    if (!opts.mesh.empty()) {
      add_job(opts.mesh, false);
      return jobs;
    }

    for (auto const& level_file : collectLevelFiles(opts)) {
      std::ifstream ifs(level_file);
      if (!ifs) {
        Vyroda::Logger::warn("Could not open level config: {}", level_file.string());
        continue;
      }

      Vyroda::json doc{};
      try {
        doc = Vyroda::json::parse(ifs);
      } catch (std::exception const& e) {
        Vyroda::Logger::warn("Failed to parse {}: {}", level_file.string(), e.what());
        continue;
      }

      if (!doc.contains("entities") || !doc["entities"].is_object()) {
        continue;
      }

      for (auto const& [entity_name, ent] : doc["entities"].items()) {
        auto const rel_mesh { ent.value("mesh", std::string{}) };
        if (rel_mesh.empty()) {
          continue;
        }
        bool const flip_y { ent.value("flipY", false) };
        add_job(rel_mesh, flip_y);
      }
    }

    return jobs;
  }

  std::vector<TextureCookJob> cookMeshesAndCollectTextures(
    std::vector<MeshCookJob> const& mesh_jobs,
    CookerOptions const& opts)
  {
    using Clock = std::chrono::steady_clock;
    auto const t_start { Clock::now() };

    std::vector<TextureCookJob> tex_jobs{};
    std::unordered_set<std::string> seen_textures{};
    std::size_t cooked_count { 0u };
    std::size_t cached_count { 0u };
    std::error_code ec{};

    auto register_texture = [&](std::string const& raw_path, CookTextureType const type) {
      if (raw_path.empty()) {
        return;
      }
      auto const full { std::filesystem::path(raw_path).lexically_normal() };
      std::string const key { full.generic_string() };
      if (seen_textures.insert(key).second) {
        tex_jobs.emplace_back(TextureCookJob{ .src_path = full, .type = type });
      }
    };

    for (auto const& job : mesh_jobs) {
      auto const cache_path { Vyroda::VMeshCache::cachePathFor(job.path) };
      if (opts.force && std::filesystem::exists(cache_path, ec)) {
        std::filesystem::remove(cache_path, ec);
      }

      auto const had_cache { std::filesystem::exists(cache_path, ec) };
      auto const old_mtime {
        had_cache ? std::filesystem::last_write_time(cache_path, ec)
                  : std::filesystem::file_time_type{}
      };

      bool collected_materials { false };
      Vyroda::AssimpLoader::loadData(
        job.path.generic_string(),
        job.flip_y,
        [&](
          Vyroda::PlpMeshData&&,
          std::vector<Vyroda::material_t> const& materials,
          std::vector<Vyroda::Animation> const&,
          std::unordered_map<std::string, std::vector<std::vector<Vyroda::Position>>> const&,
          std::unordered_map<std::string, std::vector<std::vector<Vyroda::Rotation>>> const&,
          std::unordered_map<std::string, std::vector<std::vector<Vyroda::Scale>>> const&) {
          if (collected_materials) {
            return;
          }
          collected_materials = true;
          for (auto const& m : materials) {
            if (!m.name_texture_diffuse_path.empty()) {
              register_texture(m.name_texture_diffuse_path, CookTextureType::DIFFUSE);
            } else if (!m.name_texture_ambient_path.empty()) {
              register_texture(m.name_texture_ambient_path, CookTextureType::DIFFUSE);
            }
            register_texture(m.name_texture_base_color_path, CookTextureType::DIFFUSE);
            register_texture(m.name_texture_bump_path, CookTextureType::NORMAL);
            register_texture(m.name_texture_metal_roughness_path, CookTextureType::MR);
            register_texture(m.name_texture_emissive_path, CookTextureType::EMISSIVE);
            register_texture(m.name_texture_transmission_path, CookTextureType::EMISSIVE);
            register_texture(m.name_texture_specular_path, CookTextureType::AO);
            register_texture(m.name_texture_alpha_path, CookTextureType::AO);
            register_texture(m.name_texture_ao_path, CookTextureType::AO);
          }
        });

      bool const has_cache_now { std::filesystem::exists(cache_path, ec) };
      if (has_cache_now) {
        auto const new_mtime { std::filesystem::last_write_time(cache_path, ec) };
        if (!had_cache || new_mtime != old_mtime) {
          ++cooked_count;
        } else {
          ++cached_count;
        }
      }
    }

    auto const elapsed_ms {
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count()
    };
    Vyroda::Logger::info(
      "Pass 1 (.vmesh) done in {}ms: {} cooked, {} cached, {} unique texture(s) discovered",
      elapsed_ms,
      cooked_count,
      cached_count,
      tex_jobs.size());

    return tex_jobs;
  }

  std::string buildKtxCommand(
    std::filesystem::path const& src_path,
    std::filesystem::path const& dst_path,
    CookTextureType const type)
  {
    bool const is_hdr { src_path.extension().string() == ".hdr" };
    std::string oetf { "srgb" };
    std::string options { is_hdr ? "" : " --encode uastc --zstd 11 " };
    int quality { 2 };
    std::string ktx_format { is_hdr ? "R32G32B32_SFLOAT" : "R8G8B8A8_SRGB" };

    switch (type) {
      case CookTextureType::NORMAL:
        oetf = "linear";
        options.append(" --normal-mode ");
        ktx_format = is_hdr ? "R32G32_SFLOAT" : "R8G8_UNORM";
        break;
      case CookTextureType::MR:
        oetf = "linear";
        ktx_format = is_hdr ? "R32G32B32A32_SFLOAT" : "R8G8B8A8_UNORM";
        break;
      case CookTextureType::EMISSIVE:
        oetf = "srgb";
        quality = 3;
        ktx_format = is_hdr ? "R32G32B32_SFLOAT" : "R8G8B8A8_SRGB";
        break;
      case CookTextureType::AO:
        break;
      case CookTextureType::HEIGHT:
        oetf = "linear";
        quality = 3;
        ktx_format = is_hdr ? "R32_SFLOAT" : "R8_UNORM";
        break;
      case CookTextureType::DIFFUSE:
      default:
        break;
    }

    options.append(" --uastc-quality " + std::to_string(quality));
    options.append(" --assign-tf " + oetf);

    return "ktx create --generate-mipmap --format " + ktx_format
      + options + " \"" + src_path.generic_string() + "\" \"" + dst_path.generic_string() + "\"";
  }

  int cookTexturesParallel(
    std::vector<TextureCookJob> const& tex_jobs,
    CookerOptions const& opts)
  {
    using Clock = std::chrono::steady_clock;
    auto const t_start { Clock::now() };

    struct PendingKtx
    {
      std::filesystem::path src_path{};
      std::filesystem::path ktx_path{};
      CookTextureType type { CookTextureType::DIFFUSE };
    };

    std::vector<PendingKtx> pending{};
    pending.reserve(tex_jobs.size());

    std::size_t cached_count { 0u };
    std::size_t missing_count { 0u };
    std::error_code ec{};

    for (auto const& job : tex_jobs) {
      if (job.src_path.extension() == ".ktx2") {
        ++cached_count;
        continue;
      }

      auto ktx_path { job.src_path };
      ktx_path.replace_extension(".ktx2");

      bool const has_src { std::filesystem::exists(job.src_path, ec) };
      bool const has_ktx { std::filesystem::exists(ktx_path, ec) };

      if (!has_src) {
        if (has_ktx) {
          ++cached_count;
        } else {
          ++missing_count;
          Vyroda::Logger::warn("Source texture missing, skipping: {}", job.src_path.generic_string());
        }
        continue;
      }

      if (!opts.force && has_ktx) {
        auto const src_wt { std::filesystem::last_write_time(job.src_path, ec) };
        auto const ktx_wt { std::filesystem::last_write_time(ktx_path, ec) };
        if (!ec && ktx_wt >= src_wt) {
          ++cached_count;
          continue;
        }
      }

      pending.emplace_back(PendingKtx{
        .src_path = job.src_path,
        .ktx_path = std::move(ktx_path),
        .type = job.type
      });
    }

    std::atomic<std::size_t> next_idx { 0u };
    std::atomic<std::size_t> cooked_count { 0u };
    std::atomic<std::size_t> failed_count { 0u };
    std::atomic<std::size_t> finished_count { 0u };
    std::size_t const total_pending { pending.size() };

    auto cook_one = [&](PendingKtx const& item) {
      auto tmp_path { item.ktx_path };
      tmp_path.replace_filename(item.ktx_path.stem().string() + ".__tmp.ktx2");

      std::string const cmd { buildKtxCommand(item.src_path, tmp_path, item.type) };
      int const rc { std::system(cmd.c_str()) };
      std::error_code local_ec{};

      auto const progress { finished_count.fetch_add(1u, std::memory_order_relaxed) + 1u };
      if (rc == 0 && std::filesystem::exists(tmp_path, local_ec)) {
        std::filesystem::rename(tmp_path, item.ktx_path, local_ec);
        if (!local_ec) {
          cooked_count.fetch_add(1u, std::memory_order_relaxed);
          Vyroda::Logger::info("[{}/{}] Cooked {}", progress, total_pending, item.ktx_path.generic_string());
          return;
        }
      }

      std::filesystem::remove(tmp_path, local_ec);
      failed_count.fetch_add(1u, std::memory_order_relaxed);
      Vyroda::Logger::error("[{}/{}] Failed to cook {}", progress, total_pending, item.src_path.generic_string());
    };

    if (!pending.empty()) {
      unsigned int const worker_count {
        static_cast<unsigned int>(std::min<std::size_t>(opts.jobs, pending.size()))
      };
      if (worker_count > 1u) {
        std::vector<std::thread> workers{};
        workers.reserve(worker_count);
        for (unsigned int t { 0u }; t < worker_count; ++t) {
          workers.emplace_back([&]() {
            while (true) {
              std::size_t const idx { next_idx.fetch_add(1u, std::memory_order_relaxed) };
              if (idx >= pending.size()) {
                break;
              }
              cook_one(pending[idx]);
            }
          });
        }
        for (auto& w : workers) {
          w.join();
        }
      } else {
        for (auto const& item : pending) {
          cook_one(item);
        }
      }
    }

    auto const elapsed_ms {
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count()
    };
    auto const failed { failed_count.load(std::memory_order_relaxed) };
    Vyroda::Logger::info(
      "Pass 2 (.ktx2) done in {}ms: {} cooked, {} cached, {} missing, {} failed",
      elapsed_ms,
      cooked_count.load(std::memory_order_relaxed),
      cached_count,
      missing_count,
      failed);

    return (failed > 0u) ? 1 : 0;
  }

  std::filesystem::path resolveSourceImage(std::filesystem::path const& path)
  {
    std::error_code ec{};
    if (path.extension() != ".ktx2" && std::filesystem::exists(path, ec)) {
      return path;
    }
    static constexpr std::array<char const*, 5> kExts { ".hdr", ".png", ".jpg", ".jpeg", ".dds" };
    for (auto const* ext : kExts) {
      auto candidate { path };
      candidate.replace_extension(ext);
      if (std::filesystem::exists(candidate, ec)) {
        return candidate;
      }
    }
    return path;
  }

  void collectConfigTextures(
    CookerOptions const& opts,
    std::vector<TextureCookJob>& tex_jobs,
    std::vector<SkyboxCookJob>& skybox_jobs)
  {
    std::unordered_set<std::string> seen{};
    seen.reserve(tex_jobs.size() + 64u);
    for (auto const& job : tex_jobs) {
      seen.insert(job.src_path.generic_string());
    }

    auto add_tex = [&](std::string const& rel_path, CookTextureType const type) {
      if (rel_path.empty()) {
        return;
      }
      auto const full { (opts.root / rel_path).lexically_normal() };
      auto const resolved { resolveSourceImage(full) };
      if (seen.insert(resolved.generic_string()).second) {
        tex_jobs.emplace_back(TextureCookJob{ .src_path = resolved, .type = type });
      }
    };

    auto parse_texture_sections = [&](Vyroda::json const& doc) {
      static constexpr std::array<std::pair<char const*, CookTextureType>, 4> kSections {{
        { "textures", CookTextureType::DIFFUSE },
        { "terrain", CookTextureType::HEIGHT },
        { "normal", CookTextureType::NORMAL },
        { "water", CookTextureType::DIFFUSE }
      }};
      for (auto const& [section_name, type] : kSections) {
        if (!doc.contains(section_name) || !doc[section_name].is_object()) {
          continue;
        }
        for (auto const& [tex_name, entry] : doc[section_name].items()) {
          if (entry.is_object() && entry.contains("path")) {
            add_tex(entry["path"].get<std::string>(), type);
          }
        }
      }
    };

    // 1. config/textures.json (global textures + skybox cubemaps)
    auto const textures_json_path { opts.root / "config" / "textures.json" };
    std::error_code ec{};
    if (std::filesystem::exists(textures_json_path, ec)) {
      std::ifstream ifs(textures_json_path);
      if (ifs) {
        try {
          auto const doc = Vyroda::json::parse(ifs);
          parse_texture_sections(doc);

          if (doc.contains("skybox") && doc["skybox"].is_object()) {
            for (auto const& [skybox_name, face_list] : doc["skybox"].items()) {
              if (!face_list.is_array()) {
                continue;
              }
              SkyboxCookJob sb{};
              sb.name = skybox_name;
              sb.out_ktx = (opts.root / "assets" / "texture" / (skybox_name + ".ktx2")).lexically_normal();
              for (auto const& face_val : face_list) {
                auto const face_full { (opts.root / face_val.get<std::string>()).lexically_normal() };
                sb.faces.emplace_back(resolveSourceImage(face_full));
              }
              skybox_jobs.emplace_back(std::move(sb));
            }
          }
        } catch (std::exception const& e) {
          Vyroda::Logger::warn("Failed to parse {}: {}", textures_json_path.string(), e.what());
        }
      }
    }

    // 2. Level-specific terrain / water / normal / textures entries
    if (opts.mesh.empty()) {
      for (auto const& level_file : collectLevelFiles(opts)) {
        std::ifstream ifs(level_file);
        if (!ifs) {
          continue;
        }
        try {
          auto const doc = Vyroda::json::parse(ifs);
          parse_texture_sections(doc);
        } catch (std::exception const&) {
        }
      }
    }
  }

  int cookSkyboxes(
    std::vector<SkyboxCookJob> const& skybox_jobs,
    CookerOptions const& opts)
  {
    if (skybox_jobs.empty()) {
      return 0;
    }

    using Clock = std::chrono::steady_clock;
    auto const t_start { Clock::now() };
    std::size_t cooked { 0u };
    std::size_t cached { 0u };
    std::size_t missing { 0u };
    std::size_t failed { 0u };
    std::error_code ec{};

    for (auto const& sb : skybox_jobs) {
      bool all_faces_exist { !sb.faces.empty() };
      bool is_hdr { false };
      std::filesystem::file_time_type newest_face{};
      bool has_newest { false };

      for (auto const& face : sb.faces) {
        if (!std::filesystem::exists(face, ec)) {
          all_faces_exist = false;
          break;
        }
        if (face.extension() == ".hdr") {
          is_hdr = true;
        }
        auto const wt { std::filesystem::last_write_time(face, ec) };
        if (!ec && (!has_newest || wt > newest_face)) {
          newest_face = wt;
          has_newest = true;
        }
      }

      bool const has_ktx { std::filesystem::exists(sb.out_ktx, ec) };
      if (!all_faces_exist) {
        if (has_ktx) {
          ++cached;
        } else {
          ++missing;
          Vyroda::Logger::warn("Skybox '{}' source faces missing, skipping", sb.name);
        }
        continue;
      }

      if (!opts.force && has_ktx && has_newest) {
        auto const ktx_wt { std::filesystem::last_write_time(sb.out_ktx, ec) };
        if (!ec && ktx_wt >= newest_face) {
          ++cached;
          continue;
        }
      }

      std::string const ktx_format { is_hdr ? "R32G32B32A32_SFLOAT" : "R8G8B8A8_SRGB" };
      std::string files_arg{};
      for (auto const& face : sb.faces) {
        files_arg.append("\"" + face.generic_string() + "\" ");
      }

      auto tmp_path { sb.out_ktx };
      tmp_path.replace_filename(sb.out_ktx.stem().string() + ".__tmp.ktx2");
      std::string const cmd {
        "ktx create --generate-mipmap --format " + ktx_format
        + " --cubemap " + files_arg + "\"" + tmp_path.generic_string() + "\""
      };

      int const rc { std::system(cmd.c_str()) };
      if (rc == 0 && std::filesystem::exists(tmp_path, ec)) {
        std::filesystem::rename(tmp_path, sb.out_ktx, ec);
        if (!ec) {
          ++cooked;
          Vyroda::Logger::info("Cooked skybox {}", sb.out_ktx.generic_string());
          continue;
        }
      }

      std::filesystem::remove(tmp_path, ec);
      ++failed;
      Vyroda::Logger::error("Failed to cook skybox '{}'", sb.name);
    }

    auto const elapsed_ms {
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count()
    };
    Vyroda::Logger::info(
      "Pass 3 (skyboxes) done in {}ms: {} cooked, {} cached, {} missing, {} failed",
      elapsed_ms,
      cooked,
      cached,
      missing,
      failed);

    return (failed > 0u) ? 1 : 0;
  }
}

int main(int argc, char** argv)
{
  auto const opts_opt { parseArgs(argc, argv) };
  if (!opts_opt.has_value()) {
    return 0;
  }
  auto const& opts { *opts_opt };

  Vyroda::Logger::info(
    "vyroda-cooker starting (root={}, level={}, mesh={}, jobs={}, force={})",
    opts.root.string(),
    opts.level.empty() ? "ALL" : opts.level,
    opts.mesh.empty() ? "NONE" : opts.mesh,
    opts.jobs,
    opts.force);

  auto const mesh_jobs { discoverMeshes(opts) };
  Vyroda::Logger::info("Discovered {} unique mesh(es):", mesh_jobs.size());
  for (auto const& job : mesh_jobs) {
    Vyroda::Logger::info("  - {} (flipY={})", job.path.generic_string(), job.flip_y);
  }

  auto tex_jobs { cookMeshesAndCollectTextures(mesh_jobs, opts) };
  std::vector<SkyboxCookJob> skybox_jobs{};
  if (opts.mesh.empty()) {
    collectConfigTextures(opts, tex_jobs, skybox_jobs);
  }

  int const tex_rc { cookTexturesParallel(tex_jobs, opts) };
  int const sky_rc { cookSkyboxes(skybox_jobs, opts) };
  return (tex_rc != 0 || sky_rc != 0) ? 1 : 0;
}
