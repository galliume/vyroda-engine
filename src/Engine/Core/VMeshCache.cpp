module Engine.Core.VMeshCache;

import std;

import Engine.Core.AnimationTypes;
import Engine.Core.BoneTypes;
import Engine.Core.Constants;
import Engine.Core.GLM;
import Engine.Core.Logger;
import Engine.Core.MaterialTypes;
import Engine.Core.Vertex;

namespace Vyroda
{
  namespace
  {
    constexpr std::array<char, 4> kVMeshMagic { 'V', 'M', 'S', 'H' };
    constexpr std::uint32_t kVMeshVersion { 1u };

    struct VMeshHeader
    {
      std::array<char, 4> magic { kVMeshMagic };
      std::uint32_t version { kVMeshVersion };
      std::uint32_t vertex_size { static_cast<std::uint32_t>(sizeof(Vertex)) };
      std::uint32_t vertex_bones_size { static_cast<std::uint32_t>(sizeof(VertexBones)) };
      std::uint32_t flags { 0u };
      std::uint32_t reserved { 0u };
      std::int64_t src_mtime { 0 };
      std::uint64_t src_size { 0 };
    };

    struct SourceSignature
    {
      std::int64_t mtime { 0 };
      std::uint64_t size { 0 };
    };

    SourceSignature computeSourceSignature(std::filesystem::path const& src_path)
    {
      SourceSignature sig{};
      std::error_code ec{};
      if (std::filesystem::exists(src_path, ec)) {
        auto const wt { std::filesystem::last_write_time(src_path, ec) };
        if (!ec) {
          sig.mtime = static_cast<std::int64_t>(wt.time_since_epoch().count());
        }
        auto const sz { std::filesystem::file_size(src_path, ec) };
        if (!ec) {
          sig.size = static_cast<std::uint64_t>(sz);
        }
      }
      if (src_path.extension() == ".obj") {
        auto mtl_path { src_path };
        mtl_path.replace_extension(".mtl");
        if (std::filesystem::exists(mtl_path, ec)) {
          auto const mtl_wt { std::filesystem::last_write_time(mtl_path, ec) };
          if (!ec) {
            sig.mtime ^= (static_cast<std::int64_t>(mtl_wt.time_since_epoch().count()) * 0x9e3779b9LL);
          }
          auto const mtl_sz { std::filesystem::file_size(mtl_path, ec) };
          if (!ec) {
            sig.size ^= static_cast<std::uint64_t>(mtl_sz);
          }
        }
      }
      return sig;
    }

    template <typename T>
    void writePod(std::ostream& os, T const& value)
    {
      os.write(reinterpret_cast<char const*>(&value), static_cast<std::streamsize>(sizeof(T)));
    }

