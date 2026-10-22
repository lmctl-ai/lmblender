// SPDX-FileCopyrightText: 2025-2026 lmblender contributors
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

namespace obj_import {

[[noreturn]] inline void fail(const char *code, const std::string &message)
{
  throw dslvm::Error(code, message);
}

inline std::string trim(std::string value)
{
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

inline double number(std::istringstream &row, const char *what, double low, double high)
{
  double result;
  if (!(row >> result) || !std::isfinite(result) || result < low || result > high)
    fail("OBJ_MTL_INVALID", std::string(what) + " must be finite and in [" +
         std::to_string(low) + "," + std::to_string(high) + "]");
  return result;
}

inline std::array<float, 3> color(std::istringstream &row, const char *what, double high)
{
  return {float(number(row, what, 0.0, high)), float(number(row, what, 0.0, high)),
          float(number(row, what, 0.0, high))};
}

inline uint32_t index(const std::string &part, size_t count, const char *what)
{
  if (part.empty()) fail("OBJ_INVALID", std::string("missing ") + what + " index");
  size_t used = 0;
  long long value = 0;
  try { value = std::stoll(part, &used); }
  catch (...) { fail("OBJ_INVALID", std::string("invalid ") + what + " index"); }
  if (used != part.size() || value == 0 || count > uint64_t(LLONG_MAX))
    fail("OBJ_INVALID", std::string("invalid ") + what + " index");
  const long long resolved = value < 0 ? static_cast<long long>(count) + value : value - 1;
  if (resolved < 0 || uint64_t(resolved) >= count)
    fail("OBJ_INVALID", std::string(what) + " index is out of range");
  return uint32_t(resolved);
}

struct Corner {
  uint32_t vertex = 0;
  int32_t uv = -1;
  int32_t normal = -1;
};

inline Corner corner(const std::string &token, size_t vertices, size_t uvs, size_t normals)
{
  Corner result;
  const size_t first = token.find('/');
  result.vertex = index(token.substr(0, first), vertices, "vertex");
  if (first == std::string::npos) return result;
  const size_t second = token.find('/', first + 1);
  if (second == std::string::npos) {
    if (first + 1 < token.size()) result.uv = int32_t(index(token.substr(first + 1), uvs, "UV"));
    return result;
  }
  if (token.find('/', second + 1) != std::string::npos)
    fail("OBJ_INVALID", "face corner has too many index separators");
  if (second > first + 1) result.uv = int32_t(index(token.substr(first + 1, second - first - 1), uvs, "UV"));
  if (second + 1 < token.size()) result.normal = int32_t(index(token.substr(second + 1), normals, "normal"));
  return result;
}

struct Materials {
  Scene &scene;
  std::filesystem::path root;
  std::map<std::string, Material *> by_name;
  uint64_t texture_bytes = 0;
  uint64_t library_bytes = 0;
  size_t library_count = 0;

  std::filesystem::path file(const std::string &relative, const uint64_t limit, const char *kind)
  {
    try { return gltf_import::confined_uri(root, relative, limit, kind); }
    catch (const dslvm::Error &error) {
      fail(error.code == "GLTF_LIMIT" ? "OBJ_MTL_LIMIT" : "OBJ_MTL_INVALID",
           std::string(kind) + " must be a bounded regular relative file inside the OBJ directory");
    }
  }

  std::string image(const std::string &relative, const char *kind)
  {
    const auto path = file(relative, 64u * 1024u * 1024u, kind);
    std::error_code ec;
    const uint64_t size = std::filesystem::file_size(path, ec);
    if (ec || size > 256u * 1024u * 1024u - texture_bytes)
      fail("OBJ_MTL_LIMIT", "OBJ textures exceed the 256 MiB aggregate budget");
    texture_bytes += size;
    try { return checked_image_path(path.string(), kind, {".png", ".jpg", ".jpeg", ".exr"}, 64u * 1024u * 1024u); }
    catch (const dslvm::Error &error) { fail("OBJ_MTL_INVALID", std::string(kind) + ": " + error.what()); }
  }

  void load(const std::string &relative)
  {
    if (++library_count > 16) fail("OBJ_MTL_LIMIT", "OBJ references more than 16 MTL libraries");
    const auto path = file(relative, 4u * 1024u * 1024u, "MTL library");
    std::error_code ec;
    const uint64_t size = std::filesystem::file_size(path, ec);
    if (ec || size > 16u * 1024u * 1024u - library_bytes)
      fail("OBJ_MTL_LIMIT", "MTL libraries exceed the 16 MiB aggregate budget");
    library_bytes += size;
    std::ifstream input(path);
    if (!input) fail("OBJ_MTL_INVALID", "could not open MTL library");
    Material *current = nullptr;
    std::string line;
    while (std::getline(input, line)) {
      if (line.size() > 1'000'000) fail("OBJ_MTL_LIMIT", "MTL line exceeds 1 MiB");
      std::istringstream row(line);
      std::string key;
      if (!(row >> key) || key.front() == '#') continue;
      std::string value;
      std::getline(row, value);
      value = trim(value);
      if (key == "newmtl") {
        if (value.empty() || by_name.count(value)) fail("OBJ_MTL_INVALID", "MTL material name is empty or duplicated");
        if (scene.materials.size() >= 4096) fail("OBJ_MTL_LIMIT", "OBJ material limit exceeded (4096)");
        auto material = std::make_unique<Material>();
        current = material.get();
        scene.materials.push_back(std::move(material));
        by_name[value] = current;
        continue;
      }
      if (!current) fail("OBJ_MTL_INVALID", "MTL property appears before newmtl");
      std::istringstream fields(value);
      if (key == "Kd") current->color = color(fields, "Kd", 1.0);
      else if (key == "Ks") {
        const auto ks = color(fields, "Ks", 1.0);
        const float peak = std::max({ks[0], ks[1], ks[2]});
        current->specular_level = peak;
        if (peak > 0.0f) current->specular_tint = {ks[0] / peak, ks[1] / peak, ks[2] / peak};
      } else if (key == "Ns") {
        const double shininess = number(fields, "Ns", 0.0, 1000.0);
        current->roughness = float(std::sqrt(2.0 / (shininess + 2.0)));
      } else if (key == "d") current->alpha = float(number(fields, "d", 0.0, 1.0));
      else if (key == "Tr") current->alpha = 1.0f - float(number(fields, "Tr", 0.0, 1.0));
      else if (key == "Ke") {
        current->emission = color(fields, "Ke", 100.0);
        current->emission_strength = std::max({current->emission[0], current->emission[1], current->emission[2]}) > 0 ? 1.0f : 0.0f;
      } else if (key == "map_Kd") {
        current->image_path = image(value, "map_Kd");
        current->texture_kind = 3;
        current->image_factor_enabled = true;
      } else if (key == "map_d") current->alpha_map_path = image(value, "map_d");
      else if (key == "map_Bump" || key == "bump") {
        if (value.rfind("-bm ", 0) == 0) {
          std::istringstream option(value.substr(4));
          current->bump_map_strength = float(number(option, "bump multiplier", 0.0, 10.0));
          std::getline(option, value);
          value = trim(value);
        }
        current->bump_map_path = image(value, "map_Bump");
      } else if (key.rfind("map_", 0) == 0)
        fail("OBJ_MTL_UNSUPPORTED", "unsupported MTL texture directive: " + key);
    }
    if (!input.eof()) fail("OBJ_MTL_INVALID", "could not finish reading MTL library");
  }
};

inline std::unique_ptr<Mesh> read(Scene &scene, const std::string &path)
{
  if (path.empty() || path.front() == '-') fail("OBJ_INVALID", "OBJ input path must not be empty or begin with '-'");
  std::error_code ec;
  const auto absolute = std::filesystem::canonical(path, ec);
  if (ec || !std::filesystem::is_regular_file(absolute, ec) || ec)
    fail("OBJ_INVALID", "OBJ input must be a readable regular file");
  const auto size = std::filesystem::file_size(absolute, ec);
  if (ec || size == 0 || size > 256u * 1024u * 1024u)
    fail("OBJ_LIMIT", "OBJ input must be non-empty and at most 256 MiB");
  std::ifstream input(absolute);
  if (!input) fail("OBJ_INVALID", "could not open OBJ input");
  auto mesh = std::make_unique<Mesh>();
  Materials materials{scene, absolute.parent_path()};
  std::vector<std::array<float, 2>> source_uvs;
  std::vector<std::array<float, 3>> source_normals;
  std::vector<std::array<Corner, 3>> triangle_corners;
  std::map<std::string, uint16_t> slot_ids;
  uint16_t current_slot = 0;
  bool material_selected = false;
  bool any_uv = false, any_normal = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.size() > 1'000'000) fail("OBJ_LIMIT", "OBJ line exceeds 1 MiB");
    std::istringstream row(line);
    std::string key;
    if (!(row >> key) || key.front() == '#') continue;
    if (key == "v") {
      std::array<double, 3> p{};
      if (!(row >> p[0] >> p[1] >> p[2]) ||
          std::any_of(p.begin(), p.end(), [](double value) { return !std::isfinite(value) || std::abs(value) > 1'000'000.0; }))
        fail("OBJ_INVALID", "OBJ vertex has invalid coordinates");
      mesh->vertices.push_back(p);
      if (mesh->vertices.size() > 10'000'000) fail("OBJ_LIMIT", "OBJ vertex budget exceeded (10 million)");
    } else if (key == "vt") {
      std::array<float, 2> uv{};
      if (!(row >> uv[0] >> uv[1]) ||
          std::any_of(uv.begin(), uv.end(), [](float value) { return !std::isfinite(value) || std::abs(value) > 1'000'000.0f; }))
        fail("OBJ_INVALID", "invalid OBJ UV");
      source_uvs.push_back(uv);
      if (source_uvs.size() > 20'000'000) fail("OBJ_LIMIT", "OBJ UV budget exceeded");
    } else if (key == "vn") {
      std::array<float, 3> n{};
      if (!(row >> n[0] >> n[1] >> n[2]) || !std::isfinite(n[0]) || !std::isfinite(n[1]) || !std::isfinite(n[2]))
        fail("OBJ_INVALID", "invalid OBJ normal");
      const float length = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
      if (!std::isfinite(length) || length < 1e-10f) fail("OBJ_INVALID", "OBJ normal has zero or invalid length");
      for (float &component : n) component /= length;
      source_normals.push_back(n);
      if (source_normals.size() > 20'000'000) fail("OBJ_LIMIT", "OBJ normal budget exceeded");
    } else if (key == "mtllib") {
      std::string name;
      bool found = false;
      while (row >> name) {
        if (name.front() == '#') break;
        materials.load(name);
        found = true;
      }
      if (!found) fail("OBJ_MTL_INVALID", "mtllib requires at least one library name");
    } else if (key == "usemtl") {
      std::string name; std::getline(row, name); name = trim(name);
      const auto found = materials.by_name.find(name);
      if (found == materials.by_name.end()) fail("OBJ_MTL_INVALID", "usemtl names an undefined MTL material");
      const auto [slot, inserted] = slot_ids.emplace(name, uint16_t(mesh->material_slots.size()));
      if (inserted) mesh->material_slots.push_back(found->second);
      current_slot = slot->second;
      material_selected = true;
    } else if (key == "f") {
      if (!material_selected && mesh->material_slots.empty()) mesh->material_slots.push_back(nullptr);
      std::vector<Corner> face;
      std::string token;
      while (row >> token) face.push_back(corner(token, mesh->vertices.size(), source_uvs.size(), source_normals.size()));
      if (face.size() < 3) fail("OBJ_INVALID", "OBJ face has fewer than three corners");
      if (mesh->triangles.size() + face.size() - 2 > 20'000'000) fail("OBJ_LIMIT", "OBJ triangle budget exceeded");
      for (size_t i = 1; i + 1 < face.size(); ++i) {
        const std::array<Corner, 3> corners{face[0], face[i], face[i + 1]};
        mesh->triangles.push_back({corners[0].vertex, corners[1].vertex, corners[2].vertex});
        triangle_corners.push_back(corners);
        mesh->triangle_materials.push_back(material_selected ? current_slot : 0);
        for (const Corner &corner : corners) { any_uv |= corner.uv >= 0; any_normal |= corner.normal >= 0; }
        const Material *active = material_selected ? mesh->material_slots[current_slot] : nullptr;
        if (active && (active->texture_kind == 3 || !active->alpha_map_path.empty() || !active->bump_map_path.empty()))
          for (const Corner &corner : corners) if (corner.uv < 0) fail("OBJ_MTL_INVALID", "textured OBJ face is missing a UV index");
      }
    }
  }
  if (!input.eof()) fail("OBJ_INVALID", "could not finish reading OBJ input");
  if (mesh->vertices.empty() || mesh->triangles.empty()) fail("OBJ_INVALID", "OBJ contains no triangle mesh");
  if (mesh->material_slots.empty()) mesh->triangle_materials.clear();
  if (any_uv) {
    mesh->uvs.reserve(mesh->triangles.size() * 3);
    for (const auto &tri : triangle_corners) for (const Corner &corner : tri)
      mesh->uvs.push_back(corner.uv >= 0 ? source_uvs[size_t(corner.uv)] : std::array<float, 2>{0.0f,0.0f});
  }
  if (any_normal) {
    mesh->corner_normals.reserve(mesh->triangles.size() * 3);
    for (const auto &tri : triangle_corners) {
      const auto &a = mesh->vertices[tri[0].vertex], &b = mesh->vertices[tri[1].vertex], &c = mesh->vertices[tri[2].vertex];
      const std::array<double,3> cross{(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]),
                                      (b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]),
                                      (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])};
      const double length = std::sqrt(cross[0]*cross[0] + cross[1]*cross[1] + cross[2]*cross[2]);
      const std::array<float,3> flat = length > 1e-20 ?
          std::array<float,3>{float(cross[0]/length),float(cross[1]/length),float(cross[2]/length)} :
          std::array<float,3>{0.0f,0.0f,1.0f};
      for (const Corner &corner : tri)
        mesh->corner_normals.push_back(corner.normal >= 0 ? source_normals[size_t(corner.normal)] : flat);
    }
  }
  return mesh;
}
} // namespace obj_import