    template <typename T>
    bool readPod(std::istream& is, T& value)
    {
      return static_cast<bool>(
        is.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T))));
    }

    void writeString(std::ostream& os, std::string const& s)
    {
      auto const len { static_cast<std::uint32_t>(s.size()) };
      writePod(os, len);
      if (len > 0u) {
        os.write(s.data(), static_cast<std::streamsize>(len));
      }
    }

    bool readString(std::istream& is, std::string& s)
    {
      std::uint32_t len { 0u };
      if (!readPod(is, len)) {
        return false;
      }
      s.resize(len);
      if (len > 0u) {
        return static_cast<bool>(is.read(s.data(), static_cast<std::streamsize>(len)));
      }
      return true;
    }

    template <typename T>
    void writePodVector(std::ostream& os, std::vector<T> const& vec)
    {
      auto const count { static_cast<std::uint64_t>(vec.size()) };
      writePod(os, count);
      if (count > 0u) {
        os.write(
          reinterpret_cast<char const*>(vec.data()),
          static_cast<std::streamsize>(count * sizeof(T)));
      }
    }

    template <typename T>
    bool readPodVector(std::istream& is, std::vector<T>& vec)
    {
      std::uint64_t count { 0u };
      if (!readPod(is, count)) {
        return false;
      }
      vec.resize(static_cast<std::size_t>(count));
      if (count > 0u) {
        return static_cast<bool>(is.read(
          reinterpret_cast<char*>(vec.data()),
          static_cast<std::streamsize>(count * sizeof(T))));
      }
      return true;
    }

    std::string stripRootPrefix(std::string const& full_path, std::string const& root_path)
    {
      if (!root_path.empty() && full_path.starts_with(root_path)) {
        return full_path.substr(root_path.size());
      }
      return full_path;
    }

    std::string restoreRootPrefix(std::string const& rel_path, std::string const& root_path)
    {
      if (rel_path.empty()) {
        return {};
      }
      std::filesystem::path const p { rel_path };
      if (p.is_absolute()) {
        return rel_path;
      }
      return root_path + rel_path;
    }

    void writeTexSlot(
      std::ostream& os,
      std::string const& name,
      std::string const& path,
      TextureWrapMode const wrap_u,
      TextureWrapMode const wrap_v,
      std::string const& root_path)
    {
      writeString(os, name);
      writeString(os, stripRootPrefix(path, root_path));
      auto const u { static_cast<std::uint8_t>(wrap_u) };
      auto const v { static_cast<std::uint8_t>(wrap_v) };
      writePod(os, u);
      writePod(os, v);
    }

    bool readTexSlot(
      std::istream& is,
      std::string& name,
      std::string& path,
      TextureWrapMode& wrap_u,
      TextureWrapMode& wrap_v,
      std::string const& root_path)
    {
      std::string rel_path{};
      std::uint8_t u { 0u };
      std::uint8_t v { 0u };
      if (!readString(is, name) || !readString(is, rel_path) || !readPod(is, u) || !readPod(is, v)) {
        return false;
      }
      path = restoreRootPrefix(rel_path, root_path);
      wrap_u = static_cast<TextureWrapMode>(u);
      wrap_v = static_cast<TextureWrapMode>(v);
      return true;
    }

    void writeMaterial(std::ostream& os, material_t const& m, std::string const& root_path)
    {
      writeString(os, m.name);
      writePod(os, m.base_color);
      writePod(os, m.ambient);
      writePod(os, m.diffuse);
      writePod(os, m.specular);
      writePod(os, m.transmittance);
      writePod(os, m.mre_factor);
      writePod(os, m.shininess);
      writePod(os, m.ior);
      writePod(os, m.dissolve);
      writePod(os, m.illum);
      auto const ds { static_cast<std::uint8_t>(m.double_sided ? 1u : 0u) };
      writePod(os, ds);
      writePod(os, m.alpha_mode);
      writePod(os, m.alpha_cut_off);

      writeTexSlot(os, m.name_texture_ambient, m.name_texture_ambient_path, m.texture_ambient_wrap_mode_u, m.texture_ambient_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_diffuse, m.name_texture_diffuse_path, m.texture_diffuse_wrap_mode_u, m.texture_diffuse_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_specular, m.name_texture_specular_path, m.texture_specular_wrap_mode_u, m.texture_specular_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_specular_highlight, m.name_texture_specular_highlight_path, m.texture_specular_highlight_wrap_mode_u, m.texture_specular_highlight_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_bump, m.name_texture_bump_path, m.texture_bump_wrap_mode_u, m.texture_bump_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_alpha, m.name_texture_alpha_path, m.texture_alpha_wrap_mode_u, m.texture_alpha_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_metal_roughness, m.name_texture_metal_roughness_path, m.texture_metal_roughness_wrap_mode_u, m.texture_metal_roughness_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_emissive, m.name_texture_emissive_path, m.texture_emissive_wrap_mode_u, m.texture_emissive_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_ao, m.name_texture_ao_path, m.texture_ao_wrap_mode_u, m.texture_ao_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_base_color, m.name_texture_base_color_path, m.texture_base_color_wrap_mode_u, m.texture_base_color_wrap_mode_v, root_path);
      writeTexSlot(os, m.name_texture_transmission, m.name_texture_transmission_path, m.texture_transmission_wrap_mode_u, m.texture_transmission_wrap_mode_v, root_path);

      writePod(os, m.normal_translation);
      writePod(os, m.normal_scale);
      writePod(os, m.normal_rotation);
      writePod(os, m.ambient_translation);
      writePod(os, m.ambient_scale);
      writePod(os, m.ambient_rotation);
      writePod(os, m.diffuse_translation);
      writePod(os, m.diffuse_scale);
      writePod(os, m.diffuse_rotation);
      writePod(os, m.emissive_translation);
      writePod(os, m.emissive_scale);
      writePod(os, m.emissive_rotation);
      writePod(os, m.mr_translation);
      writePod(os, m.mr_scale);
      writePod(os, m.mr_rotation);
      writePod(os, m.transmission_translation);
      writePod(os, m.transmission_scale);
      writePod(os, m.transmission_rotation);

      writePod(os, m.normal_strength);
      writePod(os, m.occlusion_strength);
      writePod(os, m.transmission_strength);
      writePod(os, m.emissive_color);
    }

    bool readMaterial(std::istream& is, material_t& m, std::string const& root_path)
    {
      std::uint8_t ds { 0u };
      if (!readString(is, m.name)
          || !readPod(is, m.base_color)
          || !readPod(is, m.ambient)
          || !readPod(is, m.diffuse)
          || !readPod(is, m.specular)
          || !readPod(is, m.transmittance)
          || !readPod(is, m.mre_factor)
          || !readPod(is, m.shininess)
          || !readPod(is, m.ior)
          || !readPod(is, m.dissolve)
          || !readPod(is, m.illum)
          || !readPod(is, ds)
          || !readPod(is, m.alpha_mode)
          || !readPod(is, m.alpha_cut_off)) {
        return false;
      }
      m.double_sided = (ds != 0u);

      if (!readTexSlot(is, m.name_texture_ambient, m.name_texture_ambient_path, m.texture_ambient_wrap_mode_u, m.texture_ambient_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_diffuse, m.name_texture_diffuse_path, m.texture_diffuse_wrap_mode_u, m.texture_diffuse_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_specular, m.name_texture_specular_path, m.texture_specular_wrap_mode_u, m.texture_specular_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_specular_highlight, m.name_texture_specular_highlight_path, m.texture_specular_highlight_wrap_mode_u, m.texture_specular_highlight_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_bump, m.name_texture_bump_path, m.texture_bump_wrap_mode_u, m.texture_bump_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_alpha, m.name_texture_alpha_path, m.texture_alpha_wrap_mode_u, m.texture_alpha_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_metal_roughness, m.name_texture_metal_roughness_path, m.texture_metal_roughness_wrap_mode_u, m.texture_metal_roughness_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_emissive, m.name_texture_emissive_path, m.texture_emissive_wrap_mode_u, m.texture_emissive_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_ao, m.name_texture_ao_path, m.texture_ao_wrap_mode_u, m.texture_ao_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_base_color, m.name_texture_base_color_path, m.texture_base_color_wrap_mode_u, m.texture_base_color_wrap_mode_v, root_path)
          || !readTexSlot(is, m.name_texture_transmission, m.name_texture_transmission_path, m.texture_transmission_wrap_mode_u, m.texture_transmission_wrap_mode_v, root_path)) {
        return false;
      }

      return readPod(is, m.normal_translation)
          && readPod(is, m.normal_scale)
          && readPod(is, m.normal_rotation)
          && readPod(is, m.ambient_translation)
          && readPod(is, m.ambient_scale)
          && readPod(is, m.ambient_rotation)
          && readPod(is, m.diffuse_translation)
          && readPod(is, m.diffuse_scale)
          && readPod(is, m.diffuse_rotation)
          && readPod(is, m.emissive_translation)
          && readPod(is, m.emissive_scale)
          && readPod(is, m.emissive_rotation)
          && readPod(is, m.mr_translation)
          && readPod(is, m.mr_scale)
          && readPod(is, m.mr_rotation)
          && readPod(is, m.transmission_translation)
          && readPod(is, m.transmission_scale)
          && readPod(is, m.transmission_rotation)
          && readPod(is, m.normal_strength)
          && readPod(is, m.occlusion_strength)
          && readPod(is, m.transmission_strength)
          && readPod(is, m.emissive_color);
    }

    template <typename TKey>
    void writeKeyMap(
      std::ostream& os,
      std::unordered_map<std::string, std::vector<std::vector<TKey>>> const& map)
    {
      auto const count { static_cast<std::uint32_t>(map.size()) };
      writePod(os, count);
      for (auto const& [node_name, channels] : map) {
        writeString(os, node_name);
        auto const ch_count { static_cast<std::uint32_t>(channels.size()) };
        writePod(os, ch_count);
        for (auto const& keys : channels) {
          writePodVector(os, keys);
        }
      }
    }

    template <typename TKey>
    bool readKeyMap(
      std::istream& is,
      std::unordered_map<std::string, std::vector<std::vector<TKey>>>& map)
    {
      std::uint32_t count { 0u };
      if (!readPod(is, count)) {
        return false;
      }
      map.reserve(count);
      for (std::uint32_t i { 0u }; i < count; ++i) {
        std::string node_name{};
        std::uint32_t ch_count { 0u };
        if (!readString(is, node_name) || !readPod(is, ch_count)) {
          return false;
        }
        auto& channels { map[std::move(node_name)] };
        channels.resize(ch_count);
        for (std::uint32_t c { 0u }; c < ch_count; ++c) {
          if (!readPodVector(is, channels[c])) {
            return false;
          }
        }
      }
      return true;
    }

    void writeBonesMap(std::ostream& os, std::unordered_map<std::string, Bone> const& bones)
    {
      auto const count { static_cast<std::uint32_t>(bones.size()) };
      writePod(os, count);
      for (auto const& [key, b] : bones) {
        writeString(os, key);
        writePod(os, b.id);
        writeString(os, b.name);
        writeString(os, b.parent_name);
        writePod(os, b.transform);
        writePod(os, b.t_pose);
        writePod(os, b.t_pose_position);
        writePod(os, b.t_pose_scale);
        writePod(os, b.t_pose_rotation);
        writePod(os, b.offset_matrix);
        writePodVector(os, b.weights);
        auto const child_count { static_cast<std::uint32_t>(b.children.size()) };
        writePod(os, child_count);
        for (auto const& ch : b.children) {
          writeString(os, ch);
        }
        auto const wl { static_cast<std::uint8_t>(b.is_weightless ? 1u : 0u) };
        writePod(os, wl);
      }
    }

    bool readBonesMap(std::istream& is, std::unordered_map<std::string, Bone>& bones)
    {
      std::uint32_t count { 0u };
      if (!readPod(is, count)) {
        return false;
      }
      bones.reserve(count);
      for (std::uint32_t i { 0u }; i < count; ++i) {
        std::string key{};
        Bone b{};
        std::uint32_t child_count { 0u };
        std::uint8_t wl { 0u };
        if (!readString(is, key)
            || !readPod(is, b.id)
            || !readString(is, b.name)
            || !readString(is, b.parent_name)
            || !readPod(is, b.transform)
            || !readPod(is, b.t_pose)
            || !readPod(is, b.t_pose_position)
            || !readPod(is, b.t_pose_scale)
            || !readPod(is, b.t_pose_rotation)
            || !readPod(is, b.offset_matrix)
            || !readPodVector(is, b.weights)
            || !readPod(is, child_count)) {
          return false;
        }
        b.children.resize(child_count);
        for (std::uint32_t c { 0u }; c < child_count; ++c) {
          if (!readString(is, b.children[c])) {
            return false;
          }
        }
        if (!readPod(is, wl)) {
          return false;
        }
        b.is_weightless = (wl != 0u);
        bones.emplace(std::move(key), std::move(b));
      }
      return true;
    }
  }

  std::filesystem::path VMeshCache::cachePathFor(std::filesystem::path const& src_path)
  {
    std::string const stem { src_path.stem().string() };
    std::filesystem::path parent { src_path.parent_path() };
    std::string folder { parent.filename().string() };

    if (folder == "glTF" || folder == "gltf" || folder == "mesh" || folder.empty()) {
      std::string const grandparent { parent.parent_path().filename().string() };
      if (!grandparent.empty() && grandparent != "mesh" && grandparent != "assets") {
        folder = grandparent;
      } else {
        folder = stem;
      }
    }

    return std::filesystem::path("./cache") / folder / (stem + ".vmesh");
  }

  bool VMeshCache::tryLoad(
    std::filesystem::path const& src_path,
    std::string const& root_path,
    bool const flip_Y,
    VMeshAsset& out_asset)
  {
    auto const cache_path { cachePathFor(src_path) };
    std::error_code ec{};
    if (!std::filesystem::exists(cache_path, ec)) {
      return false;
    }

    std::ifstream is(cache_path, std::ios::binary);
    if (!is) {
      return false;
    }

    std::vector<char> io_buf(1024u * 1024u);
    is.rdbuf()->pubsetbuf(io_buf.data(), static_cast<std::streamsize>(io_buf.size()));

    VMeshHeader hdr{};
    if (!readPod(is, hdr)) {
      return false;
    }

    auto const expected_sig { computeSourceSignature(src_path) };
    std::uint32_t const expected_flags { flip_Y ? 1u : 0u };

    if (hdr.magic != kVMeshMagic
        || hdr.version != kVMeshVersion
        || hdr.vertex_size != sizeof(Vertex)
        || hdr.vertex_bones_size != sizeof(VertexBones)
        || hdr.flags != expected_flags
        || hdr.src_mtime != expected_sig.mtime
        || hdr.src_size != expected_sig.size) {
      return false;
    }

    out_asset = VMeshAsset{};

    // Materials
    std::uint32_t mat_count { 0u };
    if (!readPod(is, mat_count)) {
      return false;
    }
    out_asset.materials.resize(mat_count);
    for (std::uint32_t i { 0u }; i < mat_count; ++i) {
      if (!readMaterial(is, out_asset.materials[i], root_path)) {
        return false;
      }
    }

    // Animations & Skeleton
    std::uint32_t anim_count { 0u };
    if (!readPod(is, anim_count)) {
      return false;
    }
    out_asset.animations.resize(anim_count);
    for (std::uint32_t i { 0u }; i < anim_count; ++i) {
      auto& a { out_asset.animations[i] };
      if (!readPod(is, a.id) || !readString(is, a.name) || !readPod(is, a.duration)
          || !readPod(is, a.ticks_per_s) || !readPod(is, a.default_anim)) {
        return false;
      }
    }

    if (!readKeyMap(is, out_asset.positions)
        || !readKeyMap(is, out_asset.rotations)
        || !readKeyMap(is, out_asset.scales)
        || !readBonesMap(is, out_asset.bones_map)
        || !readString(is, out_asset.root_bone_name)) {
      return false;
    }

    // Unique geometries
    std::uint32_t geom_count { 0u };
    if (!readPod(is, geom_count)) {
      return false;
    }
    out_asset.geometries.resize(geom_count);
    for (std::uint32_t i { 0u }; i < geom_count; ++i) {
      auto& g { out_asset.geometries[i] };
      if (!readString(is, g.base_name)
          || !readPod(is, g.material_ID)
          || !readPod(is, g.bbox_min)
          || !readPod(is, g.bbox_max)
          || !readPodVector(is, g.positions)
          || !readPodVector(is, g.vertices)
          || !readPodVector(is, g.vertices_bones)
          || !readPodVector(is, g.indices)) {
        return false;
      }
    }

    // Instances
    std::uint32_t inst_count { 0u };
    if (!readPod(is, inst_count)) {
      return false;
    }
    out_asset.instances.resize(inst_count);
    for (std::uint32_t i { 0u }; i < inst_count; ++i) {
      auto& inst { out_asset.instances[i] };
      if (!readPod(is, inst.geometry_idx)
          || !readString(is, inst.name)
          || !readPod(is, inst.transform_matrix)
          || !readPod(is, inst.local_transform)
          || !readPod(is, inst.inverse_transform_matrix)) {
        return false;
      }
    }

    return true;
  }

  bool VMeshCache::save(
    std::filesystem::path const& src_path,
    std::string const& root_path,
    bool const flip_Y,
    VMeshAsset const& asset)
  {
    auto const cache_path { cachePathFor(src_path) };
    std::error_code dir_ec{};
    std::filesystem::create_directories(cache_path.parent_path(), dir_ec);
    auto tmp_path { cache_path };
    tmp_path += ".tmp";

    std::ofstream os(tmp_path, std::ios::binary | std::ios::trunc);
    if (!os) {
      Logger::warn("VMeshCache: could not open {} for writing", tmp_path.string());
      return false;
    }

    std::vector<char> io_buf(1024u * 1024u);
    os.rdbuf()->pubsetbuf(io_buf.data(), static_cast<std::streamsize>(io_buf.size()));

    auto const sig { computeSourceSignature(src_path) };
    VMeshHeader hdr{};
    hdr.flags = flip_Y ? 1u : 0u;
    hdr.src_mtime = sig.mtime;
    hdr.src_size = sig.size;
    writePod(os, hdr);

    // Materials
    auto const mat_count { static_cast<std::uint32_t>(asset.materials.size()) };
    writePod(os, mat_count);
    for (auto const& m : asset.materials) {
      writeMaterial(os, m, root_path);
    }

    // Animations & Skeleton
    auto const anim_count { static_cast<std::uint32_t>(asset.animations.size()) };
    writePod(os, anim_count);
    for (auto const& a : asset.animations) {
      writePod(os, a.id);
      writeString(os, a.name);
      writePod(os, a.duration);
      writePod(os, a.ticks_per_s);
      writePod(os, a.default_anim);
    }

    writeKeyMap(os, asset.positions);
    writeKeyMap(os, asset.rotations);
    writeKeyMap(os, asset.scales);
    writeBonesMap(os, asset.bones_map);
    writeString(os, asset.root_bone_name);

    // Unique geometries
    auto const geom_count { static_cast<std::uint32_t>(asset.geometries.size()) };
    writePod(os, geom_count);
    for (auto const& g : asset.geometries) {
      writeString(os, g.base_name);
      writePod(os, g.material_ID);
      writePod(os, g.bbox_min);
      writePod(os, g.bbox_max);
      writePodVector(os, g.positions);
      writePodVector(os, g.vertices);
      writePodVector(os, g.vertices_bones);
      writePodVector(os, g.indices);
    }

    // Instances
    auto const inst_count { static_cast<std::uint32_t>(asset.instances.size()) };
    writePod(os, inst_count);
    for (auto const& inst : asset.instances) {
      writePod(os, inst.geometry_idx);
      writeString(os, inst.name);
      writePod(os, inst.transform_matrix);
      writePod(os, inst.local_transform);
      writePod(os, inst.inverse_transform_matrix);
    }

    os.flush();
    bool const ok { static_cast<bool>(os) };
    os.close();

    std::error_code ec{};
    if (!ok) {
      std::filesystem::remove(tmp_path, ec);
      Logger::warn("VMeshCache: write failed for {}", tmp_path.string());
      return false;
    }

    std::filesystem::rename(tmp_path, cache_path, ec);
    if (ec) {
      std::filesystem::remove(tmp_path, ec);
      Logger::warn("VMeshCache: rename failed for {}", cache_path.string());
      return false;
    }

    return true;
  }
}
