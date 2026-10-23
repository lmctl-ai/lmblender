// SPDX-FileCopyrightText: 2025-2026 lmblender contributors
// SPDX-License-Identifier: MIT

#include "dslvm.hpp"
#include "schema_embedded.hpp"
#include "presets_embedded.hpp"
#include "view_luts.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <filesystem>
#include <memory>
#include <map>
#include <tuple>
#include <limits>
#include <cmath>
#include <cctype>
#include <chrono>
#include <set>
#include "platform.hpp"
#include "atomic_ops.h"
#include <nlohmann/json.hpp>

#include "device/device.h"
#include "device/denoise.h"
#include "util/version.h"
#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/config.h>
#endif
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/pass.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/attribute.h"
#include "session/buffers.h"
#include "session/session.h"
#include "app/oiio_output_driver.h"
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include "app/cycles_xml.h"
#include "util/log.h"
#include "util/openimagedenoise.h"
#include "util/path.h"
#include "util/system.h"
#include "util/transform.h"

namespace {
struct Mesh {
  std::vector<std::array<double, 3>> vertices;
  std::vector<std::array<uint32_t, 3>> triangles;
  std::vector<std::array<float, 2>> uvs;
  std::vector<std::array<float, 3>> corner_normals;
  std::vector<struct Material *> material_slots;
  std::vector<uint16_t> triangle_materials;
  struct Material *material = nullptr;
  Mesh *geometry_source = nullptr;
  std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
  std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
  bool ground_plane = false;
  bool smooth = false;
  bool visible = true;
  bool preset_generated = false;
  struct TransformKey { int frame; std::array<float,3> value; std::string interpolation; };
  std::vector<TransformKey> transform_keys;
  struct VisibilityKey { int frame; bool value; };
  std::vector<VisibilityKey> visibility_keys;
};
struct Material {
  std::array<float, 3> color{0.8f, 0.8f, 0.8f};
  std::array<float, 3> emission{0.0f, 0.0f, 0.0f};
  float metallic = 0.0f;
  float roughness = 0.5f;
  float alpha = 1.0f;
  float specular_level = 0.5f;
  std::array<float, 3> specular_tint{1.0f, 1.0f, 1.0f};
  float transmission = 0.0f;
  float subsurface = 0.0f;
  std::array<float, 3> subsurface_radius{1.0f, 0.2f, 0.1f};
  float subsurface_scale = 0.1f;
  int volume_kind = 0; /* 1 absorption, 2 scatter */
  bool volume_only = false;
  float volume_density = 0.0f;
  std::array<float, 3> volume_color{1.0f, 1.0f, 1.0f};
  float emission_strength = 0.0f;
  bool preset_generated = false;
  int texture_kind = 0;
  std::array<float, 3> texture_color2{1.0f, 1.0f, 1.0f};
  float texture_scale = 5.0f;
  std::string image_path;
  std::string normal_map_path, roughness_map_path, metallic_map_path, ao_map_path;
  std::string bump_map_path, alpha_map_path;
  float bump_map_strength = 1.0f;
  bool image_factor_enabled = false;
  bool backface_culling = false;
  Material *mix_material = nullptr;
  float mix_factor = 0.5f;
  std::string mix_factor_image;
  bool bump_enabled = false;
  float bump_strength = 0.35f, bump_distance = 0.12f, bump_scale = 18.0f;
  struct ColorKey { int frame; std::array<float,3> value; std::string interpolation; };
  std::vector<ColorKey> color_keys;
  struct StrengthKey { int frame; float value; std::string interpolation; };
  std::vector<StrengthKey> emission_keys;
};
struct PointLightData {
  std::array<float, 3> position{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 100.0f;
  struct StrengthKey { int frame; float value; std::string interpolation; };
  std::vector<StrengthKey> strength_keys;
};
struct AreaLightData {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 100.0f;
  float size = 1.0f;
  float size_y = 1.0f;
  int shape = 0; /* 0 square/rectangle, 1 disk, 2 ellipse */
  bool preset_generated = false;
};
struct SunLightData {
  std::array<float, 3> direction{0.0f, 0.0f, -1.0f};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 1.0f;
  float angle = 0.05f;
};
struct SpotLightData {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 100.0f;
  float angle = 0.785398f;
  float blend = 0.15f;
};
struct CameraConfig {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  float fov_radians = 0.785398163f;
  bool fit_to_geometry = false;
  bool orthographic = false;
  float ortho_scale = 10.0f;
};
struct Scene {
  std::vector<std::unique_ptr<Mesh>> meshes;
  std::vector<std::unique_ptr<Material>> materials;
  std::vector<PointLightData> point_lights;
  std::vector<AreaLightData> area_lights;
  std::vector<SunLightData> sun_lights;
  std::vector<SpotLightData> spot_lights;
  size_t preset_area_light_count = 0;
  nlohmann::json active_preset = nlohmann::json::object();
  nlohmann::json palette = nlohmann::json::object();
  std::array<float, 3> world_color{0.16f, 0.18f, 0.22f};
  float world_strength = 0.45f;
  bool sky_enabled = false;
  float sky_elevation = 0.7f;
  float sky_rotation = 0.0f;
  float sky_intensity = 1.0f;
  std::string environment_image;
  float environment_rotation = 0.0f;
  float environment_strength = 1.0f;
  bool transparent_background = false;
  float camera_aperture = 0.0f;
  float camera_focus_distance = 10.0f;
  bool custom_camera = false;
  CameraConfig camera;
  int render_timeout_seconds = 3600;
  int render_threads = 4;
  int max_bounces = 12;
  float clamp_direct = 0.0f;
  float clamp_indirect = 0.0f;
  int render_seed = 0;
  bool denoise = false;
  ccl::DenoiserQuality denoise_quality = ccl::DENOISER_QUALITY_HIGH;
  ccl::DenoiserPrefilter denoise_prefilter = ccl::DENOISER_PREFILTER_FAST;
  int preset_samples = 64;
  int preset_bounces = 8;
  bool motion_blur = false;
  float motion_shutter = 1.0f;
  std::string view_transform = "Standard";
  struct CameraKey { int frame; std::array<float,6> value; std::string interpolation; };
  std::vector<CameraKey> camera_keys;
  struct CameraOpticsKey { int frame; std::array<float,2> value; std::string interpolation; };
  std::vector<CameraOpticsKey> camera_optics_keys;
  int render_frame = 0;
  /* Scene-wide totals of owned (non-instance) mesh data, enforced at creation time. */
  size_t geometry_vertices = 0;
  size_t geometry_triangles = 0;
};

/* Every mesh builder and importer already caps one mesh; these cap the whole scene so a
 * program cannot accumulate unbounded geometry before (or without) rendering. The limits
 * match the per-render Cycles budget checked again in render_direct. */
constexpr size_t kSceneVertexBudget = 10'000'000;
constexpr size_t kSceneTriangleBudget = 20'000'000;

void reserve_geometry(Scene &scene, const size_t vertices, const size_t triangles)
{
  if (vertices > kSceneVertexBudget - scene.geometry_vertices || triangles > kSceneTriangleBudget - scene.geometry_triangles)
    throw dslvm::Error("GEOMETRY_BUDGET", "scene geometry budget exceeded: adding " + std::to_string(vertices) + " vertices and " +
                       std::to_string(triangles) + " triangles to " + std::to_string(scene.geometry_vertices) + "/" +
                       std::to_string(scene.geometry_triangles) + " would pass the " + std::to_string(kSceneVertexBudget) + " vertex / " +
                       std::to_string(kSceneTriangleBudget) + " triangle scene limit; use instance_mesh for copies or fewer/simpler meshes");
  scene.geometry_vertices += vertices;
  scene.geometry_triangles += triangles;
}

void recount_geometry(Scene &scene)
{
  scene.geometry_vertices = scene.geometry_triangles = 0;
  for (const auto &mesh : scene.meshes) if (!mesh->geometry_source) {
    scene.geometry_vertices += mesh->vertices.size();
    scene.geometry_triangles += mesh->triangles.size();
  }
}

/* The one place a mesh joins the scene: charges its geometry to the scene budget first. */
Mesh *adopt_mesh(Scene &scene, std::unique_ptr<Mesh> mesh)
{
  if (!mesh->geometry_source) reserve_geometry(scene, mesh->vertices.size(), mesh->triangles.size());
  Mesh *result = mesh.get();
  scene.meshes.push_back(std::move(mesh));
  return result;
}


/* Every image an LLM program names goes through here: extension, regular file, byte
 * cap, and the decoded size read from the header, so a few-kilobyte PNG cannot make
 * Cycles allocate gigapixels at render time. Returns the absolute path. */
constexpr uint64_t kImagePixelBudget = 64'000'000;
std::string checked_image_path(const std::string &path, const char *what,
                               std::initializer_list<const char *> extensions, const uintmax_t max_bytes)
{
  if (path.empty() || path.front() == '-') throw dslvm::Error("IMAGE_INVALID", std::string(what) + " path must not be empty or begin with '-'");
  const std::string ext = std::filesystem::path(path).extension().string();
  std::string allowed;
  bool ok = false;
  for (const char *candidate : extensions) { ok = ok || ext == candidate; allowed += (allowed.empty() ? "" : "/") + std::string(candidate); }
  if (!ok) throw dslvm::Error("IMAGE_INVALID", std::string(what) + " must be a " + allowed + " file");
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) throw dslvm::Error("IMAGE_INVALID", std::string(what) + " must be a readable regular file");
  const uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size == 0 || size > max_bytes)
    throw dslvm::Error("IMAGE_LIMIT", std::string(what) + " must be non-empty and at most " + std::to_string(max_bytes / (1024 * 1024)) + " MiB");
  std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path);
  if (!input) throw dslvm::Error("IMAGE_INVALID", std::string(what) + " could not be decoded by OpenImageIO: " + OIIO::geterror());
  const OIIO::ImageSpec spec = input->spec();
  input->close();
  if (spec.width < 1 || spec.height < 1 || spec.nchannels < 1 || uint64_t(spec.width) * uint64_t(spec.height) > kImagePixelBudget)
    throw dslvm::Error("IMAGE_LIMIT", std::string(what) + " decodes to " + std::to_string(spec.width) + "x" + std::to_string(spec.height) +
                       " pixels; the limit is " + std::to_string(kImagePixelBudget / 1'000'000) + " megapixels");
  const auto full = std::filesystem::absolute(path, ec);
  if (ec) throw dslvm::Error("IMAGE_INVALID", std::string("could not resolve ") + what + " path");
  return full.string();
}

/* Whole-program budget. The DSL VM imposes no limits of its own, so the host owns a
 * wall-clock deadline and an instruction ceiling for untrusted bytecode. Renders in
 * progress cancel through run_session_with_timeout; a pure VM loop is stopped by the
 * watchdog thread in run_with_budget. */
struct RunBudget {
  std::chrono::steady_clock::time_point deadline;
  bool has_deadline = false;
  uint64_t max_instructions = 0;
  std::atomic<bool> expired{false};
};
RunBudget &run_budget() { static RunBudget budget; return budget; }

long long env_integer(const char *name, const long long fallback, const long long lo, const long long hi)
{
  const char *text = std::getenv(name);
  if (!text || !*text) return fallback;
  char *end = nullptr;
  const long long value = std::strtoll(text, &end, 10);
  if (end == text || *end != '\0' || value < lo || value > hi)
    throw dslvm::Error("RUN_BUDGET", std::string(name) + " must be an integer in [" + std::to_string(lo) + "," + std::to_string(hi) + "]");
  return value;
}

/* Executes the program under the whole-program budget. Defaults: 7200 s of wall clock
 * and 100 million VM instructions; LMBLENDER_RUN_TIMEOUT_SECONDS and
 * LMBLENDER_MAX_INSTRUCTIONS override them and 0 disables either. When the deadline
 * passes, a running render is cancelled (RUN_TIMEOUT through the normal error path);
 * a program still executing five seconds later, such as a pure VM loop, is terminated
 * after printing the standard JSON error line. */
void run_with_budget(const dslvm::Program &program, const dslvm::Context &context, const dslvm::Registry &registry)
{
  RunBudget &budget = run_budget();
  const long long timeout_seconds = env_integer("LMBLENDER_RUN_TIMEOUT_SECONDS", 7200, 0, 7 * 86400);
  budget.max_instructions = uint64_t(env_integer("LMBLENDER_MAX_INSTRUCTIONS", 100'000'000, 0, 1'000'000'000'000LL));
  budget.has_deadline = timeout_seconds > 0;
  budget.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  dslvm::Program::RunStats stats;
  std::atomic<bool> finished{false};
  std::thread watchdog([&] {
    constexpr auto grace = std::chrono::seconds(5);
    while (!finished.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      /* The VM increments this counter without atomics; an atomic load of the aligned
       * 64-bit word is a best-effort monitor. The wall clock is the authoritative guard. */
      const uint64_t executed = atomic_load_uint64(&stats.instructions);
      if (budget.max_instructions && executed > budget.max_instructions) {
        budget.expired.store(true, std::memory_order_release);
        dslvm::report_error("run", dslvm::Error("RUN_BUDGET", "program exceeded LMBLENDER_MAX_INSTRUCTIONS=" +
                                                 std::to_string(budget.max_instructions) + " VM instructions; simplify its loops or raise the limit"));
        std::_Exit(3);
      }
      const auto now = std::chrono::steady_clock::now();
      if (budget.has_deadline && now >= budget.deadline) {
        budget.expired.store(true, std::memory_order_release);
        if (now >= budget.deadline + grace) {
          dslvm::report_error("run", dslvm::Error("RUN_TIMEOUT", "program exceeded LMBLENDER_RUN_TIMEOUT_SECONDS=" +
                                                   std::to_string(timeout_seconds) + " seconds of wall-clock time"));
          std::_Exit(3);
        }
      }
    }
  });
  try {
    program.run(context, registry, &stats);
  } catch (...) {
    finished.store(true, std::memory_order_release);
    watchdog.join();
    throw;
  }
  finished.store(true, std::memory_order_release);
  watchdog.join();
}

/* Output policy: the parent directory must exist and must not be a symlink, and when
 * LMBLENDER_OUTPUT_ROOT is set every output must resolve inside that directory. */
void check_output_path(const std::string &output, const char *what)
{
  if (output.empty() || output.front() == '-') throw dslvm::Error("OUTPUT_PATH", std::string(what) + " path must not be empty or begin with '-'");
  std::error_code ec;
  const std::filesystem::path path(output);
  std::filesystem::path parent = path.parent_path();
  if (parent.empty()) parent = ".";
  if (!std::filesystem::is_directory(parent, ec) || ec) throw dslvm::Error("OUTPUT_PATH", std::string(what) + " directory does not exist: " + parent.string());
  if (std::filesystem::is_symlink(parent, ec)) throw dslvm::Error("OUTPUT_PATH", std::string(what) + " directory must not be a symlink: " + parent.string());
  if (std::filesystem::is_symlink(path, ec)) throw dslvm::Error("OUTPUT_PATH", std::string(what) + " must not replace an existing symlink: " + output);
  const char *root_text = std::getenv("LMBLENDER_OUTPUT_ROOT");
  if (root_text && *root_text) {
    const auto root = std::filesystem::canonical(root_text, ec);
    if (ec || !std::filesystem::is_directory(root, ec)) throw dslvm::Error("OUTPUT_PATH", std::string("LMBLENDER_OUTPUT_ROOT is not an existing directory: ") + root_text);
    const auto resolved = std::filesystem::canonical(parent, ec);
    if (ec) throw dslvm::Error("OUTPUT_PATH", std::string("could not resolve the ") + what + " directory: " + parent.string());
    auto r = root.begin();
    auto f = resolved.begin();
    for (; r != root.end() && f != resolved.end() && *r == *f; ++r, ++f) {}
    if (r != root.end()) throw dslvm::Error("OUTPUT_PATH", std::string(what) + " must stay inside LMBLENDER_OUTPUT_ROOT (" + root.string() + "): " + output);
  }
}

/* Staged files use predictable names; drop whatever sits there (including a planted
 * symlink) before the image writer opens the path. */
void clear_temporary(const std::string &path)
{
  std::error_code ec;
  if (std::filesystem::is_symlink(path, ec) || std::filesystem::exists(path, ec)) std::filesystem::remove(path, ec);
}

using PresetJson = nlohmann::json;

const PresetJson &builtin_presets()
{
  static const PresetJson data = PresetJson::parse(
      reinterpret_cast<const char *>(lmblender::embedded_presets),
      reinterpret_cast<const char *>(lmblender::embedded_presets) + lmblender::embedded_presets_size);
  return data;
}

PresetJson find_builtin_preset(const std::string &name)
{
  for (const auto &preset : builtin_presets().at("presets"))
    if (preset.value("name", std::string()) == name) return preset;
  throw dslvm::Error("PRESET_NOT_FOUND", "unknown preset '" + name + "'; list choices with lmblender --presets");
}

void remove_preset_area_rig(Scene &scene)
{
  scene.area_lights.erase(std::remove_if(scene.area_lights.begin(), scene.area_lights.end(),
      [](const AreaLightData &light) { return light.preset_generated; }), scene.area_lights.end());
  scene.preset_area_light_count = 0;
}

void apply_preset(Scene &scene, const PresetJson &preset)
{
  try {
    if (!preset.is_object() || !preset.contains("name") || !preset.contains("render") ||
        !preset.contains("world") || !preset.contains("camera") || !preset.contains("ground") ||
        !preset.contains("lights") || !preset.contains("palette"))
      throw dslvm::Error("PRESET_INVALID", "preset needs name, render, world, camera, ground, lights, and palette fields");
    const std::string tier = preset.at("render").at("tier").get<std::string>();
    if (tier != "draft" && tier != "final") throw dslvm::Error("PRESET_INVALID", "render.tier must be draft or final");
    scene.preset_samples = preset.at("render").value("samples", tier == "draft" ? 8 : 64);
    scene.preset_bounces = preset.at("render").value("bounces", tier == "draft" ? 2 : 8);
    scene.max_bounces = scene.preset_bounces;
    scene.denoise = preset.at("render").value("denoise", tier == "final");
    const std::string denoise_quality = preset.at("render").value("denoise_quality", std::string("high"));
    if (denoise_quality == "fast") scene.denoise_quality = ccl::DENOISER_QUALITY_FAST;
    else if (denoise_quality == "balanced") scene.denoise_quality = ccl::DENOISER_QUALITY_BALANCED;
    else if (denoise_quality == "high") scene.denoise_quality = ccl::DENOISER_QUALITY_HIGH;
    else throw dslvm::Error("PRESET_INVALID", "render.denoise_quality must be fast, balanced, or high");
    if (scene.preset_samples < 1 || scene.preset_samples > 65536 || scene.preset_bounces < 0 || scene.preset_bounces > 64)
      throw dslvm::Error("PRESET_INVALID", "render.samples must be 1..65536 and render.bounces 0..64");
    if (scene.denoise && !ccl::openimagedenoise_supported())
      throw dslvm::Error("PRESET_UNSUPPORTED", "this preset needs CPU OpenImageDenoise, unavailable in this build");
    scene.render_threads = preset.at("render").value("threads", 4);
    if (scene.render_threads < 1 || scene.render_threads > 64) throw dslvm::Error("PRESET_INVALID", "render.threads must be in [1,64]");
    scene.view_transform = preset.at("render").value("view_transform", std::string("Standard"));
    if (scene.view_transform != "Standard" && scene.view_transform != "Filmic" && scene.view_transform != "AgX")
      throw dslvm::Error("PRESET_INVALID", "render.view_transform must be Standard, Filmic, or AgX");
    const auto &world = preset.at("world");
    const auto &world_rgb = world.at("color");
    if (!world_rgb.is_array() || world_rgb.size() != 3) throw dslvm::Error("PRESET_INVALID", "world.color must contain three channels");
    for (size_t i = 0; i < 3; ++i) {
      const float channel = world_rgb[i].get<float>();
      if (!std::isfinite(channel) || channel < 0 || channel > 1) throw dslvm::Error("PRESET_INVALID", "world.color channels must be in [0,1]");
      scene.world_color[i] = channel;
    }
    scene.world_strength = world.value("strength", 0.5f);
    if (!std::isfinite(scene.world_strength) || scene.world_strength < 0 || scene.world_strength > 100)
      throw dslvm::Error("PRESET_INVALID", "world.strength must be in [0,100]");

    const auto &camera = preset.at("camera");
    const float fov = camera.value("fov_degrees", 42.0f);
    const float vertical_span = camera.value("vertical_span", 8.0f);
    if (!std::isfinite(fov) || fov < 5 || fov > 170 || !std::isfinite(vertical_span) || vertical_span <= 0)
      throw dslvm::Error("PRESET_INVALID", "camera FOV must be 5..170 degrees and vertical span positive");
    scene.camera.fov_radians = fov * float(M_PI / 180.0);
    scene.camera.ortho_scale = vertical_span;
    scene.camera.orthographic = camera.value("orthographic", false);
    scene.camera.fit_to_geometry = camera.value("auto_frame", true);
    scene.camera.target = {0, 0, 0};
    scene.custom_camera = true;

    /* Replace only lights and ground created by the previous preset. */
    scene.area_lights.erase(std::remove_if(scene.area_lights.begin(), scene.area_lights.end(), [](const auto &light) { return light.preset_generated; }), scene.area_lights.end());
    scene.preset_area_light_count = 0;
    scene.meshes.erase(std::remove_if(scene.meshes.begin(), scene.meshes.end(), [](const auto &mesh) { return mesh->preset_generated; }), scene.meshes.end());
    recount_geometry(scene);

    std::array<float,3> center{0,0,0};
    std::array<double,3> lo{INFINITY,INFINITY,INFINITY}, hi{-INFINITY,-INFINITY,-INFINITY};
    for (const auto &mesh : scene.meshes) if (!mesh->ground_plane) {
      const Mesh *data = mesh->geometry_source ? mesh->geometry_source : mesh.get();
      for (const auto &v : data->vertices) for (size_t axis = 0; axis < 3; ++axis) {
        const double p = v[axis] * mesh->scale[axis] + mesh->translation[axis];
        lo[axis] = std::min(lo[axis], p); hi[axis] = std::max(hi[axis], p);
      }
    }
    if (std::isfinite(lo[0])) for (size_t axis = 0; axis < 3; ++axis) center[axis] = float((lo[axis] + hi[axis]) * 0.5);
    const float radius = std::isfinite(lo[0]) ? std::max(0.5f, float(std::sqrt((hi[0]-lo[0])*(hi[0]-lo[0]) + (hi[1]-lo[1])*(hi[1]-lo[1]) + (hi[2]-lo[2])*(hi[2]-lo[2])) * 0.5)) : 1.0f;
    scene.camera.target = center;
    scene.camera.ortho_scale = vertical_span;
    if (!scene.camera.fit_to_geometry) {
      /* Without auto-framing the preset must place the camera itself, in units of the
       * geometry radius relative to its center, like the light positions. */
      if (!camera.contains("position"))
        throw dslvm::Error("PRESET_INVALID", "camera.auto_frame=false requires camera.position [x,y,z] (scene-radius units relative to the geometry center); optional camera.target uses the same units");
      const auto pos = camera.at("position").get<std::array<float, 3>>();
      const auto look = camera.value("target", std::array<float, 3>{0, 0, 0});
      for (size_t i = 0; i < 3; ++i) {
        scene.camera.position[i] = center[i] + pos[i] * radius;
        scene.camera.target[i] = center[i] + look[i] * radius;
        if (!std::isfinite(scene.camera.position[i]) || !std::isfinite(scene.camera.target[i]))
          throw dslvm::Error("PRESET_INVALID", "camera.position and camera.target must be finite");
      }
      const float dx = pos[0] - look[0], dy = pos[1] - look[1], dz = pos[2] - look[2];
      if (dx * dx + dy * dy + dz * dz < 1.0e-12f) throw dslvm::Error("PRESET_INVALID", "camera.position and camera.target must differ");
    }
    const auto &ground = preset.at("ground");
    if (ground.value("enabled", false)) {
      auto material = std::make_unique<Material>();
      const auto color = ground.value("color", std::vector<float>{0.32f,0.34f,0.37f});
      if (color.size() != 3) throw dslvm::Error("PRESET_INVALID", "ground.color must contain three channels");
      for (size_t i=0;i<3;++i) material->color[i]=color[i];
      material->roughness = 0.82f; material->preset_generated = true;
      Material *mat = material.get(); scene.materials.push_back(std::move(material));
      const float z = std::isfinite(lo[2]) ? float(lo[2]) - ground.value("offset", 0.01f) * radius : -radius;
      const float side = std::max(10.0f, radius * ground.value("size_scale", 10.0f));
      auto mesh = std::make_unique<Mesh>();
      mesh->vertices = {{{center[0]-side,center[1]-side,z},{center[0]+side,center[1]-side,z},{center[0]+side,center[1]+side,z},{center[0]-side,center[1]+side,z}}};
      mesh->triangles = {{{0,1,2},{0,2,3}}}; mesh->material=mat; mesh->ground_plane=true; mesh->preset_generated=true;
      adopt_mesh(scene, std::move(mesh));
    }
    for (const auto &light_json : preset.at("lights")) {
      AreaLightData light;
      const auto pos = light_json.at("position").get<std::array<float,3>>();
      const auto target = light_json.value("target", std::array<float,3>{0,0,0});
      const auto color = light_json.at("color").get<std::array<float,3>>();
      for (size_t i=0;i<3;++i) {
        light.position[i]=center[i]+pos[i]*radius;
        light.target[i]=center[i]+target[i]*radius;
        light.color[i]=color[i];
      }
      light.strength = light_json.value("strength", 100.0f) * radius * radius;
      light.size = light.size_y = light_json.value("size", 3.0f) * radius;
      light.preset_generated = true;
      scene.area_lights.push_back(light); ++scene.preset_area_light_count;
    }
    scene.palette = preset.at("palette");
    scene.active_preset = preset;
  } catch (const dslvm::Error &) { throw; }
  catch (const std::exception &error) { throw dslvm::Error("PRESET_INVALID", std::string("invalid preset data: ") + error.what()); }
}

template<typename Key, typename Value>
Value sample_keys(const std::vector<Key> &keys, double frame, const Value &base);
bool sample_visibility(const std::vector<Mesh::VisibilityKey> &keys, int frame, bool base);
template<typename Key> float sample_scalar_keys(const std::vector<Key> &keys, double frame, float base);

#include "gltf_import.hpp"
#include "obj_import.hpp"

const std::array<float, view_luts::kSize * view_luts::kSize * view_luts::kSize * 3> &view_lut(const std::string &name)
{
  if (name == "Standard") return view_luts::kStandard;
  if (name == "Filmic") return view_luts::kFilmic;
  return view_luts::kAgX;
}

std::array<float, 3> apply_view_lut(const std::array<float, view_luts::kSize * view_luts::kSize * view_luts::kSize * 3> &lut,
                                    float r, float g, float b)
{
  auto coordinate = [](float value) {
    value = std::clamp(value, 0.0f, 16.0f);
    return std::log1p(value * view_luts::kShaperScale) / view_luts::kLogRange * float(view_luts::kSize - 1);
  };
  const std::array<float, 3> c{coordinate(r), coordinate(g), coordinate(b)};
  std::array<int, 3> lo{}, hi{};
  std::array<float, 3> f{};
  for (int i = 0; i < 3; ++i) {
    lo[i] = int(std::floor(c[i])); hi[i] = std::min(lo[i] + 1, view_luts::kSize - 1); f[i] = c[i] - lo[i];
  }
  std::array<float, 3> result{};
  for (int dr = 0; dr <= 1; ++dr) for (int dg = 0; dg <= 1; ++dg) for (int db = 0; db <= 1; ++db) {
    const int ri = dr ? hi[0] : lo[0], gi = dg ? hi[1] : lo[1], bi = db ? hi[2] : lo[2];
    const float weight = (dr ? f[0] : 1.0f-f[0]) * (dg ? f[1] : 1.0f-f[1]) * (db ? f[2] : 1.0f-f[2]);
    const size_t index = (size_t(ri) * view_luts::kSize * view_luts::kSize + size_t(gi) * view_luts::kSize + bi) * 3;
    for (int channel = 0; channel < 3; ++channel) result[channel] += lut[index + channel] * weight;
  }
  return result;
}

/* The host runs Cycles without an OCIO config, so Cycles cannot convert images tagged
 * plain sRGB and would use their encoded values as linear. Integer PNG/JPEG color
 * images are sRGB-encoded Rec.709; tagging them scene_linear_srgb makes the Cycles
 * kernel decode them exactly. EXR/HDR stay on auto, which Cycles treats as linear. */
ccl::ustring color_image_colorspace(const std::string &path)
{
  std::string extension = std::filesystem::path(path).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ? ccl::u_colorspace_scene_linear_srgb : ccl::u_colorspace_auto;
}

/* Blender's Standard view on an sRGB display is the sRGB transfer function with
 * clipping, so it is evaluated exactly instead of through the LUT. */
std::array<float, 3> apply_standard_view(float r, float g, float b)
{
  auto encode = [](float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value <= 0.0031308f ? 12.92f * value : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
  };
  return {encode(r), encode(g), encode(b)};
}

class ViewTransformOutputDriver final : public ccl::OutputDriver {
 public:
  ViewTransformOutputDriver(std::string filepath, const std::string &view)
      : filepath_(std::move(filepath)), lut_(view_lut(view)), standard_(view == "Standard") {}

  void write_render_tile(const Tile &tile) override
  {
    const auto encode_start = std::chrono::steady_clock::now();
    if (!(tile.size == tile.full_size)) return;
    const int width = tile.size.x, height = tile.size.y;
    const OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::FLOAT);
    std::vector<float> source(size_t(width) * height * 4), pixels(source.size());
    if (!tile.get_pass_pixels("combined", 4, source.data())) return;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      const size_t src = (size_t(height - y - 1) * width + x) * 4;
      const size_t dst = (size_t(y) * width + x) * 4;
      const auto mapped = standard_ ? apply_standard_view(source[src], source[src + 1], source[src + 2]) :
                                      apply_view_lut(lut_, source[src], source[src + 1], source[src + 2]);
      pixels[dst] = mapped[0]; pixels[dst + 1] = mapped[1]; pixels[dst + 2] = mapped[2];
      pixels[dst + 3] = std::clamp(source[src + 3], 0.0f, 1.0f);
    }
    std::unique_ptr<OIIO::ImageOutput> output(OIIO::ImageOutput::create(filepath_));
    if (output && output->open(filepath_, spec)) {
      output->write_image(OIIO::TypeDesc::FLOAT, pixels.data());
      output->close();
    }
    encode_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - encode_start)
                     .count();
  }

  int64_t encode_ms() const { return encode_ms_; }

 private:
  std::string filepath_;
  const std::array<float, view_luts::kSize * view_luts::kSize * view_luts::kSize * 3> &lut_;
  bool standard_ = false;
  int64_t encode_ms_ = 0;
};

class PassOutputDriver final : public ccl::OutputDriver {
 public:
  explicit PassOutputDriver(std::string prefix) : prefix_(std::move(prefix)) {}
  ~PassOutputDriver() override
  {
    if (!published_) for (const auto &item : outputs_) { std::error_code ec; std::filesystem::remove(item.first, ec); }
  }

  void write_render_tile(const Tile &tile) override
  {
    if (!(tile.size == tile.full_size) || wrote_) return;
    const int width = tile.size.x, height = tile.size.y;
    if (!write_pass(tile, "combined", 4, width, height) ||
        !write_pass(tile, "depth", 1, width, height) ||
        !write_pass(tile, "normal", 3, width, height) ||
        !write_pass(tile, "albedo", 3, width, height)) {
      failed_ = true;
      return;
    }
    wrote_ = true;
  }

  bool publish()
  {
    if (failed_ || !wrote_) return false;
    for (const auto &item : outputs_) {
      std::error_code ec;
      std::filesystem::rename(item.first, item.second, ec);
      if (ec) return false;
    }
    published_ = true;
    return true;
  }

 private:
  bool write_pass(const Tile &tile, const std::string &name, const int channels, const int width, const int height)
  {
    const size_t count = size_t(width) * height * channels;
    std::vector<float> values(count);
    if (!tile.get_pass_pixels(name, channels, values.data())) return false;
    std::vector<float> ordered(count);
    for (int y = 0; y < height; ++y)
      std::copy_n(values.data() + size_t(height - 1 - y) * width * channels,
                  size_t(width) * channels,
                  ordered.data() + size_t(y) * width * channels);
    const std::string final_exr = prefix_ + "-" + name + ".exr";
    const std::string final_png = prefix_ + "-" + name + ".png";
    const std::string suffix = ".tmp-" + std::to_string(lmblender::platform::process_id());
    const std::string temp_exr = prefix_ + "-" + name + suffix + ".exr";
    const std::string temp_png = prefix_ + "-" + name + suffix + ".png";
    outputs_.push_back({temp_exr, final_exr});
    outputs_.push_back({temp_png, final_png});
    clear_temporary(temp_exr);
    clear_temporary(temp_png);

    auto exr = std::unique_ptr<OIIO::ImageOutput>(OIIO::ImageOutput::create(temp_exr));
    if (!exr) return false;
    OIIO::ImageSpec exr_spec(width, height, channels, OIIO::TypeDesc::FLOAT);
    if (!exr->open(temp_exr, exr_spec) || !exr->write_image(OIIO::TypeDesc::FLOAT, ordered.data())) return false;
    exr->close();

    std::vector<uint8_t> bytes(count);
    if (name == "depth") {
      float near_depth = std::numeric_limits<float>::infinity(), far_depth = 0.0f;
      for (float value : values) if (std::isfinite(value) && value > 0.0f && value < 1.0e9f) {
        near_depth = std::min(near_depth, value); far_depth = std::max(far_depth, value);
      }
      if (!std::isfinite(near_depth)) return false;
      const float range = std::max(1.0e-20f, far_depth - near_depth);
      for (size_t i = 0; i < count; ++i) {
        const float value = ordered[i];
        const float normalized = std::isfinite(value) && value > 0.0f && value < 1.0e9f ?
            1.0f - (value - near_depth) / range : 0.0f;
        bytes[i] = uint8_t(std::lround(std::clamp(normalized, 0.0f, 1.0f) * 255.0f));
      }
    } else {
      for (size_t pixel = 0; pixel < size_t(width) * height; ++pixel) {
        for (int channel = 0; channel < channels; ++channel) {
          float value = ordered[pixel * channels + channel];
          if (name == "combined" && channel < 3) value = apply_view_lut(view_luts::kAgX, ordered[pixel * channels], ordered[pixel * channels + 1], ordered[pixel * channels + 2])[channel];
          else if (name == "normal") value = value * 0.5f + 0.5f;
          bytes[pixel * channels + channel] = uint8_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }
      }
    }
    auto png = std::unique_ptr<OIIO::ImageOutput>(OIIO::ImageOutput::create(temp_png));
    if (!png) return false;
    OIIO::ImageSpec png_spec(width, height, channels, OIIO::TypeDesc::UINT8);
    if (!png->open(temp_png, png_spec) || !png->write_image(OIIO::TypeDesc::UINT8, bytes.data())) return false;
    png->close();
    return true;
  }

  std::string prefix_;
  std::vector<std::pair<std::string, std::string>> outputs_;
  bool wrote_ = false;
  bool failed_ = false;
  bool published_ = false;
};

std::array<float, 3> fitted_camera_position(const ccl::float3 center,
                                           const float radius,
                                           const float fov,
                                           const int width,
                                           const int height)
{
  const float vertical_half = fov * 0.5f;
  const float aspect = float(width) / float(height);
  const float half_fov = std::min(vertical_half, std::atan(std::tan(vertical_half) * aspect));
  const float distance = radius / std::sin(half_fov) * 1.2f;
  // Use a raised three-quarter view so tall imported CAD parts retain readable
  // side proportions while their top openings remain visible.
  const ccl::float3 direction = ccl::normalize(ccl::make_float3(0.50f, -0.61f, 0.25f));
  return {center.x + distance * direction.x,
          center.y + distance * direction.y,
          center.z + distance * direction.z};
}

template<typename SessionType> void run_session_with_timeout(SessionType *session, const int timeout_seconds, const std::string &temporary_output)
{
  std::atomic<bool> finished{false};
  std::atomic<bool> timed_out{false};
  std::thread watchdog([&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    while (!finished.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline &&
           !run_budget().expired.load(std::memory_order_acquire))
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!finished.exchange(true, std::memory_order_acq_rel)) {
      timed_out.store(true, std::memory_order_release);
      session->cancel();
    }
  });
  session->start();
  session->wait();
  finished.store(true, std::memory_order_release);
  watchdog.join();
  if (timed_out.load(std::memory_order_acquire)) {
    std::error_code ec;
    std::filesystem::remove(temporary_output, ec);
    if (run_budget().expired.load(std::memory_order_acquire))
      throw dslvm::Error("RUN_TIMEOUT", "render cancelled: the program exceeded its LMBLENDER_RUN_TIMEOUT_SECONDS wall-clock budget");
    throw dslvm::Error("RENDER_TIMEOUT", "render timed out after " + std::to_string(timeout_seconds) + " seconds");
  }
}

void render_xml_oracle(const Scene &source, const std::string &output, int samples, int width, int height)
{
  if (source.meshes.empty()) throw dslvm::Error("render_png requires at least one imported mesh");
  const std::string extension = std::filesystem::path(output).extension().string();
  if (output.empty() || output.front() == '-' || (extension != ".png" && extension != ".exr"))
    throw dslvm::Error("render output must be a PNG or EXR path that does not begin with '-'");
  if (samples < 1 || samples > 65536 || width < 16 || width > 4096 || height < 16 || height > 4096 ||
      uint64_t(width) * uint64_t(height) > 16'000'000)
    throw dslvm::Error("render_png samples or dimensions exceed supported limits");
  std::error_code ec;
  check_output_path(output, "render output");

  std::array<double, 3> lo{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  std::array<double, 3> hi{-lo[0], -lo[1], -lo[2]};
  for (const auto &mesh : source.meshes) if (!mesh->ground_plane) for (const auto &v : mesh->vertices)
    for (size_t axis = 0; axis < 3; ++axis) { lo[axis] = std::min(lo[axis], v[axis]); hi[axis] = std::max(hi[axis], v[axis]); }
  const ccl::float3 center = ccl::make_float3(float((lo[0] + hi[0]) * 0.5), float((lo[1] + hi[1]) * 0.5), float((lo[2] + hi[2]) * 0.5));
  const float radius = std::max(0.5f, float(std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]})) * 0.5f);
  const float fit_radius = std::max(0.5f, 0.5f * ccl::len(ccl::make_float3(hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2])));
  const float fov = source.custom_camera ? source.camera.fov_radians : 0.8f;
  ccl::float3 camera_position;
  ccl::float3 camera_target;
  if (source.custom_camera) {
    const auto fitted = source.camera.fit_to_geometry ?
        fitted_camera_position(center, fit_radius, source.camera.fov_radians, width, height) : source.camera.position;
    camera_position = ccl::make_float3(fitted[0], fitted[1], fitted[2]);
    camera_target = ccl::make_float3(source.camera.target[0], source.camera.target[1], source.camera.target[2]);
  } else {
    const float distance = radius * 1.85f / std::tan(fov * 0.5f);
    camera_position = center + ccl::make_float3(distance * 0.62f, -distance, distance * 0.8f);
    camera_target = center;
  }
  if (ccl::len_squared(camera_target - camera_position) < 1.0e-12f)
    throw dslvm::Error("camera position and target coincide; call camera_look_at or camera_orthographic, or give the preset a camera.position");
  const ccl::float3 camera_forward = ccl::normalize(camera_target - camera_position);
  ccl::float3 camera_right = ccl::cross(camera_forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
  if (ccl::len_squared(camera_right) < 1.0e-8f) camera_right = ccl::cross(camera_forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
  camera_right = ccl::normalize(camera_right);
  const ccl::float3 camera_up = ccl::normalize(ccl::cross(camera_right, camera_forward));
  /* Mirror the direct path's pixel-aspect correction so the oracle checks the same projection. */
  camera_right *= (float(width) / float(height)) * 0.5f;
  const ccl::Transform camera_tfm = ccl::make_transform(camera_right.x, camera_up.x, camera_forward.x, camera_position.x,
                                                        camera_right.y, camera_up.y, camera_forward.y, camera_position.y,
                                                        camera_right.z, camera_up.z, camera_forward.z, camera_position.z);

  ccl::log_init(nullptr);
  ccl::path_init();
  ccl::system_max_open_files_ensure();
  const ccl::vector<ccl::DeviceInfo> devices = ccl::Device::available_devices(1u << ccl::DEVICE_CPU);
  if (devices.empty()) throw dslvm::Error("Cycles CPU device is unavailable");
  ccl::SessionParams params;
  params.device = devices.front();
  params.background = true;
  params.headless = true;
  params.samples = samples;
  params.threads = std::min(source.render_threads, int(std::max(1u, std::thread::hardware_concurrency())));
  ccl::SceneParams scene_params;
  /* Test-only: render with Cycles BVH2 so test.sh can compare it with Embree in the same build. */
  if (std::getenv("LMBLENDER_TEST_BVH2")) scene_params.bvh_layout = ccl::BVH_LAYOUT_BVH2;
  auto session = ccl::make_unique<ccl::Session>(params, scene_params);
  ccl::Scene *scene = session->scene.get();

  std::ostringstream xml;
  xml << "<cycles>\n<transform matrix=\""
      << camera_tfm.x.x << " " << camera_tfm.y.x << " " << camera_tfm.z.x << " 0 "
      << camera_tfm.x.y << " " << camera_tfm.y.y << " " << camera_tfm.z.y << " 0 "
      << camera_tfm.x.z << " " << camera_tfm.y.z << " " << camera_tfm.z.z << " 0 "
      << camera_tfm.x.w << " " << camera_tfm.y.w << " " << camera_tfm.z.w << " 1\"><camera width=\"" << width << "\" height=\"" << height
      << "\" fov=\"" << fov << "\"/></transform>\n"
      << "<transform translate=\"" << (center.x - radius * 2.5f) << " " << (center.y - radius * 3.2f) << " " << (center.z + radius * 4.0f)
      << "\"><light light_type=\"point\" strength=\"550\"/></transform>\n"
      << "<shader name=\"surface\"><emission name=\"emit\" color=\"0.62 0.66 0.72\" strength=\"1\"/>"
      << "<connect from=\"emit Emission\" to=\"output Surface\"/></shader>\n"
      << "<background color=\"0.16 0.18 0.22\" strength=\"0.45\"/>\n";
  size_t total_triangles = 0;
  for (const auto &source_mesh : source.meshes) {
    if (!source_mesh->visible) continue;
    const Mesh *mesh_data = source_mesh->geometry_source ? source_mesh->geometry_source : source_mesh.get();
    if (mesh_data->vertices.size() > 10'000'000 || mesh_data->triangles.size() > 20'000'000)
      throw dslvm::Error("render_png mesh exceeds Cycles geometry budget");
    total_triangles += mesh_data->triangles.size();
    if (total_triangles > 20'000'000) throw dslvm::Error("render_png combined triangle budget exceeded");
    xml << "<state shader=\"surface\"><mesh P=\"";
    for (const auto &v : mesh_data->vertices) for (size_t axis = 0; axis < 3; ++axis)
      xml << v[axis] * source_mesh->scale[axis] + source_mesh->translation[axis] << " ";
    xml << "\" verts=\"";
    for (const auto &triangle : mesh_data->triangles) xml << triangle[0] << " " << triangle[1] << " " << triangle[2] << " ";
    xml << "\" nverts=\"";
    for (size_t i = 0; i < mesh_data->triangles.size(); ++i) xml << "3 ";
    xml << "\"/></state>\n";
  }
  xml << "</cycles>\n";
  const auto scene_path = output + ".tmp-" + std::to_string(lmblender::platform::process_id()) + ".xml";
  clear_temporary(scene_path);
  {
    std::ofstream xml_file(scene_path, std::ios::binary | std::ios::trunc);
    const std::string contents = xml.str();
    if (!xml_file || contents.size() > 256u * 1024u * 1024u || !(xml_file << contents))
      throw dslvm::Error("could not write bounded Cycles scene XML");
  }
  ccl::xml_read_file(scene, scene_path.c_str());
  std::filesystem::remove(scene_path, ec);
  if (ec) throw dslvm::Error("could not remove temporary Cycles scene XML");
  if (scene->passes.empty()) throw dslvm::Error("Cycles did not initialize its combined render pass");
  scene->passes[0]->set_name(ccl::ustring("combined"));

  const auto tmp = output + ".tmp-" + std::to_string(lmblender::platform::process_id()) + extension;
  clear_temporary(tmp);
  if (extension == ".png")
    session->set_output_driver(ccl::make_unique<ViewTransformOutputDriver>(tmp, source.view_transform));
  else
    session->set_output_driver(ccl::make_unique<ccl::OIIOOutputDriver>(tmp, "combined", [](const ccl::string &) {}));
  ccl::BufferParams buffers;
  buffers.width = buffers.full_width = width;
  buffers.height = buffers.full_height = height;
  session->reset(params, buffers);
  run_session_with_timeout(session.get(), source.render_timeout_seconds, tmp);
  session.reset();
  std::filesystem::rename(tmp, output, ec);
  if (ec) {
    std::filesystem::remove(tmp);
    throw dslvm::Error("Cycles rendered the PNG but could not publish the output");
  }
}

template<typename Key, typename Value>
Value sample_keys(const std::vector<Key> &keys, double frame, const Value &base);

void render_direct(const Scene &source, const std::string &output, int samples, int width, int height, bool emit_passes = false)
{
  const auto profile_start = std::chrono::steady_clock::now();
  const bool profile = std::getenv("LMBLENDER_PROFILE") != nullptr;
  if (source.meshes.empty()) throw dslvm::Error("render_png requires at least one imported mesh");
  const std::string extension = std::filesystem::path(output).extension().string();
  if (output.empty() || output.front() == '-' || (!emit_passes && extension != ".png" && extension != ".exr") ||
      (emit_passes && extension.size()))
    throw dslvm::Error("render output must be a PNG or EXR path, or a pass prefix without an extension, and must not begin with '-'");
  if (samples < 1 || samples > 65536 || width < 16 || width > 4096 || height < 16 || height > 4096 ||
      uint64_t(width) * uint64_t(height) > 16'000'000)
    throw dslvm::Error("render_png samples or dimensions exceed supported limits");
  std::error_code ec;
  check_output_path(output, "render output");

  std::array<double, 3> lo{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  std::array<double, 3> hi{-lo[0], -lo[1], -lo[2]};
  size_t triangle_count = 0;
  for (const auto &mesh : source.meshes) {
    if (mesh->ground_plane) continue;
    const Mesh *mesh_data = mesh->geometry_source ? mesh->geometry_source : mesh.get();
    if (mesh_data->vertices.size() > 10'000'000 || mesh_data->triangles.size() > 20'000'000)
      throw dslvm::Error("render_png mesh exceeds Cycles geometry budget");
    triangle_count += mesh_data->triangles.size();
    if (triangle_count > 20'000'000) throw dslvm::Error("render_png combined triangle budget exceeded");
    for (const auto &v : mesh_data->vertices) for (size_t axis = 0; axis < 3; ++axis) {
      const double position = v[axis] * mesh->scale[axis] + mesh->translation[axis];
      lo[axis] = std::min(lo[axis], position);
      hi[axis] = std::max(hi[axis], position);
    }
  }
  const ccl::float3 center = ccl::make_float3(float((lo[0] + hi[0]) * 0.5), float((lo[1] + hi[1]) * 0.5), float((lo[2] + hi[2]) * 0.5));
  const float radius = std::max(0.5f, float(std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]})) * 0.5f);
  const float fit_radius = std::max(0.5f, 0.5f * ccl::len(ccl::make_float3(hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2])));

  ccl::log_init(nullptr);
  if (profile) ccl::log_level_set(ccl::LOG_LEVEL_DEBUG);
  ccl::path_init();
  ccl::system_max_open_files_ensure();
  const ccl::vector<ccl::DeviceInfo> devices = ccl::Device::available_devices(1u << ccl::DEVICE_CPU);
  if (devices.empty()) throw dslvm::Error("Cycles CPU device is unavailable");
  ccl::SessionParams params;
  params.device = devices.front();
  params.background = true;
  params.headless = true;
  params.samples = samples;
  params.threads = std::min(source.render_threads, int(std::max(1u, std::thread::hardware_concurrency())));
  ccl::SceneParams scene_params;
  /* Test-only: render with Cycles BVH2 so test.sh can compare it with Embree in the same build. */
  if (std::getenv("LMBLENDER_TEST_BVH2")) scene_params.bvh_layout = ccl::BVH_LAYOUT_BVH2;
  auto session = ccl::make_unique<ccl::Session>(params, scene_params);
  ccl::Scene *scene = session->scene.get();
  scene->params.bvh_type = ccl::BVH_TYPE_STATIC;

  auto make_emission_shader = [scene](const ccl::float3 color, const float strength) {
    ccl::Shader *shader = scene->create_node<ccl::Shader>();
    auto graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::EmissionNode *emission = graph->create_node<ccl::EmissionNode>();
    emission->set_color(color);
    emission->set_strength(strength);
    graph->connect(emission->output("Emission"), graph->output()->input("Surface"));
    shader->set_graph(std::move(graph));
    shader->tag_update(scene);
    return shader;
  };
  std::map<const Material *, ccl::Shader *> material_shader_cache;
  auto make_principled_shader = [scene, &material_shader_cache](const Material &material) {
    const auto cached = material_shader_cache.find(&material);
    if (cached != material_shader_cache.end()) return cached->second;
    ccl::Shader *shader = scene->create_node<ccl::Shader>();
    auto graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::PrincipledBsdfNode *principled = graph->create_node<ccl::PrincipledBsdfNode>();
    principled->set_base_color(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
    principled->set_metallic(material.metallic);
    principled->set_roughness(material.roughness);
    principled->set_alpha(material.alpha);
    principled->set_specular_ior_level(material.specular_level);
    principled->set_specular_tint(ccl::make_float3(material.specular_tint[0], material.specular_tint[1], material.specular_tint[2]));
    principled->set_transmission_weight(material.transmission);
    principled->set_subsurface_weight(material.subsurface);
    principled->set_subsurface_radius(ccl::make_float3(material.subsurface_radius[0], material.subsurface_radius[1], material.subsurface_radius[2]));
    principled->set_subsurface_scale(material.subsurface_scale);
    principled->set_emission_color(ccl::make_float3(material.emission[0], material.emission[1], material.emission[2]));
    principled->set_emission_strength(material.emission_strength);
    ccl::ShaderOutput *base_color_source = nullptr;
    if (material.texture_kind == 1) {
      ccl::CheckerTextureNode *checker = graph->create_node<ccl::CheckerTextureNode>();
      checker->set_color1(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
      checker->set_color2(ccl::make_float3(material.texture_color2[0], material.texture_color2[1], material.texture_color2[2]));
      checker->set_scale(material.texture_scale);
      base_color_source = checker->output("Color");
    } else if (material.texture_kind == 2) {
      ccl::NoiseTextureNode *noise = graph->create_node<ccl::NoiseTextureNode>();
      noise->set_scale(material.texture_scale);
      noise->set_detail(3.0f);
      noise->set_roughness(0.65f);
      ccl::MixColorNode *mix = graph->create_node<ccl::MixColorNode>();
      mix->set_a(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
      mix->set_b(ccl::make_float3(material.texture_color2[0], material.texture_color2[1], material.texture_color2[2]));
      graph->connect(noise->output("Fac"), mix->input("Factor"));
      base_color_source = mix->output("Result");
    } else if (material.texture_kind == 3) {
      ccl::TextureCoordinateNode *coordinates = graph->create_node<ccl::TextureCoordinateNode>();
      ccl::ImageTextureNode *image = graph->create_node<ccl::ImageTextureNode>();
      image->set_filename(ccl::ustring(material.image_path));
      image->set_colorspace(color_image_colorspace(material.image_path));
      image->set_interpolation(ccl::INTERPOLATION_LINEAR);
      image->set_extension(ccl::EXTENSION_REPEAT);
      graph->connect(coordinates->output("UV"), image->input("Vector"));
      if (material.image_factor_enabled) {
        ccl::MixColorNode *multiply = graph->create_node<ccl::MixColorNode>();
        multiply->set_blend_type(ccl::NODE_MIX_MUL);
        multiply->set_fac(1.0f);
        multiply->set_a(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
        graph->connect(image->output("Color"), multiply->input("B"));
        base_color_source = multiply->output("Result");
      } else {
        base_color_source = image->output("Color");
      }
    } else if (material.texture_kind == 4) {
      ccl::TextureCoordinateNode *coordinates=graph->create_node<ccl::TextureCoordinateNode>();
      ccl::NoiseTextureNode *noise=graph->create_node<ccl::NoiseTextureNode>();noise->set_scale(material.texture_scale);noise->set_detail(2.0f);
      graph->connect(coordinates->output("Generated"),noise->input("Vector"));
      ccl::RGBRampNode *ramp=graph->create_node<ccl::RGBRampNode>();
      ccl::array<ccl::packed_float3> colors;colors.push_back_slow(ccl::make_float3(material.color[0],material.color[1],material.color[2]));colors.push_back_slow(ccl::make_float3(material.texture_color2[0],material.texture_color2[1],material.texture_color2[2]));ramp->set_ramp(colors);
      ccl::array<float> alpha;alpha.push_back_slow(1.0f);alpha.push_back_slow(1.0f);ramp->set_ramp_alpha(alpha);
      graph->connect(noise->output("Fac"),ramp->input("Fac"));base_color_source=ramp->output("Color");
    }
    auto data_texture = [&](const std::string &path) {
      ccl::TextureCoordinateNode *coordinates = graph->create_node<ccl::TextureCoordinateNode>();
      ccl::ImageTextureNode *image = graph->create_node<ccl::ImageTextureNode>();
      image->set_filename(ccl::ustring(path));
      image->set_colorspace(ccl::u_colorspace_data);
      image->set_interpolation(ccl::INTERPOLATION_LINEAR);
      image->set_extension(ccl::EXTENSION_REPEAT);
      graph->connect(coordinates->output("UV"), image->input("Vector"));
      return image;
    };
    if (!material.roughness_map_path.empty()) {
      ccl::ImageTextureNode *map = data_texture(material.roughness_map_path);
      graph->connect(map->output("Color"), principled->input("Roughness"));
    }
    if (!material.metallic_map_path.empty()) {
      ccl::ImageTextureNode *map = data_texture(material.metallic_map_path);
      graph->connect(map->output("Color"), principled->input("Metallic"));
    }
    if (!material.ao_map_path.empty()) {
      ccl::ImageTextureNode *map = data_texture(material.ao_map_path);
      ccl::MixColorNode *multiply = graph->create_node<ccl::MixColorNode>();
      multiply->set_blend_type(ccl::NODE_MIX_MUL);
      multiply->set_fac(1.0f);
      multiply->set_a(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
      if (base_color_source) graph->connect(base_color_source, multiply->input("A"));
      graph->connect(map->output("Color"), multiply->input("B"));
      base_color_source = multiply->output("Result");
    }
    if (base_color_source) graph->connect(base_color_source, principled->input("Base Color"));
    if (!material.alpha_map_path.empty()) {
      ccl::ImageTextureNode *map = data_texture(material.alpha_map_path);
      ccl::MathNode *multiply = graph->create_node<ccl::MathNode>();
      multiply->set_math_type(ccl::NODE_MATH_MULTIPLY);
      multiply->set_value2(material.alpha);
      graph->connect(map->output("Color"), multiply->input("Value1"));
      graph->connect(multiply->output("Value"), principled->input("Alpha"));
    }
    if (!material.normal_map_path.empty()) {
      ccl::ImageTextureNode *map = data_texture(material.normal_map_path);
      ccl::NormalMapNode *normal = graph->create_node<ccl::NormalMapNode>();
      normal->set_space(ccl::NODE_NORMAL_MAP_TANGENT);
      graph->connect(map->output("Color"), normal->input("Color"));
      graph->connect(normal->output("Normal"), principled->input("Normal"));
    }
    if(material.bump_enabled){
      ccl::TextureCoordinateNode *coordinates=graph->create_node<ccl::TextureCoordinateNode>();
      ccl::NoiseTextureNode *height=graph->create_node<ccl::NoiseTextureNode>();height->set_scale(material.bump_scale);height->set_detail(4.0f);height->set_roughness(0.7f);
      graph->connect(coordinates->output("Generated"),height->input("Vector"));
      ccl::BumpNode *bump=graph->create_node<ccl::BumpNode>();bump->set_strength(material.bump_strength);bump->set_distance(material.bump_distance);
      graph->connect(height->output("Fac"),bump->input("Height"));graph->connect(bump->output("Normal"),principled->input("Normal"));
    }
    if (!material.bump_map_path.empty()) {
      ccl::ImageTextureNode *height = data_texture(material.bump_map_path);
      ccl::BumpNode *bump = graph->create_node<ccl::BumpNode>();
      bump->set_strength(material.bump_map_strength);
      bump->set_distance(0.1f);
      graph->connect(height->output("Color"), bump->input("Height"));
      graph->connect(bump->output("Normal"), principled->input("Normal"));
    }
    if (material.volume_only) {
      ccl::TransparentBsdfNode *transparent = graph->create_node<ccl::TransparentBsdfNode>();
      transparent->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));
      graph->connect(transparent->output("BSDF"), graph->output()->input("Surface"));
    } else {
      ccl::ShaderOutput *surface = principled->output("BSDF");
      if (material.mix_material) {
        const Material &other = *material.mix_material;
        if (other.texture_kind != 0 || other.bump_enabled || !other.normal_map_path.empty() ||
            !other.roughness_map_path.empty() || !other.metallic_map_path.empty() || !other.ao_map_path.empty() ||
            other.mix_material || other.backface_culling || other.volume_kind != 0 || other.volume_only)
          throw dslvm::Error("mix_with currently accepts a plain-factor material without maps on the second layer");
        ccl::PrincipledBsdfNode *layer = graph->create_node<ccl::PrincipledBsdfNode>();
        layer->set_base_color(ccl::make_float3(other.color[0], other.color[1], other.color[2]));
        layer->set_metallic(other.metallic);
        layer->set_roughness(other.roughness);
        layer->set_transmission_weight(other.transmission);
        layer->set_subsurface_weight(other.subsurface);
        layer->set_subsurface_radius(ccl::make_float3(other.subsurface_radius[0], other.subsurface_radius[1], other.subsurface_radius[2]));
        layer->set_subsurface_scale(other.subsurface_scale);
        layer->set_emission_color(ccl::make_float3(other.emission[0], other.emission[1], other.emission[2]));
        layer->set_emission_strength(other.emission_strength);
        ccl::MixClosureNode *blend = graph->create_node<ccl::MixClosureNode>();
        blend->set_fac(material.mix_factor);
        if (!material.mix_factor_image.empty()) {
          ccl::ImageTextureNode *factor = data_texture(material.mix_factor_image);
          graph->connect(factor->output("Color"), blend->input("Fac"));
        }
        graph->connect(surface, blend->input("Closure1"));
        graph->connect(layer->output("BSDF"), blend->input("Closure2"));
        surface = blend->output("Closure");
      }
      if (material.backface_culling) {
        ccl::GeometryNode *geometry = graph->create_node<ccl::GeometryNode>();
        ccl::TransparentBsdfNode *transparent = graph->create_node<ccl::TransparentBsdfNode>();
        ccl::MixClosureNode *cull = graph->create_node<ccl::MixClosureNode>();
        cull->set_fac(0.0f);
        graph->connect(geometry->output("Backfacing"), cull->input("Fac"));
        graph->connect(surface, cull->input("Closure1"));
        graph->connect(transparent->output("BSDF"), cull->input("Closure2"));
        surface = cull->output("Closure");
      }
      graph->connect(surface, graph->output()->input("Surface"));
    }
    if (material.volume_kind == 1) {
      ccl::AbsorptionVolumeNode *volume = graph->create_node<ccl::AbsorptionVolumeNode>();
      volume->set_color(ccl::make_float3(material.volume_color[0], material.volume_color[1], material.volume_color[2]));
      volume->set_density(material.volume_density);
      graph->connect(volume->output("Volume"), graph->output()->input("Volume"));
    } else if (material.volume_kind == 2) {
      ccl::ScatterVolumeNode *volume = graph->create_node<ccl::ScatterVolumeNode>();
      volume->set_color(ccl::make_float3(material.volume_color[0], material.volume_color[1], material.volume_color[2]));
      volume->set_density(material.volume_density);
      volume->set_anisotropy(0.0f);
      graph->connect(volume->output("Volume"), graph->output()->input("Volume"));
    }
    shader->set_graph(std::move(graph));
    shader->tag_update(scene);
    material_shader_cache[&material] = shader;
    return shader;
  };
  auto make_light_shader = [scene]() {
    ccl::Shader *shader = scene->create_node<ccl::Shader>();
    auto graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::EmissionNode *emission = graph->create_node<ccl::EmissionNode>();
    emission->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f)); emission->set_strength(1.0f);
    graph->connect(emission->output("Emission"), graph->output()->input("Surface"));
    shader->set_graph(std::move(graph)); shader->tag_update(scene);
    return shader;
  };
  ccl::Shader *default_surface = make_emission_shader(ccl::make_float3(0.62f, 0.66f, 0.72f), 1.0f);
  ccl::Shader *world = scene->default_background;
  auto world_graph = ccl::make_unique<ccl::ShaderGraph>();
  ccl::BackgroundNode *background = world_graph->create_node<ccl::BackgroundNode>();
  background->set_color(ccl::make_float3(source.world_color[0], source.world_color[1], source.world_color[2]));
  background->set_strength(source.world_strength * (source.sky_enabled ? source.sky_intensity : 1.0f));
  scene->background->set_transparent(source.transparent_background);
  scene->background->tag_update(scene);
  if (source.sky_enabled) {
    ccl::SkyTextureNode *sky = world_graph->create_node<ccl::SkyTextureNode>();
    sky->set_sky_type(ccl::NODE_SKY_MULTIPLE_SCATTERING);
    sky->set_sun_elevation(source.sky_elevation);
    sky->set_sun_rotation(source.sky_rotation);
    sky->set_sun_intensity(source.sky_intensity);
    world_graph->connect(sky->output("Color"), background->input("Color"));
  } else if (!source.environment_image.empty()) {
    ccl::TextureCoordinateNode *coordinates = world_graph->create_node<ccl::TextureCoordinateNode>();
    ccl::EnvironmentTextureNode *environment = world_graph->create_node<ccl::EnvironmentTextureNode>();
    environment->set_filename(ccl::ustring(source.environment_image));
    environment->set_colorspace(color_image_colorspace(source.environment_image));
    environment->set_projection(ccl::NODE_ENVIRONMENT_EQUIRECTANGULAR);
    environment->set_interpolation(ccl::INTERPOLATION_LINEAR);
    environment->set_tex_mapping_rotation(ccl::make_float3(0.0f, 0.0f, source.environment_rotation));
    world_graph->connect(coordinates->output("Reflection"), environment->input("Vector"));
    world_graph->connect(environment->output("Color"), background->input("Color"));
    background->set_strength(source.environment_strength);
  }
  world_graph->connect(background->output("Background"), world_graph->output()->input("Surface"));
  world->set_graph(std::move(world_graph));
  world->tag_update(scene);

  std::map<std::tuple<const Mesh *, const Material *, bool>, ccl::Mesh *> geometry_cache;
  for (const auto &source_mesh : source.meshes) {
    if (!source_mesh->visible) continue;
    const Mesh *mesh_data = source_mesh->geometry_source ? source_mesh->geometry_source : source_mesh.get();
    const Material *material = source_mesh->material ? source_mesh->material : mesh_data->material;
    const bool needs_generated_uv = mesh_data->uvs.empty() &&
        ((material && material->texture_kind == 3) ||
         (!material && std::any_of(mesh_data->material_slots.begin(), mesh_data->material_slots.end(),
                                   [](const Material *slot) { return slot && slot->texture_kind == 3; })));
    const bool object_transform = !needs_generated_uv;
    const auto key = std::make_tuple(mesh_data, material, source_mesh->smooth);
    ccl::Mesh *mesh = nullptr;
    if (object_transform) {
      const auto cached = geometry_cache.find(key);
      if (cached != geometry_cache.end()) mesh = cached->second;
    }
    if (mesh == nullptr) {
      ccl::array<ccl::Node *> used_shaders;
      if (material || mesh_data->material_slots.empty()) {
        used_shaders.push_back_slow(material ? make_principled_shader(*material) : default_surface);
      } else {
        for (const Material *slot : mesh_data->material_slots)
          used_shaders.push_back_slow(slot ? make_principled_shader(*slot) : default_surface);
      }
      mesh = scene->create_node<ccl::Mesh>();
      mesh->resize_mesh(int(mesh_data->vertices.size()), int(mesh_data->triangles.size()));
      ccl::packed_float3 *positions = mesh->get_position_for_write();
      for (size_t i = 0; i < mesh_data->vertices.size(); ++i) {
        const auto &v = mesh_data->vertices[i];
        positions[i] = object_transform ? ccl::make_float3(float(v[0]), float(v[1]), float(v[2])) :
            ccl::make_float3(float(v[0] * source_mesh->scale[0] + source_mesh->translation[0]),
                             float(v[1] * source_mesh->scale[1] + source_mesh->translation[1]),
                             float(v[2] * source_mesh->scale[2] + source_mesh->translation[2]));
      }
      ccl::array<int> &tris = mesh->get_triangles();
      for (size_t i = 0; i < mesh_data->triangles.size(); ++i)
        for (size_t j = 0; j < 3; ++j) tris[i * 3 + j] = int(mesh_data->triangles[i][j]);
      std::fill(mesh->get_shader().begin(), mesh->get_shader().end(), 0);
      if (!material && !mesh_data->triangle_materials.empty()) {
        if (mesh_data->triangle_materials.size() != mesh_data->triangles.size())
          throw dslvm::Error("OBJ triangle material count does not match triangle count");
        for (size_t i = 0; i < mesh_data->triangle_materials.size(); ++i)
          mesh->get_shader()[i] = int(mesh_data->triangle_materials[i]);
      }
      std::fill(mesh->get_smooth().begin(), mesh->get_smooth().end(), source_mesh->smooth || !mesh_data->corner_normals.empty());
      mesh->set_used_shaders(used_shaders);
      if (!mesh_data->corner_normals.empty()) {
        if (mesh_data->corner_normals.size() != mesh_data->triangles.size() * 3)
          throw dslvm::Error("OBJ corner normal count does not match triangle count");
        ccl::Attribute *normal_attr = mesh->attributes.add(ccl::ATTR_STD_CORNER_NORMAL);
        ccl::packed_normal *normal = normal_attr->data_for_write<ccl::packed_normal>();
        for (size_t i = 0; i < mesh_data->corner_normals.size(); ++i) {
          const auto &n = mesh_data->corner_normals[i];
          normal[i] = ccl::packed_normal(ccl::make_float3(n[0], n[1], n[2]));
        }
      }
      if (!mesh_data->uvs.empty()) {
        ccl::Attribute *uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV);
        ccl::float2 *uv = uv_attr->data_for_write<ccl::float2>();
        if (mesh_data->uvs.size() != mesh_data->triangles.size() * 3)
          throw dslvm::Error("mesh UV corner count does not match triangle count");
        for (size_t i = 0; i < mesh_data->uvs.size(); ++i)
          uv[i] = ccl::make_float2(mesh_data->uvs[i][0], mesh_data->uvs[i][1]);
      }
      if (!object_transform && mesh_data->uvs.empty()) {
        /* Build a cube-style per-corner projection. A vertex generated-coordinate
         * attribute collapses one UV axis on each side face, smearing 2D images. */
        ccl::Attribute *uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV);
        ccl::float2 *uv = uv_attr->data_for_write<ccl::float2>();
        std::array<double, 3> lo{INFINITY, INFINITY, INFINITY};
        std::array<double, 3> hi{-INFINITY, -INFINITY, -INFINITY};
        for (const auto &v : mesh_data->vertices) for (size_t axis = 0; axis < 3; ++axis) {
          const double position = v[axis] * source_mesh->scale[axis];
          lo[axis] = std::min(lo[axis], position);
          hi[axis] = std::max(hi[axis], position);
        }
        for (size_t i = 0; i < mesh_data->triangles.size(); ++i) {
          const auto &tri = mesh_data->triangles[i];
          const auto &a = mesh_data->vertices[tri[0]], &b = mesh_data->vertices[tri[1]], &c = mesh_data->vertices[tri[2]];
          const double nx = std::abs((b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]));
          const double ny = std::abs((b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]));
          const double nz = std::abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]));
          const int u = nx >= ny && nx >= nz ? 1 : 0;
          const int v = nx >= ny && nx >= nz ? 2 : (ny >= nz ? 2 : 1);
          for (int corner = 0; corner < 3; ++corner) {
            const auto &p = mesh_data->vertices[tri[corner]];
            const double su = hi[u] - lo[u], sv = hi[v] - lo[v];
            uv[i * 3 + corner] = ccl::make_float2(su > 1e-20 ? float((p[u]-lo[u])/su) : 0.5f,
                                                   sv > 1e-20 ? float((p[v]-lo[v])/sv) : 0.5f);
          }
        }
      }
      mesh->tag_triangles_modified();
      mesh->tag_shader_modified();
      mesh->tag_smooth_modified();
      if (object_transform) geometry_cache[key] = mesh;
    }
    ccl::Object *object = scene->create_node<ccl::Object>();
    object->set_geometry(mesh);
    const ccl::Transform object_tfm = object_transform ?
        ccl::transform_translate(source_mesh->translation[0], source_mesh->translation[1], source_mesh->translation[2]) *
            ccl::transform_scale(source_mesh->scale[0], source_mesh->scale[1], source_mesh->scale[2]) :
        ccl::transform_identity();
    object->set_tfm(object_tfm);
    if (source.motion_blur && !source_mesh->transform_keys.empty()) {
      ccl::array<ccl::Transform> motion;
      motion.resize(3);
      for (int step = 0; step < 3; ++step) {
        const double frame = double(source.render_frame) + 0.5 * (step - 1);
        const auto position = sample_keys<Mesh::TransformKey, std::array<float, 3>>(
            source_mesh->transform_keys, frame, source_mesh->translation);
        motion[step] = ccl::transform_translate(position[0], position[1], position[2]) *
                       ccl::transform_scale(source_mesh->scale[0], source_mesh->scale[1], source_mesh->scale[2]);
      }
      object->set_motion(motion);
      object->update_motion();
    }
    object->tag_update(scene);
  }

  auto add_point_light = [scene, make_light_shader](const ccl::float3 position, const ccl::float3 color, const float strength) {
    ccl::PointLight *point = scene->create_node<ccl::PointLight>();
    point->set_strength(color * strength);
    point->set_radius(0.0f);
    ccl::Shader *light_shader = make_light_shader();
    ccl::array<ccl::Node *> light_shaders; light_shaders.push_back_slow(light_shader); point->set_used_shaders(light_shaders);
    ccl::Object *object = scene->create_node<ccl::Object>();
    object->set_geometry(point);
    ccl::Transform tfm = ccl::transform_translate(position);
    object->set_tfm(tfm);
  };
  auto add_area_light = [scene, make_light_shader](const AreaLightData &light) {
    ccl::AreaLight *area = scene->create_node<ccl::AreaLight>();
    area->set_strength(ccl::make_float3(light.color[0], light.color[1], light.color[2]) * light.strength);
    area->set_sizeu(light.size);
    area->set_sizev(light.size_y);
    area->set_ellipse(light.shape != 0);
    ccl::Shader *light_shader = make_light_shader();
    ccl::array<ccl::Node *> light_shaders; light_shaders.push_back_slow(light_shader); area->set_used_shaders(light_shaders);
    ccl::Object *object = scene->create_node<ccl::Object>();
    object->set_geometry(area);
    const ccl::float3 p = ccl::make_float3(light.position[0], light.position[1], light.position[2]);
    const ccl::float3 direction = ccl::normalize(ccl::make_float3(light.target[0], light.target[1], light.target[2]) - p);
    ccl::float3 right = ccl::cross(direction, ccl::make_float3(0.0f, 0.0f, 1.0f));
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::make_float3(1.0f, 0.0f, 0.0f);
    right = ccl::normalize(right);
    const ccl::float3 up = ccl::normalize(ccl::cross(right, direction));
    const ccl::float3 back = -direction;
    object->set_tfm(ccl::make_transform(right.x, up.x, back.x, p.x,
                                        right.y, up.y, back.y, p.y,
                                        right.z, up.z, back.z, p.z));
  };
  auto light_transform = [](const ccl::float3 position, const ccl::float3 direction) {
    const ccl::float3 forward = ccl::normalize(direction);
    ccl::float3 right = ccl::cross(forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::make_float3(1.0f, 0.0f, 0.0f);
    right = ccl::normalize(right);
    const ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
    const ccl::float3 back = -forward;
    return ccl::make_transform(right.x, up.x, back.x, position.x,
                               right.y, up.y, back.y, position.y,
                               right.z, up.z, back.z, position.z);
  };
  auto add_spot_light = [scene, make_light_shader, light_transform](const SpotLightData &light) {
    ccl::SpotLight *spot = scene->create_node<ccl::SpotLight>();
    spot->set_strength(ccl::make_float3(light.color[0], light.color[1], light.color[2]) * light.strength);
    spot->set_angle(light.angle); spot->set_smooth(light.blend); spot->set_radius(0.0f);
    ccl::Shader *shader = make_light_shader(); ccl::array<ccl::Node *> shaders; shaders.push_back_slow(shader); spot->set_used_shaders(shaders);
    ccl::Object *object = scene->create_node<ccl::Object>(); object->set_geometry(spot);
    const ccl::float3 p = ccl::make_float3(light.position[0], light.position[1], light.position[2]);
    const ccl::float3 target = ccl::make_float3(light.target[0], light.target[1], light.target[2]);
    object->set_tfm(light_transform(p, target - p));
  };
  auto add_sun_light = [scene, make_light_shader, light_transform](const SunLightData &light) {
    ccl::SunLight *sun = scene->create_node<ccl::SunLight>();
    sun->set_strength(ccl::make_float3(light.color[0], light.color[1], light.color[2]) * light.strength);
    sun->set_angle(light.angle);
    ccl::Shader *shader = make_light_shader(); ccl::array<ccl::Node *> shaders; shaders.push_back_slow(shader); sun->set_used_shaders(shaders);
    ccl::Object *object = scene->create_node<ccl::Object>(); object->set_geometry(sun);
    const ccl::float3 direction = ccl::make_float3(light.direction[0], light.direction[1], light.direction[2]);
    object->set_tfm(light_transform(ccl::make_float3(0.0f,0.0f,0.0f), direction));
  };
  if (source.point_lights.empty() && source.area_lights.empty() && source.spot_lights.empty() && source.sun_lights.empty()) {
    /* Unlit scene: a single key light so geometry-only programs still show something. */
    add_point_light(ccl::make_float3(center.x - radius * 2.5f, center.y - radius * 3.2f, center.z + radius * 4.0f),
                    ccl::make_float3(1.0f, 1.0f, 1.0f), 550.0f);
  } else {
    for (const PointLightData &light : source.point_lights)
      add_point_light(ccl::make_float3(light.position[0], light.position[1], light.position[2]),
                      ccl::make_float3(light.color[0], light.color[1], light.color[2]), light.strength);
  }
  for (const AreaLightData &light : source.area_lights) add_area_light(light);
  for (const SpotLightData &light : source.spot_lights) add_spot_light(light);
  for (const SunLightData &light : source.sun_lights) add_sun_light(light);

  scene->camera->set_full_width(width);
  scene->camera->set_full_height(height);
  scene->camera->set_fov(source.custom_camera ? source.camera.fov_radians : 0.8f);
  scene->camera->set_camera_type(source.custom_camera && source.camera.orthographic ? ccl::CAMERA_ORTHOGRAPHIC : ccl::CAMERA_PERSPECTIVE);
  scene->camera->set_nearclip(0.01f);
  scene->camera->set_farclip(10000.0f);
  scene->camera->set_aperturesize(source.camera_aperture);
  scene->camera->set_focaldistance(source.camera_focus_distance);
  scene->camera->set_shuttertime(source.motion_blur ? source.motion_shutter : 0.5f);
  ccl::Transform camera_tfm;
  if (!source.custom_camera) {
    const float distance = radius * 1.85f / std::tan(0.8f * 0.5f);
    const ccl::float3 p = center + ccl::make_float3(distance * 0.62f, -distance, distance * 0.8f);
    const ccl::float3 forward = ccl::normalize(center - p);
    ccl::float3 right = ccl::cross(forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::cross(forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
    right = ccl::normalize(right);
    const ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
    /* Same pixel-aspect correction as explicit cameras below, so the fallback view
     * is isotropic too (it used to be squeezed horizontally in landscape output). */
    right *= (float(width) / float(height)) * 0.5f;
    camera_tfm = ccl::make_transform(right.x, up.x, forward.x, p.x,
                                     right.y, up.y, forward.y, p.y,
                                     right.z, up.z, forward.z, p.z);
  } else {
    const auto fitted = source.camera.fit_to_geometry ?
        fitted_camera_position(center, fit_radius, source.camera.fov_radians, width, height) : source.camera.position;
    const auto &p = fitted;
    const auto &t = source.camera.target;
    if (ccl::len_squared(ccl::make_float3(t[0] - p[0], t[1] - p[1], t[2] - p[2])) < 1.0e-12f)
      throw dslvm::Error("camera position and target coincide; call camera_look_at or camera_orthographic, or give the preset a camera.position");
    ccl::float3 forward = ccl::normalize(ccl::make_float3(t[0] - p[0], t[1] - p[1], t[2] - p[2]));
    ccl::float3 up_ref = ccl::make_float3(0.0f, 0.0f, 1.0f);
    ccl::float3 right = ccl::cross(forward, up_ref);
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::cross(forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
    right = ccl::normalize(right);
    /* Cycles' camera projection uses a normalized screen plane whose horizontal
     * span is not pixel-corrected for portrait output. Scale the camera's
     * horizontal basis by half the pixel aspect (the camera basis spans two
     * normalized units). This keeps world units isotropic at every aspect.
     * fov_radians is the vertical field of view. */
    ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
    right *= (float(width) / float(height)) * 0.5f;
    if (source.camera.orthographic) {
      /* ortho_scale is the visible vertical world-space span. */
      right *= source.camera.ortho_scale * 0.5f;
      up *= source.camera.ortho_scale * 0.5f;
    }
    camera_tfm = ccl::make_transform(right.x, up.x, forward.x, p[0],
                                     right.y, up.y, forward.y, p[1],
                                     right.z, up.z, forward.z, p[2]);
  }
    scene->camera->set_matrix(camera_tfm);
  if (source.motion_blur && !source.camera_keys.empty()) {
    ccl::array<ccl::Transform> motion;
    motion.resize(3);
    for (int step = 0; step < 3; ++step) {
      const double frame = double(source.render_frame) + 0.5 * (step - 1);
      const auto c = sample_keys<Scene::CameraKey, std::array<float, 6>>(source.camera_keys, frame,
          {source.camera.position[0], source.camera.position[1], source.camera.position[2],
           source.camera.target[0], source.camera.target[1], source.camera.target[2]});
      const ccl::float3 p = ccl::make_float3(c[0], c[1], c[2]);
      const ccl::float3 target = ccl::make_float3(c[3], c[4], c[5]);
      const ccl::float3 forward = ccl::normalize(target - p);
      ccl::float3 right = ccl::cross(forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
      if (ccl::len_squared(right) < 1.0e-8f) right = ccl::cross(forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
      right = ccl::normalize(right) * (float(width) / float(height) * 0.5f);
      const ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
      motion[step] = ccl::make_transform(right.x, up.x, forward.x, p.x,
                                         right.y, up.y, forward.y, p.y,
                                         right.z, up.z, forward.z, p.z);
    }
    scene->camera->set_motion(motion);
  }
  scene->camera->need_flags_update = true;
  scene->camera->update(scene);
  scene->integrator->set_max_bounce(source.max_bounces);
  scene->integrator->set_sample_clamp_direct(source.clamp_direct);
  scene->integrator->set_sample_clamp_indirect(source.clamp_indirect);
  scene->integrator->set_seed(source.render_seed);
  scene->integrator->set_use_denoise(source.denoise);
  scene->integrator->set_denoiser_quality(source.denoise_quality);
  scene->integrator->set_denoiser_prefilter(source.denoise_prefilter);
  scene->integrator->set_motion_blur(source.motion_blur);

  if (emit_passes) {
    auto add_pass = [scene](const ccl::PassType type, const char *name) {
      auto pass = ccl::make_unique<ccl::Pass>();
      pass->set_type(type);
      pass->set_mode(ccl::PassMode::NOISY);
      pass->set_name(ccl::ustring(name));
      pass->set_owner(scene);
      scene->passes.push_back(std::move(pass));
    };
    add_pass(ccl::PASS_DEPTH, "depth");
    add_pass(ccl::PASS_NORMAL, "normal");
    add_pass(ccl::PASS_DIFFUSE_COLOR, "albedo");
    scene->film->update_passes(scene);
  }

  if (scene->passes.empty()) throw dslvm::Error("Cycles did not initialize its combined render pass");
  scene->passes[0]->set_name(ccl::ustring("combined"));
  const auto tmp = output + ".tmp-" + std::to_string(lmblender::platform::process_id()) + (emit_passes ? ".png" : extension);
  clear_temporary(tmp);
  PassOutputDriver *pass_driver = nullptr;
  ViewTransformOutputDriver *png_driver = nullptr;
  if (emit_passes) {
    auto driver = ccl::make_unique<PassOutputDriver>(output);
    pass_driver = driver.get();
    session->set_output_driver(std::move(driver));
  } else if (extension == ".png") {
    auto driver = ccl::make_unique<ViewTransformOutputDriver>(tmp, source.view_transform);
    png_driver = driver.get();
    session->set_output_driver(std::move(driver));
  }
  else
    session->set_output_driver(ccl::make_unique<ccl::OIIOOutputDriver>(tmp, "combined", [](const ccl::string &) {}));
  ccl::BufferParams buffers;
  buffers.width = buffers.full_width = width;
  buffers.height = buffers.full_height = height;
  const auto before_reset = std::chrono::steady_clock::now();
  session->reset(params, buffers);
  const auto before_render = std::chrono::steady_clock::now();
  run_session_with_timeout(session.get(), source.render_timeout_seconds, tmp);
  const auto after_render = std::chrono::steady_clock::now();
  if (profile) {
    const auto setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(before_reset - profile_start).count();
    const auto reset_ms = std::chrono::duration_cast<std::chrono::milliseconds>(before_render - before_reset).count();
    const auto render_output_ms = std::chrono::duration_cast<std::chrono::milliseconds>(after_render - before_render).count();
    const int64_t png_encode_ms = png_driver ? png_driver->encode_ms() : 0;
    std::cerr << "LMBLENDER_PROFILE width=" << width << " height=" << height << " spp=" << samples
              << " threads=" << params.threads << " scene_build_ms=" << setup_ms << " bvh_ms=" << reset_ms
              << " trace_denoise_encode_ms=" << render_output_ms << " png_transform_encode_ms=" << png_encode_ms
              << " trace_denoise_ms=" << std::max<int64_t>(0, render_output_ms - png_encode_ms) << '\n';
  }
  if (emit_passes && (!pass_driver || !pass_driver->publish()))
    throw dslvm::Error("Cycles did not produce all combined/depth/normal/albedo pass files");
  session.reset();
  if (!emit_passes) {
    std::filesystem::rename(tmp, output, ec);
    if (ec) {
      std::filesystem::remove(tmp);
      throw dslvm::Error("Cycles rendered the PNG but could not publish the output");
    }
  }
}

void render_image(const Scene &source, const std::string &output, int samples, int width, int height)
{
  if (std::getenv("LMBLENDER_TEST_XML_ORACLE"))
    render_xml_oracle(source, output, samples, width, height);
  else if (source.motion_blur && (!source.camera_keys.empty() || std::any_of(source.meshes.begin(), source.meshes.end(), [](const auto &m) { return !m->transform_keys.empty(); }))) {
    /* Cycles' aarch64 CPU motion-time interpolation can receive an invalid ray
     * time under GCC 14. Render shutter samples as ordinary static scenes and
     * average their PNGs. This gives deterministic transform blur without
     * entering the unstable per-ray interpolation path. */
    Scene &scene = const_cast<Scene &>(source);
    const bool old_motion = scene.motion_blur;
    const int old_frame = scene.render_frame;
    const auto old_camera_position = scene.camera.position;
    const auto old_camera_target = scene.camera.target;
    std::vector<std::array<float,3>> old_transforms;
    old_transforms.reserve(scene.meshes.size());
    for (const auto &mesh : scene.meshes) old_transforms.push_back(mesh->translation);
    std::vector<bool> old_visibility; for (const auto &mesh : scene.meshes) old_visibility.push_back(mesh->visible);
    std::vector<std::array<float,3>> old_colors;
    old_colors.reserve(scene.materials.size());
    for (const auto &material : scene.materials) old_colors.push_back(material->color);
    std::vector<float> old_emissions; for (const auto &material : scene.materials) old_emissions.push_back(material->emission_strength);
    std::vector<float> old_light_strengths; for (const auto &light : scene.point_lights) old_light_strengths.push_back(light.strength);
    const float old_fov = scene.camera.fov_radians, old_ortho = scene.camera.ortho_scale;
    std::vector<std::string> temporaries;
    const std::string staged = output + ".motion-" + std::to_string(lmblender::platform::process_id()) + ".png";
    clear_temporary(staged);
    try {
      scene.motion_blur = false;
      constexpr int temporal_samples = 9;
      for (int i = 0; i < temporal_samples; ++i) {
        const double frame = double(old_frame) + scene.motion_shutter * (double(i) / (temporal_samples - 1) - 0.5);
        for (size_t m = 0; m < scene.meshes.size(); ++m)
          scene.meshes[m]->translation = sample_keys<Mesh::TransformKey, std::array<float,3>>(
              scene.meshes[m]->transform_keys, frame, old_transforms[m]);
        for (size_t m = 0; m < scene.meshes.size(); ++m)
          scene.meshes[m]->visible = sample_visibility(scene.meshes[m]->visibility_keys, int(std::lround(frame)), old_visibility[m]);
        for (size_t m = 0; m < scene.materials.size(); ++m)
          scene.materials[m]->color = sample_keys<Material::ColorKey, std::array<float,3>>(
              scene.materials[m]->color_keys, frame, old_colors[m]);
        for (size_t m = 0; m < scene.materials.size(); ++m)
          scene.materials[m]->emission_strength = sample_scalar_keys(scene.materials[m]->emission_keys, frame, old_emissions[m]);
        for (size_t l = 0; l < scene.point_lights.size(); ++l)
          scene.point_lights[l].strength = sample_scalar_keys(scene.point_lights[l].strength_keys, frame, old_light_strengths[l]);
        if (!scene.camera_optics_keys.empty()) {
          const auto optics = sample_keys<Scene::CameraOpticsKey, std::array<float, 2>>(
              scene.camera_optics_keys, frame, {old_fov, old_ortho});
          scene.camera.fov_radians = optics[0]; scene.camera.ortho_scale = optics[1];
        }
        if (!scene.camera_keys.empty()) {
          const auto camera = sample_keys<Scene::CameraKey, std::array<float,6>>(
              scene.camera_keys, frame,
              {old_camera_position[0],old_camera_position[1],old_camera_position[2],
               old_camera_target[0],old_camera_target[1],old_camera_target[2]});
          scene.camera.position={camera[0],camera[1],camera[2]};
          scene.camera.target={camera[3],camera[4],camera[5]};
        }
        const std::string temp = output + ".shutter-" + std::to_string(lmblender::platform::process_id()) + "-" + std::to_string(i) + ".png";
        temporaries.push_back(temp);
        clear_temporary(temp);
        scene.render_frame = old_frame;
        render_direct(scene, temp, samples, width, height);
      }
      OIIO::ImageSpec spec;
      std::vector<float> accumulated;
      for (size_t i = 0; i < temporaries.size(); ++i) {
        auto input = OIIO::ImageInput::open(temporaries[i]);
        if (!input) throw dslvm::Error("could not read a Cycles shutter sample for motion blur");
        if (i == 0) {
          spec = input->spec();
          if (spec.width != width || spec.height != height || spec.nchannels < 3 || spec.nchannels > 4)
            throw dslvm::Error("Cycles shutter sample has unexpected image dimensions or channels");
          accumulated.assign(size_t(width) * size_t(height) * size_t(spec.nchannels), 0.0f);
        }
        std::vector<float> pixels(accumulated.size());
        if (!input->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, pixels.data()))
          throw dslvm::Error("could not decode a Cycles shutter sample for motion blur");
        input->close();
        for (size_t p = 0; p < pixels.size(); ++p) accumulated[p] += pixels[p] / float(temporaries.size());
      }
      spec.format = OIIO::TypeDesc::FLOAT;
      auto writer = OIIO::ImageOutput::create(staged);
      if (!writer || !writer->open(staged, spec) || !writer->write_image(OIIO::TypeDesc::FLOAT, accumulated.data()))
        throw dslvm::Error("could not write the temporally sampled motion-blur PNG");
      writer->close();
      std::error_code ec;
      std::filesystem::rename(staged, output, ec);
      if (ec) { std::filesystem::remove(staged); throw dslvm::Error("could not atomically publish motion-blur PNG"); }
    }
    catch (...) {
      scene.motion_blur = old_motion; scene.render_frame = old_frame;
      scene.camera.position = old_camera_position; scene.camera.target = old_camera_target;
      for (size_t i = 0; i < scene.meshes.size(); ++i) scene.meshes[i]->translation = old_transforms[i];
      for (size_t i = 0; i < scene.meshes.size(); ++i) scene.meshes[i]->visible = old_visibility[i];
      for (size_t i = 0; i < scene.materials.size(); ++i) scene.materials[i]->color = old_colors[i];
      for (size_t i = 0; i < scene.materials.size(); ++i) scene.materials[i]->emission_strength = old_emissions[i];
      for (size_t i = 0; i < scene.point_lights.size(); ++i) scene.point_lights[i].strength = old_light_strengths[i];
      scene.camera.fov_radians = old_fov; scene.camera.ortho_scale = old_ortho;
      { std::error_code ec; std::filesystem::remove(staged, ec); }
      for (const auto &temp : temporaries) { std::error_code ec; std::filesystem::remove(temp, ec); }
      throw;
    }
    scene.motion_blur = old_motion; scene.render_frame = old_frame;
    scene.camera.position = old_camera_position; scene.camera.target = old_camera_target;
    for (size_t i = 0; i < scene.meshes.size(); ++i) scene.meshes[i]->translation = old_transforms[i];
    for (size_t i = 0; i < scene.meshes.size(); ++i) scene.meshes[i]->visible = old_visibility[i];
    for (size_t i = 0; i < scene.materials.size(); ++i) scene.materials[i]->color = old_colors[i];
    for (size_t i = 0; i < scene.materials.size(); ++i) scene.materials[i]->emission_strength = old_emissions[i];
    for (size_t i = 0; i < scene.point_lights.size(); ++i) scene.point_lights[i].strength = old_light_strengths[i];
    scene.camera.fov_radians = old_fov; scene.camera.ortho_scale = old_ortho;
    for (const auto &temp : temporaries) { std::error_code ec; std::filesystem::remove(temp, ec); }
  }
  else
    render_direct(source, output, samples, width, height);
}

template<typename Key, typename Value>
Value sample_keys(const std::vector<Key> &keys, double frame, const Value &base)
{
  if (keys.empty()) return base;
  auto hi = std::lower_bound(keys.begin(), keys.end(), frame,
                             [](const Key &key, double f) { return key.frame < f; });
  if (hi == keys.begin()) return hi->value;
  if (hi == keys.end()) return keys.back().value;
  if (hi->frame == frame) return hi->value;
  const Key &b = *hi, &a = *(hi - 1);
  double t = double(frame - a.frame) / double(b.frame - a.frame);
  if (a.interpolation == "ease") t = t * t * (3.0 - 2.0 * t);
  Value result{};
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = typename Value::value_type(a.value[i] + (b.value[i] - a.value[i]) * t);
  return result;
}

bool sample_visibility(const std::vector<Mesh::VisibilityKey> &keys, int frame, bool base)
{
  if (keys.empty()) return base;
  auto hi = std::lower_bound(keys.begin(), keys.end(), frame,
                             [](const auto &key, int f) { return key.frame < f; });
  if (hi == keys.begin()) return hi->value;
  if (hi == keys.end()) return keys.back().value;
  if (hi->frame == frame) return hi->value;
  return (hi - 1)->value;
}

template<typename Key>
float sample_scalar_keys(const std::vector<Key> &keys, double frame, float base)
{
  if (keys.empty()) return base;
  auto hi = std::lower_bound(keys.begin(), keys.end(), frame,
                             [](const Key &key, double f) { return key.frame < f; });
  if (hi == keys.begin()) return hi->value;
  if (hi == keys.end()) return keys.back().value;
  if (hi->frame == frame) return hi->value;
  const Key &a = *(hi - 1), &b = *hi;
  double t = double(frame - a.frame) / double(b.frame - a.frame);
  if (a.interpolation == "ease") t = t * t * (3.0 - 2.0 * t);
  return float(a.value + (b.value - a.value) * t);
}

struct AnimationBase {
  std::vector<std::array<float, 3>> translations, colors;
  std::vector<bool> visibility;
  std::vector<float> emissions, point_strengths;
  std::array<float, 6> camera{};
  std::array<float, 2> optics{};
};

AnimationBase capture_animation(const Scene &scene)
{
  AnimationBase base;
  for (const auto &mesh : scene.meshes) { base.translations.push_back(mesh->translation); base.visibility.push_back(mesh->visible); }
  for (const auto &material : scene.materials) { base.colors.push_back(material->color); base.emissions.push_back(material->emission_strength); }
  for (const auto &light : scene.point_lights) base.point_strengths.push_back(light.strength);
  base.camera = {scene.camera.position[0], scene.camera.position[1], scene.camera.position[2],
                 scene.camera.target[0], scene.camera.target[1], scene.camera.target[2]};
  base.optics = {scene.camera.fov_radians, scene.camera.ortho_scale};
  return base;
}

void apply_animation(Scene &scene, int frame, const AnimationBase &base)
{
  scene.render_frame = frame;
  for (size_t i = 0; i < scene.meshes.size(); ++i) {
    scene.meshes[i]->translation = sample_keys<Mesh::TransformKey, std::array<float, 3>>(scene.meshes[i]->transform_keys, frame, base.translations[i]);
    scene.meshes[i]->visible = sample_visibility(scene.meshes[i]->visibility_keys, frame, base.visibility[i]);
  }
  for (size_t i = 0; i < scene.materials.size(); ++i) {
    scene.materials[i]->color = sample_keys<Material::ColorKey, std::array<float, 3>>(scene.materials[i]->color_keys, frame, base.colors[i]);
    scene.materials[i]->emission_strength = sample_scalar_keys(scene.materials[i]->emission_keys, frame, base.emissions[i]);
  }
  for (size_t i = 0; i < scene.point_lights.size(); ++i)
    scene.point_lights[i].strength = sample_scalar_keys(scene.point_lights[i].strength_keys, frame, base.point_strengths[i]);
  if (!scene.camera_keys.empty()) {
    const auto c = sample_keys<Scene::CameraKey, std::array<float, 6>>(scene.camera_keys, frame, base.camera);
    for (size_t j = 0; j < 3; ++j) { scene.camera.position[j] = c[j]; scene.camera.target[j] = c[j + 3]; }
  }
  if (!scene.camera_optics_keys.empty()) {
    const auto optics = sample_keys<Scene::CameraOpticsKey, std::array<float, 2>>(scene.camera_optics_keys, frame, base.optics);
    scene.camera.fov_radians = optics[0]; scene.camera.ortho_scale = optics[1];
  }
}

void restore_animation(Scene &scene, const AnimationBase &base)
{
  for (size_t i = 0; i < scene.meshes.size(); ++i) { scene.meshes[i]->translation = base.translations[i]; scene.meshes[i]->visible = base.visibility[i]; }
  for (size_t i = 0; i < scene.materials.size(); ++i) { scene.materials[i]->color = base.colors[i]; scene.materials[i]->emission_strength = base.emissions[i]; }
  for (size_t i = 0; i < scene.point_lights.size(); ++i) scene.point_lights[i].strength = base.point_strengths[i];
  scene.camera.position = {base.camera[0], base.camera[1], base.camera[2]};
  scene.camera.target = {base.camera[3], base.camera[4], base.camera[5]};
  scene.camera.fov_radians = base.optics[0]; scene.camera.ortho_scale = base.optics[1];
}

std::string sequence_path(std::string pattern, int frame)
{
  const std::string replacement = std::to_string(frame);
  size_t pos = pattern.find("{frame}");
  if (pos != std::string::npos) pattern.replace(pos, 7, replacement);
  else if ((pos = pattern.find("####")) != std::string::npos) {
    std::ostringstream padded; padded.width(4); padded.fill('0'); padded << frame;
    pattern.replace(pos, 4, padded.str());
  } else throw dslvm::Error("render_sequence pattern must contain {frame} or ####");
  return pattern;
}

void png_put32(std::vector<unsigned char> &out, uint32_t v)
{ out.push_back(v>>24); out.push_back(v>>16); out.push_back(v>>8); out.push_back(v); }
uint32_t png_crc32(const unsigned char *p, size_t n)
{
  uint32_t c=0xffffffffu;
  for(size_t i=0;i<n;++i){c^=p[i];for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}
  return c^0xffffffffu;
}
void png_chunk(std::vector<unsigned char> &out,const char *type,const std::vector<unsigned char> &data)
{
  png_put32(out,uint32_t(data.size())); const size_t begin=out.size();
  for(int i=0;i<4;++i)out.push_back(type[i]); out.insert(out.end(),data.begin(),data.end());
  png_put32(out,png_crc32(out.data()+begin,out.size()-begin));
}
void write_apng_from_pngs(const std::string &output,const std::vector<std::string> &paths,int fps)
{
  if(paths.empty()||paths.size()>240||fps<1||fps>240)throw dslvm::Error("APNG needs 1..240 frames and frame rate 1..240");
  std::vector<std::vector<unsigned char>> frame_data; std::vector<unsigned char> ihdr; uint32_t width=0,height=0;
  for(const std::string &path:paths){
    std::ifstream in(path,std::ios::binary); if(!in)throw dslvm::Error("could not read rendered APNG frame");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.size()<33||bytes.size()>128u*1024u*1024u||std::memcmp(bytes.data(),"\x89PNG\r\n\x1a\n",8)!=0)throw dslvm::Error("rendered animation frame is not a bounded PNG");
    std::vector<unsigned char> compressed; size_t pos=8;
    while(pos+12<=bytes.size()){
      uint32_t n=(uint32_t(bytes[pos])<<24)|(uint32_t(bytes[pos+1])<<16)|(uint32_t(bytes[pos+2])<<8)|bytes[pos+3];
      if(n>bytes.size()-pos-12)throw dslvm::Error("truncated PNG chunk in rendered animation frame");
      const char *type=reinterpret_cast<const char*>(bytes.data()+pos+4);
      if(std::memcmp(type,"IHDR",4)==0){ihdr.assign(bytes.begin()+pos+8,bytes.begin()+pos+8+n);if(n!=13)throw dslvm::Error("invalid PNG IHDR");}
      if(std::memcmp(type,"IDAT",4)==0)compressed.insert(compressed.end(),bytes.begin()+pos+8,bytes.begin()+pos+8+n);
      if(std::memcmp(type,"IEND",4)==0)break; pos+=size_t(n)+12;
    }
    if(ihdr.size()!=13||compressed.empty())throw dslvm::Error("PNG frame is missing image data");
    const uint32_t w=(uint32_t(ihdr[0])<<24)|(uint32_t(ihdr[1])<<16)|(uint32_t(ihdr[2])<<8)|ihdr[3];
    const uint32_t h=(uint32_t(ihdr[4])<<24)|(uint32_t(ihdr[5])<<16)|(uint32_t(ihdr[6])<<8)|ihdr[7];
    if(frame_data.empty()){width=w;height=h;} else if(w!=width||h!=height)throw dslvm::Error("APNG frames must have identical dimensions");
    frame_data.push_back(std::move(compressed));
  }
  std::vector<unsigned char> out{137,80,78,71,13,10,26,10}; png_chunk(out,"IHDR",ihdr); png_chunk(out,"sRGB",{0});
  std::vector<unsigned char> actl;png_put32(actl,uint32_t(paths.size()));png_put32(actl,0);png_chunk(out,"acTL",actl);
  uint32_t seq=0;
  for(size_t i=0;i<frame_data.size();++i){
    std::vector<unsigned char> ctl;png_put32(ctl,seq++);png_put32(ctl,width);png_put32(ctl,height);png_put32(ctl,0);png_put32(ctl,0);ctl.push_back(0);ctl.push_back(1);ctl.push_back(uint8_t(fps>>8));ctl.push_back(uint8_t(fps));ctl.push_back(0);ctl.push_back(0);png_chunk(out,"fcTL",ctl);
    if(i==0)png_chunk(out,"IDAT",frame_data[i]);else{std::vector<unsigned char> d;png_put32(d,seq++);d.insert(d.end(),frame_data[i].begin(),frame_data[i].end());png_chunk(out,"fdAT",d);}
  }
  png_chunk(out,"IEND",{});
  std::error_code ec;
  check_output_path(output, "APNG output");
  std::string tmp=output+".tmp."+std::to_string(lmblender::platform::process_id());
  clear_temporary(tmp);
  {std::ofstream file(tmp,std::ios::binary|std::ios::trunc);if(!file||!file.write(reinterpret_cast<const char*>(out.data()),std::streamsize(out.size())))throw dslvm::Error("could not write staged APNG");}
  std::filesystem::rename(tmp,output,ec);if(ec){std::filesystem::remove(tmp);throw dslvm::Error("could not atomically publish APNG");}
}

Mesh read_ply_ascii(const std::string &path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec)
    throw dslvm::Error("PLY input must be a readable regular file");
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size == 0 || size > 256u * 1024u * 1024u)
    throw dslvm::Error("PLY input must be non-empty and at most 256 MiB");
  std::ifstream in(path, std::ios::binary);
  if (!in) throw dslvm::Error("could not open PLY input");
  std::string line;
  if (!std::getline(in, line) || line != "ply") throw dslvm::Error("invalid PLY magic");
  bool ascii = false, ended = false, vertices_seen = false, faces_seen = false;
  size_t vertex_count = 0, face_count = 0;
  std::string element;
  bool xyz[3] = {false, false, false};
  while (std::getline(in, line)) {
    if (line.size() > 4096) throw dslvm::Error("PLY header line exceeds 4096 bytes");
    std::istringstream row(line);
    std::string key;
    row >> key;
    if (key == "format") {
      std::string format, version;
      row >> format >> version;
      ascii = format == "ascii" && version == "1.0";
      if (!ascii) throw dslvm::Error("PLY supports only ASCII format 1.0");
    } else if (key == "element") {
      std::string name, count_text;
      row >> name >> count_text;
      size_t count;
      try { count = std::stoull(count_text); } catch (...) { throw dslvm::Error("invalid PLY element count"); }
      if (name == "vertex") { vertex_count = count; vertices_seen = true; if (count > 10'000'000) throw dslvm::Error("PLY vertex budget exceeded"); }
      else if (name == "face") { face_count = count; faces_seen = true; if (count > 20'000'000) throw dslvm::Error("PLY face budget exceeded"); }
      element = name;
    } else if (key == "property" && element == "vertex") {
      std::string type, name; row >> type >> name;
      if (name == "x") xyz[0] = true;
      if (name == "y") xyz[1] = true;
      if (name == "z") xyz[2] = true;
      if (type.rfind("list", 0) == 0) throw dslvm::Error("PLY vertex list properties are unsupported");
    } else if (key == "end_header") { ended = true; break; }
  }
  if (!ended || !ascii || !vertices_seen || !faces_seen || !xyz[0] || !xyz[1] || !xyz[2])
    throw dslvm::Error("PLY requires ASCII 1.0, vertex x/y/z, and face elements");
  Mesh mesh;
  mesh.vertices.reserve(vertex_count);
  for (size_t i = 0; i < vertex_count; ++i) {
    if (!std::getline(in, line)) throw dslvm::Error("truncated PLY vertex data");
    std::istringstream row(line);
    std::array<double, 3> v{};
    if (!(row >> v[0] >> v[1] >> v[2]) || !std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2]))
      throw dslvm::Error("invalid PLY vertex coordinates");
    mesh.vertices.push_back(v);
  }
  for (size_t i = 0; i < face_count; ++i) {
    if (!std::getline(in, line)) throw dslvm::Error("truncated PLY face data");
    std::istringstream row(line);
    size_t n;
    if (!(row >> n) || n < 3 || n > vertex_count) throw dslvm::Error("invalid PLY face vertex count");
    std::vector<uint32_t> face(n);
    for (uint32_t &index : face) if (!(row >> index) || index >= vertex_count) throw dslvm::Error("PLY face index is out of range");
    for (size_t j = 1; j + 1 < n; ++j) mesh.triangles.push_back({face[0], face[j], face[j+1]});
    if (mesh.triangles.size() > 20'000'000) throw dslvm::Error("PLY triangle budget exceeded");
  }
  if (mesh.vertices.empty() || mesh.triangles.empty()) throw dslvm::Error("PLY contains no triangle mesh");
  return mesh;
}

uint32_t read_u32_le(const unsigned char *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
float read_f32_le(const unsigned char *p) {
  const uint32_t bits = read_u32_le(p);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
Mesh read_stl(const std::string &path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) throw dslvm::Error("STL input must be a readable regular file");
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > 256u * 1024u * 1024u) throw dslvm::Error("STL input exceeds the 256 MiB limit or cannot be read");
  std::ifstream in(path, std::ios::binary);
  if (!in) throw dslvm::Error("could not open STL input");
  std::vector<unsigned char> bytes(static_cast<size_t>(size));
  in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!in && !bytes.empty()) throw dslvm::Error("could not read STL input");
  Mesh mesh;
  if (bytes.size() >= 84 && 84ull + uint64_t(read_u32_le(bytes.data() + 80)) * 50ull == bytes.size()) {
    const uint32_t count = read_u32_le(bytes.data() + 80);
    if (count > 5'000'000) throw dslvm::Error("STL triangle budget exceeded");
    size_t offset = 84;
    for (uint32_t t = 0; t < count; ++t, offset += 50) {
      std::array<uint32_t, 3> tri{};
      for (int v = 0; v < 3; ++v) {
        std::array<double, 3> p{};
        for (int c = 0; c < 3; ++c) {
          p[c] = read_f32_le(bytes.data() + offset + 12 + (v * 3 + c) * 4);
          if (!std::isfinite(p[c])) throw dslvm::Error("STL contains a non-finite vertex");
        }
        tri[v] = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(p);
      }
      mesh.triangles.push_back(tri);
    }
  } else {
    std::istringstream text(std::string(bytes.begin(), bytes.end()));
    std::string word;
    std::vector<uint32_t> tri;
    while (text >> word) {
      if (word != "vertex") continue;
      std::array<double, 3> p{};
      if (!(text >> p[0] >> p[1] >> p[2]) || !std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
        throw dslvm::Error("ASCII STL contains an invalid vertex");
      tri.push_back(static_cast<uint32_t>(mesh.vertices.size()));
      mesh.vertices.push_back(p);
      if (tri.size() == 3) { mesh.triangles.push_back({tri[0], tri[1], tri[2]}); tri.clear(); }
      if (mesh.triangles.size() > 5'000'000) throw dslvm::Error("STL triangle budget exceeded");
    }
    if (!tri.empty()) throw dslvm::Error("ASCII STL has a truncated triangle");
  }
  if (mesh.triangles.empty()) throw dslvm::Error("STL contains no triangles or is malformed");
  return mesh;
}

void append_triangle(Mesh &mesh, uint32_t a, uint32_t b, uint32_t c)
{
  mesh.triangles.push_back({a, b, c});
}

/* Compact, bounded coordinate list for DSL programs: "x,y,z; x,y,z". */
std::vector<std::array<double, 3>> parse_points(const std::string &input, int dimensions,
                                                 size_t minimum, size_t maximum)
{
  if (input.empty() || input.size() > 65536) throw dslvm::Error("path text must be 1..65536 bytes");
  std::vector<std::array<double, 3>> points;
  std::istringstream rows(input);
  std::string row;
  while (std::getline(rows, row, ';')) {
    if (points.size() == maximum) throw dslvm::Error("path point budget exceeded");
    std::istringstream fields(row);
    std::string field;
    std::array<double, 3> point{};
    for (int i = 0; i < dimensions; ++i) {
      if (!std::getline(fields, field, ',')) throw dslvm::Error("path point has missing coordinate");
      size_t used = 0;
      try { point[i] = std::stod(field, &used); }
      catch (...) { throw dslvm::Error("path coordinate is not a number"); }
      if (field.find_first_not_of(" \t\r\n", used) != std::string::npos ||
          !std::isfinite(point[i]) || std::abs(point[i]) > 1'000'000)
        throw dslvm::Error("path coordinate must be finite and within scene bounds");
    }
    if (std::getline(fields, field, ',')) throw dslvm::Error("path point has extra coordinate");
    points.push_back(point);
  }
  if (points.size() < minimum) throw dslvm::Error("path has too few points");
  return points;
}

std::array<double, 3> unit3(std::array<double, 3> v)
{
  const double n = std::hypot(v[0], v[1], v[2]);
  if (n < 1e-8) throw dslvm::Error("path contains coincident points");
  for (double &x : v) x /= n;
  return v;
}
std::array<double, 3> cross3(const std::array<double, 3> &a, const std::array<double, 3> &b)
{
  return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}

/* Parallel enough for short paths: each section uses the local bisector tangent.
 * Closed profiles make an honest loft; a circular profile with tapered radii is a strand. */
void append_sweep(Mesh &mesh, const std::vector<std::array<double, 3>> &path,
                  const std::vector<std::array<double, 3>> &profile,
                  double base_radius, double tip_radius, bool cap)
{
  const uint32_t base = uint32_t(mesh.vertices.size()), sides = uint32_t(profile.size());
  std::array<double,3> previous_u{};
  for (size_t i = 0; i < path.size(); ++i) {
    const auto &before = path[i ? i-1 : i], &after = path[i+1 < path.size() ? i+1 : i];
    const auto tangent = unit3({after[0]-before[0], after[1]-before[1], after[2]-before[2]});
    std::array<double,3> u;
    if (i==0) {
      const auto ref=std::abs(tangent[2])<0.9 ? std::array<double,3>{0,0,1} : std::array<double,3>{0,1,0};
      u=unit3(cross3(tangent,ref));
    } else {
      const double dot=previous_u[0]*tangent[0]+previous_u[1]*tangent[1]+previous_u[2]*tangent[2];
      u=unit3({previous_u[0]-dot*tangent[0],previous_u[1]-dot*tangent[1],previous_u[2]-dot*tangent[2]});
    }
    previous_u=u;
    const auto v = cross3(tangent, u);
    const double radius = base_radius + (tip_radius-base_radius)*double(i)/double(path.size()-1);
    for (const auto &q : profile)
      mesh.vertices.push_back({path[i][0]+radius*(u[0]*q[0]+v[0]*q[1]),
                               path[i][1]+radius*(u[1]*q[0]+v[1]*q[1]),
                               path[i][2]+radius*(u[2]*q[0]+v[2]*q[1])});
  }
  for (uint32_t i=0; i+1<path.size(); ++i) for (uint32_t j=0; j<sides; ++j) {
    const uint32_t n=(j+1)%sides, a=base+i*sides+j, b=base+i*sides+n,
                   c=base+(i+1)*sides+n, d=base+(i+1)*sides+j;
    append_triangle(mesh,a,b,c); append_triangle(mesh,a,c,d);
  }
  if (cap) for (uint32_t j=1; j+1<sides; ++j) {
    append_triangle(mesh,base,base+j+1,base+j);
    append_triangle(mesh,base+uint32_t(path.size()-1)*sides,
                    base+uint32_t(path.size()-1)*sides+j,
                    base+uint32_t(path.size()-1)*sides+j+1);
  }
}

void append_box(Mesh &mesh, const std::array<double, 3> &center, const std::array<double, 3> &size)
{
  const uint32_t base = uint32_t(mesh.vertices.size());
  for (int z = -1; z <= 1; z += 2) for (int y = -1; y <= 1; y += 2) for (int x = -1; x <= 1; x += 2)
    mesh.vertices.push_back({center[0] + x * size[0] * 0.5, center[1] + y * size[1] * 0.5, center[2] + z * size[2] * 0.5});
  const uint32_t faces[][3] = {{0,2,3},{0,3,1},{4,5,7},{4,7,6},{0,1,5},{0,5,4},
                               {2,6,7},{2,7,3},{0,4,6},{0,6,2},{1,3,7},{1,7,5}};
  for (const auto &face : faces) append_triangle(mesh, base + face[0], base + face[1], base + face[2]);
}

void append_prism_segment(Mesh &mesh, const std::array<double, 3> &a, const std::array<double, 3> &b,
                          double width, double depth)
{
  const double dx=b[0]-a[0], dy=b[1]-a[1], length=std::hypot(dx,dy);
  if (length < 1.0e-9) return;
  const double nx=-dy/length*width*0.5, ny=dx/length*width*0.5, z0=-depth*0.5, z1=depth*0.5;
  const uint32_t i=uint32_t(mesh.vertices.size());
  mesh.vertices.push_back({a[0]+nx,a[1]+ny,z0}); mesh.vertices.push_back({a[0]-nx,a[1]-ny,z0});
  mesh.vertices.push_back({b[0]-nx,b[1]-ny,z0}); mesh.vertices.push_back({b[0]+nx,b[1]+ny,z0});
  mesh.vertices.push_back({a[0]+nx,a[1]+ny,z1}); mesh.vertices.push_back({a[0]-nx,a[1]-ny,z1});
  mesh.vertices.push_back({b[0]-nx,b[1]-ny,z1}); mesh.vertices.push_back({b[0]+nx,b[1]+ny,z1});
  const uint32_t f[][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
                         {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
  for(const auto &t:f) append_triangle(mesh,i+t[0],i+t[1],i+t[2]);
}

std::array<uint8_t,7> glyph_rows(char input)
{
  const char c = char(std::toupper(static_cast<unsigned char>(input)));
  static const std::array<std::array<uint8_t,7>,26> letters = {{
    {{14,17,17,31,17,17,17}},{{30,17,17,30,17,17,30}},{{14,17,16,16,16,17,14}},
    {{30,17,17,17,17,17,30}},{{31,16,16,30,16,16,31}},{{31,16,16,30,16,16,16}},
    {{14,17,16,23,17,17,15}},{{17,17,17,31,17,17,17}},{{14,4,4,4,4,4,14}},
    {{7,2,2,2,18,18,12}},{{17,18,20,24,20,18,17}},{{16,16,16,16,16,16,31}},
    {{17,27,21,21,17,17,17}},{{17,25,21,19,17,17,17}},{{14,17,17,17,17,17,14}},
    {{30,17,17,30,16,16,16}},{{14,17,17,17,21,18,13}},{{30,17,17,30,20,18,17}},
    {{15,16,16,14,1,1,30}},{{31,4,4,4,4,4,4}},{{17,17,17,17,17,17,14}},
    {{17,17,17,17,17,10,4}},{{17,17,17,21,21,21,10}},{{17,17,10,4,10,17,17}},
    {{17,17,10,4,4,4,4}},{{31,1,2,4,8,16,31}}
  }};
  if(c==' ') return {0,0,0,0,0,0,0};
  if(c>='A'&&c<='Z') return letters[size_t(c-'A')];
  throw dslvm::Error("label currently accepts ASCII letters and spaces only");
}

dslvm::Registry make_registry() {
  dslvm::Registry registry;
  registry.context("scene", "Scene");
  auto primitive = [](void *p, const std::string &kind, const double *v, size_t count, bool smooth) -> dslvm::Value {
    auto *scene = static_cast<Scene *>(p);
    if (scene->meshes.size() >= 4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    auto mesh = std::make_unique<Mesh>();
    mesh->smooth = smooth;
    auto add_tri = [&](uint32_t a, uint32_t b, uint32_t c) { mesh->triangles.push_back({a,b,c}); };
    if (kind == "plane") {
      const double s=v[0]*0.5;
      mesh->vertices={{{-s,-s,0},{s,-s,0},{s,s,0},{-s,s,0}}}; add_tri(0,1,2);add_tri(0,2,3);
    } else if (kind == "cube") {
      const double s=v[0]*0.5;
      mesh->vertices={{{-s,-s,-s},{s,-s,-s},{s,s,-s},{-s,s,-s},{-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s}}};
      const uint32_t f[][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
      for(const auto &t:f)add_tri(t[0],t[1],t[2]);
    } else {
      const int seg=int(v[kind=="uv_sphere"?1:(kind=="torus"?2:2)]);
      const int rings=int(v[kind=="uv_sphere"?2:(kind=="torus"?3:2)]);
      if(seg<8||seg>256||rings<4||rings>256) throw dslvm::Error("primitive segments/rings must be within 8..256 and 4..256");
      if(kind=="uv_sphere") {
        const double r=v[0];
        for(int y=0;y<=rings;++y){double phi=M_PI*y/rings;for(int x=0;x<seg;++x){double th=2*M_PI*x/seg;mesh->vertices.push_back({r*std::sin(phi)*std::cos(th),r*std::sin(phi)*std::sin(th),r*std::cos(phi)});}}
        for(int y=0;y<rings;++y)for(int x=0;x<seg;++x){uint32_t a=y*seg+x,b=y*seg+(x+1)%seg,c=(y+1)*seg+(x+1)%seg,d=(y+1)*seg+x;if(y>0)add_tri(a,d,b);if(y<rings-1)add_tri(b,d,c);}
      } else if(kind=="cylinder"||kind=="cone") {
        const double r=v[0],h=v[1];
        for(int z=0;z<2;++z)for(int x=0;x<seg;++x){double th=2*M_PI*x/seg;double rr=(kind=="cone"&&z==1)?0:r;mesh->vertices.push_back({rr*std::cos(th),rr*std::sin(th),z?h*0.5:-h*0.5});}
        const uint32_t bottom=2*seg,top=bottom+1;mesh->vertices.push_back({0,0,-h*0.5});mesh->vertices.push_back({0,0,h*0.5});
        for(int x=0;x<seg;++x){uint32_t n=(x+1)%seg;add_tri(x,n,seg+n);if(kind=="cylinder")add_tri(x,seg+n,seg+x);add_tri(bottom,n,x);if(kind=="cylinder")add_tri(top,seg+x,seg+n);}
      } else if(kind=="torus") {
        const double R=v[0],r=v[1];
        for(int y=0;y<rings;++y)for(int x=0;x<seg;++x){double a=2*M_PI*x/seg,b=2*M_PI*y/rings;double rr=R+r*std::cos(b);mesh->vertices.push_back({rr*std::cos(a),rr*std::sin(a),r*std::sin(b)});}
        for(int y=0;y<rings;++y)for(int x=0;x<seg;++x){uint32_t a=y*seg+x,b=y*seg+(x+1)%seg,c=((y+1)%rings)*seg+(x+1)%seg,d=((y+1)%rings)*seg+x;add_tri(a,b,c);add_tri(a,c,d);}
      } else throw dslvm::Error("unknown primitive kind");
    }
    if(mesh->vertices.size()>2'000'000||mesh->triangles.size()>4'000'000)throw dslvm::Error("primitive geometry budget exceeded");
    std::array<double,3> lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
    for(const auto &q:mesh->vertices)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],q[k]);hi[k]=std::max(hi[k],q[k]);}
    for(const auto &t:mesh->triangles)for(int k=0;k<3;++k){const auto &q=mesh->vertices[t[k]];mesh->uvs.push_back({float((q[0]-lo[0])/std::max(hi[0]-lo[0],1e-9)),float((q[1]-lo[1])/std::max(hi[1]-lo[1],1e-9))});}
    Mesh *result=mesh.get();adopt_mesh(*scene, std::move(mesh));return dslvm::Value::object("Mesh",result);
  };
  registry.method("Scene","plane",{"float"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||a[0].f<=0||a[0].f>10000)throw dslvm::Error("plane size must be in (0,10000]");return primitive(p,"plane",&a[0].f,1,false);});
  registry.method("Scene","cube",{"float"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||a[0].f<=0||a[0].f>10000)throw dslvm::Error("cube size must be in (0,10000]");return primitive(p,"cube",&a[0].f,1,false);});
  registry.method("Scene","uv_sphere",{"float","int","int","bool"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||a[0].f<=0||a[0].f>10000)throw dslvm::Error("sphere radius must be in (0,10000]");double v[]={a[0].f,double(a[1].i),double(a[2].i)};return primitive(p,"uv_sphere",v,3,a[3].b);});
  registry.method("Scene","cylinder",{"float","float","int","bool"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||!std::isfinite(a[1].f)||a[0].f<=0||a[1].f<=0||a[0].f>10000||a[1].f>10000)throw dslvm::Error("cylinder dimensions must be in (0,10000]");double v[]={a[0].f,a[1].f,double(a[2].i),double(a[2].i)};return primitive(p,"cylinder",v,4,a[3].b);});
  registry.method("Scene","cone",{"float","float","int","bool"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||!std::isfinite(a[1].f)||a[0].f<=0||a[1].f<=0||a[0].f>10000||a[1].f>10000)throw dslvm::Error("cone dimensions must be in (0,10000]");double v[]={a[0].f,a[1].f,double(a[2].i),double(a[2].i)};return primitive(p,"cone",v,4,a[3].b);});
  registry.method("Scene","torus",{"float","float","int","int","bool"},"Mesh",[primitive](void*p,dslvm::Args&a){if(!std::isfinite(a[0].f)||!std::isfinite(a[1].f)||a[0].f<=a[1].f||a[1].f<=0||a[0].f>10000)throw dslvm::Error("torus radii require major > minor > 0");double v[]={a[0].f,a[1].f,double(a[2].i),double(a[3].i)};return primitive(p,"torus",v,4,a[4].b);});
  registry.method("Scene","glow_orb",{"float","float","float","float","float","float","float","float"},"Mesh",[primitive](void*p,dslvm::Args&a){
    for(const auto &v:a)if(!std::isfinite(v.f))throw dslvm::Error("glow orb parameters must be finite");if(a[3].f<=0||a[3].f>10000||a[4].f<0||a[4].f>1||a[5].f<0||a[5].f>1||a[6].f<0||a[6].f>1||a[7].f<0||a[7].f>10000)throw dslvm::Error("glow orb radius/color/emission outside supported bounds");
    double v[]={a[3].f,32,16};auto result=primitive(p,"uv_sphere",v,3,true);auto *mesh=static_cast<Mesh*>(result.obj);mesh->translation={float(a[0].f),float(a[1].f),float(a[2].f)};
    auto *scene=static_cast<Scene*>(p);auto mat=std::make_unique<Material>();mat->color={float(a[4].f),float(a[5].f),float(a[6].f)};mat->emission=mat->color;mat->emission_strength=float(a[7].f);mat->roughness=0.3f;mesh->material=mat.get();scene->materials.push_back(std::move(mat));return result;
  });
  registry.method("Scene", "capsule", {"float","float","int","int"}, "Mesh", [](void *p,dslvm::Args&a) {
    auto *scene=static_cast<Scene*>(p);if(scene->meshes.size()>=4096)throw dslvm::Error("mesh object limit exceeded (4096)"); const double r=a[0].f, length=a[1].f; const int seg=int(a[2].i), hemi=int(a[3].i);
    if(!std::isfinite(r)||!std::isfinite(length)||r<=0||length<2*r||r>10000||length>10000||seg<8||seg>128||hemi<4||hemi>64) throw dslvm::Error("capsule requires radius>0, length>=2*radius, segments 8..128 and cap rings 4..64");
    auto m=std::make_unique<Mesh>(); m->smooth=true; const double half=length*0.5-r;
    std::vector<std::pair<double,double>> profile;
    for(int i=0;i<=hemi;++i){double t=-M_PI*0.5+(M_PI*0.5)*i/hemi;profile.push_back({-half+r*std::sin(t),r*std::cos(t)});}
    profile.push_back({half,r});
    for(int i=1;i<=hemi;++i){double t=(M_PI*0.5)*i/hemi;profile.push_back({half+r*std::sin(t),r*std::cos(t)});}
    for(const auto &ring:profile)for(int x=0;x<seg;++x){double t=2*M_PI*x/seg;m->vertices.push_back({ring.second*std::cos(t),ring.second*std::sin(t),ring.first});}
    for(size_t y=0;y+1<profile.size();++y)for(int x=0;x<seg;++x){uint32_t a0=uint32_t(y*seg+x),b=uint32_t(y*seg+(x+1)%seg),c=uint32_t((y+1)*seg+(x+1)%seg),d=uint32_t((y+1)*seg+x);append_triangle(*m,a0,b,c);append_triangle(*m,a0,c,d);}
    Mesh *result=m.get();adopt_mesh(*scene, std::move(m));return dslvm::Value::object("Mesh",result);
  });
  auto path_mesh = [](void *p,dslvm::Args&a,bool arrow) {
    auto *scene=static_cast<Scene*>(p); if(scene->meshes.size()>=4096)throw dslvm::Error("mesh object limit exceeded (4096)");
    for(const auto &v:a)if(!std::isfinite(v.f))throw dslvm::Error("path coordinates and radius must be finite");
    const double x0=a[0].f,y0=a[1].f,z0=a[2].f,x1=a[3].f,y1=a[4].f,z1=a[5].f,r=a[6].f;
    if(r<=0||r>1000||std::abs(x0)>1e6||std::abs(y0)>1e6||std::abs(z0)>1e6||std::abs(x1)>1e6||std::abs(y1)>1e6||std::abs(z1)>1e6)throw dslvm::Error("path radius or endpoint outside supported bounds");
    std::array<double,3>d{x1-x0,y1-y0,z1-z0};double len=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);if(len<1e-5)throw dslvm::Error("path endpoints must differ");for(auto &v:d)v/=len;
    std::array<double,3> ref=std::abs(d[2])<0.9?std::array<double,3>{0,0,1}:std::array<double,3>{0,1,0};
    std::array<double,3>u{d[1]*ref[2]-d[2]*ref[1],d[2]*ref[0]-d[0]*ref[2],d[0]*ref[1]-d[1]*ref[0]};double ul=std::sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);for(auto &v:u)v/=ul;
    std::array<double,3>v{d[1]*u[2]-d[2]*u[1],d[2]*u[0]-d[0]*u[2],d[0]*u[1]-d[1]*u[0]};
    auto m=std::make_unique<Mesh>();m->smooth=true;const int sides=16;const double head_len=arrow?std::min(len*0.35,r*5):0.0;const int rings=arrow?3:2;
    for(int ring=0;ring<rings;++ring){double t=arrow?(ring==0?0.0:ring==1?(len-head_len)/len:1.0):(ring?1.0:0.0);double rr=arrow?(ring==2?0.0:ring==1?r*2.8:r):r;
      for(int s=0;s<sides;++s){double q=2*M_PI*s/sides;double cs=std::cos(q),sn=std::sin(q);m->vertices.push_back({x0+d[0]*len*t+rr*(u[0]*cs+v[0]*sn),y0+d[1]*len*t+rr*(u[1]*cs+v[1]*sn),z0+d[2]*len*t+rr*(u[2]*cs+v[2]*sn)});}}
    for(int ring=0;ring+1<rings;++ring)for(int s=0;s<sides;++s){uint32_t a0=ring*sides+s,b=ring*sides+(s+1)%sides,c=(ring+1)*sides+(s+1)%sides,e=(ring+1)*sides+s;append_triangle(*m,a0,b,c);append_triangle(*m,a0,c,e);}
    Mesh *result=m.get();adopt_mesh(*scene, std::move(m));return dslvm::Value::object("Mesh",result);
  };
  registry.method("Scene","tube_between",{"float","float","float","float","float","float","float"},"Mesh",[path_mesh](void*p,dslvm::Args&a){return path_mesh(p,a,false);});
  registry.method("Scene","arrow_between",{"float","float","float","float","float","float","float"},"Mesh",[path_mesh](void*p,dslvm::Args&a){return path_mesh(p,a,true);});
  registry.method("Scene", "strand", {"string", "float", "float", "int"}, "Mesh", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if (scene->meshes.size()>=4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    const auto path=parse_points(a[0].s,3,2,512);
    const double root=a[1].f, tip=a[2].f;
    const int sides=int(a[3].i);
    if (!std::isfinite(root)||!std::isfinite(tip)||root<=0||root>1000||tip<0||tip>root||sides<3||sides>32)
      throw dslvm::Error("strand requires root radius (0,1000], tip radius [0,root], and sides 3..32");
    std::vector<std::array<double,3>> profile;
    for (int j=0;j<sides;++j) { const double angle=2*M_PI*j/sides; profile.push_back({std::cos(angle),std::sin(angle),0}); }
    auto mesh=std::make_unique<Mesh>(); mesh->smooth=true;
    append_sweep(*mesh,path,profile,root,tip,true);
    Mesh *result=adopt_mesh(*scene,std::move(mesh));
    return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene", "hair_sphere", {"float","float","float","float","int","float","float"}, "Mesh", [](void *p,dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if (scene->meshes.size()>=4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    for (size_t i=0;i<7;++i) if (i!=4 && !std::isfinite(a[i].f)) throw dslvm::Error("hair sphere parameters must be finite");
    const double x=a[0].f,y=a[1].f,z=a[2].f,r=a[3].f,length=a[5].f,width=a[6].f;
    const int count=int(a[4].i);
    if (std::abs(x)>1e6||std::abs(y)>1e6||std::abs(z)>1e6||r<=0||r>1000||count<1||count>2048||length<=0||length>100||width<=0||width>r)
      throw dslvm::Error("hair_sphere requires bounded center, radius (0,1000], strands 1..2048, length (0,100], width (0,radius]");
    std::vector<std::array<double,3>> profile;
    for (int j=0;j<8;++j) { const double angle=2*M_PI*j/8;profile.push_back({std::cos(angle),std::sin(angle),0}); }
    auto mesh=std::make_unique<Mesh>(); mesh->smooth=true;
    for (int i=0;i<count;++i) {
      const double h=1.0-2.0*(i+0.5)/count, t=i*2.399963229728653,
                   radial=std::sqrt(std::max(0.0,1.0-h*h));
      const std::array<double,3> dir{radial*std::cos(t),radial*std::sin(t),h};
      const auto side=unit3(cross3(dir, std::abs(dir[2])<0.95 ? std::array<double,3>{0,0,1} : std::array<double,3>{0,1,0}));
      std::vector<std::array<double,3>> path;
      for (double t : {0.0,0.35,0.72,1.0}) {
        const double d=r+length*t, bend=length*0.72*t*t;
        path.push_back({x+d*dir[0]+bend*side[0],y+d*dir[1]+bend*side[1],
                        z+d*dir[2]+bend*side[2]-length*0.28*t*t});
      }
      append_sweep(*mesh,path,profile,width,width*0.08,true);
    }
    Mesh *result=adopt_mesh(*scene,std::move(mesh));
    return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene", "hair_patch", {"float","float","float","float","int","float","float"}, "Mesh", [](void *p,dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if (scene->meshes.size()>=4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    for (size_t i=0;i<7;++i) if (i!=4 && !std::isfinite(a[i].f)) throw dslvm::Error("hair patch parameters must be finite");
    const double x=a[0].f,y=a[1].f,z=a[2].f,size=a[3].f,height=a[5].f,width=a[6].f;
    const int count=int(a[4].i);
    if (std::abs(x)>1e6||std::abs(y)>1e6||std::abs(z)>1e6||size<=0||size>1000||count<1||count>2048||height<=0||height>100||width<=0||width>height*0.2)
      throw dslvm::Error("hair_patch requires bounded center, size (0,1000], strands 1..2048, height (0,100], width (0,height/5]");
    std::vector<std::array<double,3>> profile;
    for (int j=0;j<6;++j) { const double angle=2*M_PI*j/6;profile.push_back({std::cos(angle),std::sin(angle),0}); }
    auto mesh=std::make_unique<Mesh>();mesh->smooth=true;
    for (int i=0;i<count;++i) {
      const double u=std::fmod((i+0.5)*0.7548776662466927,1.0),
                   v=std::fmod((i+0.5)*0.5698402909980532,1.0),
                   px=x+(u-0.5)*size,py=y+(v-0.5)*size,
                   h=height*(0.72+0.28*std::fmod((i+1)*0.618033988749895,1.0)),
                   lean=0.22*h;
      const std::vector<std::array<double,3>> path{{px,py,z},{px+0.02*h,py,z+0.4*h},
                                                   {px+0.11*h,py+0.04*h,z+0.76*h},
                                                   {px+lean,py+0.08*h,z+h}};
      append_sweep(*mesh,path,profile,width,width*0.07,true);
    }
    Mesh *result=adopt_mesh(*scene,std::move(mesh));
    return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene", "loft", {"string","string","bool"}, "Mesh", [](void *p,dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if (scene->meshes.size()>=4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    const auto path=parse_points(a[0].s,3,2,512);
    auto profile=parse_points(a[1].s,2,3,64);
    double area=0.0;
    for (size_t i=0;i<profile.size();++i) {
      const auto &v=profile[i], &w=profile[(i+1)%profile.size()];
      area+=v[0]*w[1]-w[0]*v[1];
    }
    if (!std::isfinite(area)||std::abs(area)<1e-8) throw dslvm::Error("loft profile must enclose a nonzero area");
    if (area<0) std::reverse(profile.begin(),profile.end());
    for (size_t i=0;i<profile.size();++i) {
      const auto &a0=profile[i],&b=profile[(i+1)%profile.size()],&c=profile[(i+2)%profile.size()];
      if ((b[0]-a0[0])*(c[1]-b[1])-(b[1]-a0[1])*(c[0]-b[0])<=1e-9)
        throw dslvm::Error("loft profile must be a strictly convex polygon without duplicate corners");
    }
    auto mesh=std::make_unique<Mesh>();mesh->smooth=false;
    append_sweep(*mesh,path,profile,1.0,1.0,a[2].b);
    Mesh *result=adopt_mesh(*scene,std::move(mesh));
    return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene", "toon_outline", {"Mesh","float","float","float","float"}, "Mesh", [](void *p,dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    auto *object=static_cast<Mesh *>(a[0].obj);
    if (scene->meshes.size()>=4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    if (std::none_of(scene->meshes.begin(),scene->meshes.end(),[object](const auto &mesh){return mesh.get()==object;}))
      throw dslvm::Error("toon_outline source must belong to the scene");
    if (!std::isfinite(a[1].f)||a[1].f<=0||a[1].f>100)
      throw dslvm::Error("toon outline thickness must be in (0,100] local scene units");
    for (size_t i=2;i<5;++i) if (!std::isfinite(a[i].f)||a[i].f<0||a[i].f>1)
      throw dslvm::Error("toon outline color channels must be in [0,1]");
    const Mesh *source=object->geometry_source?object->geometry_source:object;
    if (source->vertices.empty()||source->triangles.empty()) throw dslvm::Error("toon outline source has no surface triangles");
    if (source->triangles.size()>10'000) throw dslvm::Error("toon outline supports at most 10000 source triangles");
    std::map<std::pair<uint32_t,uint32_t>,std::vector<std::array<double,3>>> edges;
    for (const auto &t:source->triangles) {
      const auto &v0=source->vertices[t[0]],&v1=source->vertices[t[1]],&v2=source->vertices[t[2]];
      const std::array<double,3> e1{v1[0]-v0[0],v1[1]-v0[1],v1[2]-v0[2]},
                                 e2{v2[0]-v0[0],v2[1]-v0[1],v2[2]-v0[2]};
      auto n=cross3(e1,e2);
      const double len=std::hypot(n[0],n[1],n[2]);
      if (len<1e-12) continue;
      for (double &value:n) value/=len;
      for (int j=0;j<3;++j) {
        const uint32_t x=t[j],y=t[(j+1)%3];
        edges[{std::min(x,y),std::max(x,y)}].push_back(n);
      }
    }
    auto strokes=std::make_unique<Mesh>();
    std::vector<std::array<double,3>> circle;
    for (int j=0;j<8;++j) { const double t=2*M_PI*j/8;circle.push_back({std::cos(t),std::sin(t),0}); }
    for (const auto &[edge,adjacent]:edges) {
      const bool sharp=adjacent.size()!=2 ||
        adjacent[0][0]*adjacent[1][0]+adjacent[0][1]*adjacent[1][1]+adjacent[0][2]*adjacent[1][2]<0.8660254;
      if (!sharp) continue;
      const auto &start=source->vertices[edge.first],&end=source->vertices[edge.second];
      if (std::hypot(start[0]-end[0],start[1]-end[1],start[2]-end[2])<1e-8) continue;
      append_sweep(*strokes,{start,end},circle,a[1].f,a[1].f,true);
    }
    if (strokes->triangles.empty()) throw dslvm::Error("toon outline found no boundary or sharp feature edges");
    strokes->translation=object->translation;strokes->scale=object->scale;strokes->smooth=true;
    auto ink=std::make_unique<Material>();
    ink->color={float(a[2].f),float(a[3].f),float(a[4].f)};
    ink->roughness=1.0f;
    strokes->material=ink.get();
    Mesh *result=adopt_mesh(*scene,std::move(strokes));
    scene->materials.push_back(std::move(ink));
    return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene","ring_arc",{"float","float","float","float","float","float","float","int"},"Mesh",[](void*p,dslvm::Args&a){
    auto *scene=static_cast<Scene*>(p);if(scene->meshes.size()>=4096)throw dslvm::Error("mesh object limit exceeded (4096)");for(size_t i=0;i<7;++i)if(!std::isfinite(a[i].f))throw dslvm::Error("ring arc values must be finite");
    const double R=a[3].f,r=a[4].f,start=a[5].f*M_PI/180.0,end=a[6].f*M_PI/180.0;const int seg=int(a[7].i),sides=12;
    if(R<=r||r<=0||R>10000||std::abs(a[0].f)>1e6||std::abs(a[1].f)>1e6||std::abs(a[2].f)>1e6||end<=start||end-start>2*M_PI+1e-6||seg<8||seg>512)throw dslvm::Error("ring arc requires major>minor>0, ordered angle span <=360 degrees and 8..512 segments");
    auto m=std::make_unique<Mesh>();m->smooth=true;
    for(int x=0;x<=seg;++x){double th=start+(end-start)*x/seg;for(int y=0;y<sides;++y){double ph=2*M_PI*y/sides,rr=R+r*std::cos(ph);m->vertices.push_back({a[0].f+rr*std::cos(th),a[1].f+rr*std::sin(th),a[2].f+r*std::sin(ph)});}}
    for(int x=0;x<seg;++x)for(int y=0;y<sides;++y){uint32_t q=x*sides+y,q1=x*sides+(y+1)%sides,q2=(x+1)*sides+(y+1)%sides,q3=(x+1)*sides+y;append_triangle(*m,q,q1,q2);append_triangle(*m,q,q2,q3);}
    Mesh *result=m.get();adopt_mesh(*scene, std::move(m));return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene","checkmark",{"float","float","float","float","float"},"Mesh",[](void*p,dslvm::Args&a){
    auto *scene=static_cast<Scene*>(p);if(scene->meshes.size()>=4096)throw dslvm::Error("mesh object limit exceeded (4096)");for(const auto &v:a)if(!std::isfinite(v.f))throw dslvm::Error("check mark values must be finite");const double x=a[0].f,y=a[1].f,z=a[2].f,s=a[3].f,w=a[4].f;
    if(s<=0||s>10000||w<=0||w>s*0.4)throw dslvm::Error("check mark size must be positive and stroke width <=40% of size");
    auto m=std::make_unique<Mesh>();append_prism_segment(*m,{x-s*0.42,y+s*0.02,0},{x-s*0.12,y-s*0.28,0},w,s*0.12);append_prism_segment(*m,{x-s*0.12,y-s*0.28,0},{x+s*0.45,y+s*0.34,0},w,s*0.12);for(auto &vertex:m->vertices)vertex[2]+=z;
    Mesh *result=m.get();adopt_mesh(*scene, std::move(m));return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene","label",{"string","float","float","float","float"},"Mesh",[](void*p,dslvm::Args&a){
    auto *scene=static_cast<Scene*>(p);if(scene->meshes.size()>=4096)throw dslvm::Error("mesh object limit exceeded (4096)");if(a[0].s.empty()||a[0].s.size()>64)throw dslvm::Error("label text length must be 1..64 ASCII letters/spaces");
    for(size_t i=1;i<5;++i)if(!std::isfinite(a[i].f))throw dslvm::Error("label position and size must be finite");const double x=a[1].f,y=a[2].f,z=a[3].f,u=a[4].f;
    if(u<=0||u>1000||std::abs(x)>1e6||std::abs(y)>1e6||std::abs(z)>1e6)throw dslvm::Error("label size or position outside supported bounds");
    auto m=std::make_unique<Mesh>();const double thick=u*0.14,step=u/6.0;
    for(size_t ci=0;ci<a[0].s.size();++ci){auto rows=glyph_rows(a[0].s[ci]);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col)))append_box(*m,{x+(ci*6+col)*step,y+(6-row)*step,z},{step*0.82,step*0.82,thick});}
    if(m->vertices.size()>100000)throw dslvm::Error("label geometry exceeds 100,000 vertices");Mesh *result=m.get();adopt_mesh(*scene, std::move(m));return dslvm::Value::object("Mesh",result);
  });
  registry.method("Scene", "camera_look_at", {"float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &value : a) if (!std::isfinite(value.f)) throw dslvm::Error("camera values must be finite");
    for (size_t i = 0; i < 6; ++i)
      if (std::abs(a[i].f) > 1'000'000.0) throw dslvm::Error("camera position or target exceeds scene bounds");
    const double dx = a[3].f - a[0].f, dy = a[4].f - a[1].f, dz = a[5].f - a[2].f;
    if (dx * dx + dy * dy + dz * dz < 1.0e-12) throw dslvm::Error("camera position and target must differ");
    if (a[6].f < 5.0 || a[6].f > 170.0) throw dslvm::Error("camera field of view must be 5..170 degrees");
    scene->camera.position = {float(a[0].f), float(a[1].f), float(a[2].f)};
    scene->camera.target = {float(a[3].f), float(a[4].f), float(a[5].f)};
    scene->camera.fov_radians = float(a[6].f * M_PI / 180.0);
    scene->camera.orthographic = false;
    scene->camera.fit_to_geometry = false;
    scene->custom_camera = true;
    return dslvm::Value::none();
  });
  registry.method("Scene", "camera_keyframe", {"int", "float", "float", "float", "float", "float", "float", "string"}, "void", [](void *p,dslvm::Args &a){
    auto *scene=static_cast<Scene*>(p);if(a[0].i<0||a[0].i>1'000'000)throw dslvm::Error("camera keyframe must be in [0,1000000]");if(a[7].s!="linear"&&a[7].s!="ease")throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    std::array<float,6> value{};for(size_t i=0;i<6;++i){if(!std::isfinite(a[i+1].f)||std::abs(a[i+1].f)>1'000'000)throw dslvm::Error("camera keyframe coordinates must be finite and bounded");value[i]=float(a[i+1].f);}double dx=value[3]-value[0],dy=value[4]-value[1],dz=value[5]-value[2];if(dx*dx+dy*dy+dz*dz<1e-12)throw dslvm::Error("camera keyframe position and target must differ");
    auto &keys=scene->camera_keys;auto it=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.frame==a[0].i;});Scene::CameraKey key{int(a[0].i),value,a[7].s};if(it==keys.end())keys.push_back(key);else *it=key;std::sort(keys.begin(),keys.end(),[](const auto&x,const auto&y){return x.frame<y.frame;});scene->custom_camera=true;scene->camera.fit_to_geometry=false;return dslvm::Value::none();
  });
  registry.method("Scene", "camera_optics_keyframe", {"int", "float", "float", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (a[0].i < 0 || a[0].i > 1'000'000) throw dslvm::Error("camera optics keyframe must be in [0,1000000]");
    if (a[3].s != "linear" && a[3].s != "ease") throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    if (!std::isfinite(a[1].f) || a[1].f < 5 || a[1].f > 170 || !std::isfinite(a[2].f) || a[2].f < 0.01 || a[2].f > 1'000'000)
      throw dslvm::Error("camera optics keyframe requires vertical FOV 5..170 degrees and orthographic vertical span 0.01..1000000");
    auto &keys = scene->camera_optics_keys;
    Scene::CameraOpticsKey key{int(a[0].i), {float(a[1].f * M_PI / 180.0), float(a[2].f)}, a[3].s};
    auto it = std::find_if(keys.begin(), keys.end(), [&](const auto &entry) { return entry.frame == a[0].i; });
    if (it == keys.end()) keys.push_back(key); else *it = key;
    std::sort(keys.begin(), keys.end(), [](const auto &x, const auto &y) { return x.frame < y.frame; });
    scene->camera.fit_to_geometry = false;
    return dslvm::Value::none();
  });
  registry.method("Scene", "camera_dof", {"float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (!std::isfinite(a[0].f) || !std::isfinite(a[1].f) || a[0].f < 0.0 || a[0].f > 10.0 || a[1].f < 0.1 || a[1].f > 1'000'000.0)
      throw dslvm::Error("camera DOF aperture must be 0..10 and focus distance 0.1..1000000");
    scene->camera_aperture = float(a[0].f);
    scene->camera_focus_distance = float(a[1].f);
    return dslvm::Value::none();
  });
  registry.method("Scene", "motion_blur", {"bool", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (!std::isfinite(a[1].f) || a[1].f < 0.1 || a[1].f > 2.0)
      throw dslvm::Error("motion shutter must be within [0.1,2] frames");
    scene->motion_blur = a[0].b;
    scene->motion_shutter = float(a[1].f);
    return dslvm::Value::none();
  });
  registry.method("Scene", "light_clamp", {"float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    for(const auto &v:a) if(!std::isfinite(v.f)||v.f<0||v.f>10000) throw dslvm::Error("light clamp values must be finite and within [0,10000] (0 disables)");
    scene->clamp_direct=float(a[0].f);scene->clamp_indirect=float(a[1].f);return dslvm::Value::none();
  });
  registry.method("Scene", "camera_orthographic", {"float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("orthographic camera values must be finite");
    for (size_t i = 0; i < 6; ++i) if (std::abs(a[i].f) > 1'000'000) throw dslvm::Error("orthographic camera position or target exceeds scene bounds");
    const double dx=a[3].f-a[0].f, dy=a[4].f-a[1].f, dz=a[5].f-a[2].f;
    if (dx*dx+dy*dy+dz*dz < 1.0e-12) throw dslvm::Error("orthographic camera position and target must differ");
    if (a[6].f < 0.1 || a[6].f > 100000.0) throw dslvm::Error("orthographic camera width must be in [0.1,100000]");
    scene->camera.position={float(a[0].f),float(a[1].f),float(a[2].f)};
    scene->camera.target={float(a[3].f),float(a[4].f),float(a[5].f)};
    scene->camera.orthographic=true; scene->camera.ortho_scale=float(a[6].f); scene->camera.fit_to_geometry=false; scene->custom_camera=true;
    return dslvm::Value::none();
  });
  registry.method("Scene", "fit_camera", {"float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    const double fov = a[0].f;
    if (!std::isfinite(fov) || fov < 5.0 || fov > 150.0) throw dslvm::Error("fit_camera field of view must be in [5,150] degrees");
    std::array<double, 3> lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
    bool any = false;
    for (const auto &mesh : scene->meshes) if (!mesh->ground_plane) {
      const Mesh *data = mesh->geometry_source ? mesh->geometry_source : mesh.get();
      for (const auto &v : data->vertices) {
        any = true;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double position = v[axis] * mesh->scale[axis] + mesh->translation[axis];
          lo[axis] = std::min(lo[axis], position); hi[axis] = std::max(hi[axis], position);
        }
      }
    }
    if (!any) throw dslvm::Error("fit_camera requires imported geometry");
    const float cx = float((lo[0] + hi[0]) * 0.5), cy = float((lo[1] + hi[1]) * 0.5), cz = float((lo[2] + hi[2]) * 0.5);
    const float radius = std::max(0.1f, float(std::sqrt((hi[0]-lo[0])*(hi[0]-lo[0]) + (hi[1]-lo[1])*(hi[1]-lo[1]) + (hi[2]-lo[2])*(hi[2]-lo[2])) * 0.5));
    scene->camera.fit_to_geometry = true;
    scene->camera.target = {cx, cy, cz};
    scene->camera.fov_radians = float(fov * M_PI / 180.0);
    scene->custom_camera = true;
    return dslvm::Value::none();
  });
  registry.method("Scene", "studio_setup", {}, "void", [](void *p, dslvm::Args &) {
    auto *scene = static_cast<Scene *>(p);
    std::array<double, 3> lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
    bool any = false;
    for (const auto &mesh : scene->meshes) if (!mesh->ground_plane) {
      const Mesh *data = mesh->geometry_source ? mesh->geometry_source : mesh.get();
      for (const auto &v : data->vertices) {
        any = true;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double q = v[axis] * mesh->scale[axis] + mesh->translation[axis];
          lo[axis] = std::min(lo[axis], q); hi[axis] = std::max(hi[axis], q);
        }
      }
    }
    if (!any) throw dslvm::Error("studio_setup requires at least one imported or created mesh");
    const float cx=float((lo[0]+hi[0])*0.5), cy=float((lo[1]+hi[1])*0.5), cz=float((lo[2]+hi[2])*0.5);
    const float radius=std::max(0.5f,float(std::sqrt((hi[0]-lo[0])*(hi[0]-lo[0])+(hi[1]-lo[1])*(hi[1]-lo[1])+(hi[2]-lo[2])*(hi[2]-lo[2]))*0.5));
    scene->world_color={0.34f,0.37f,0.42f}; scene->world_strength=0.55f;
    scene->camera.fit_to_geometry=true; scene->camera.target={cx,cy,cz}; scene->camera.fov_radians=float(42.0*M_PI/180.0); scene->custom_camera=true;
    auto add_area=[&](float x,float y,float z,std::array<float,3> color,float power,float size) {
      AreaLightData light; light.position={cx+x*radius,cy+y*radius,cz+z*radius}; light.target={cx,cy,cz}; light.color=color; light.strength=power*radius*radius; light.size=radius*size; light.size_y=radius*size; scene->area_lights.push_back(light);
    };
    add_area(-1.8f,-2.2f,2.2f,{1.0f,0.86f,0.72f},115.0f,1.1f);
    add_area(2.0f,-0.8f,1.1f,{0.68f,0.80f,1.0f},52.0f,1.5f);
    add_area(0.1f,1.8f,1.8f,{1.0f,1.0f,1.0f},78.0f,0.9f);
    auto floor=std::make_unique<Material>(); floor->color={0.32f,0.34f,0.37f}; floor->roughness=0.86f; Material *mat=floor.get(); scene->materials.push_back(std::move(floor));
    auto ground=std::make_unique<Mesh>(); const float z=float(lo[2])-std::max(0.01f,radius*0.004f), side=std::max(10.0f,radius*10.0f);
    ground->vertices={{{cx-side,cy-side,z},{cx+side,cy-side,z},{cx+side,cy+side,z},{cx-side,cy+side,z}}}; ground->triangles={{{0,1,2},{0,2,3}}}; ground->material=mat; ground->ground_plane=true; adopt_mesh(*scene, std::move(ground));
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_timeout", {"int"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].i < 1 || a[0].i > 3600) throw dslvm::Error("render timeout must be in [1,3600] seconds");
    static_cast<Scene *>(p)->render_timeout_seconds = int(a[0].i);
    return dslvm::Value::none();
  });
  registry.method("Scene", "set_threads", {"int"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].i < 1 || a[0].i > 64) throw dslvm::Error("render thread count must be in [1,64]");
    static_cast<Scene *>(p)->render_threads = int(a[0].i);
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_settings", {"int", "int", "bool"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].i < 0 || a[0].i > 64) throw dslvm::Error("max bounces must be in [0,64]");
    if (a[1].i < 0 || a[1].i > 2'147'483'647) throw dslvm::Error("render seed must be in [0,2147483647]");
    if (a[2].b && !ccl::openimagedenoise_supported())
      throw dslvm::Error("denoising is unavailable: Cycles was built without CPU OpenImageDenoise");
    auto *scene = static_cast<Scene *>(p);
    scene->max_bounces = int(a[0].i);
    scene->render_seed = int(a[1].i);
    scene->denoise = a[2].b;
    return dslvm::Value::none();
  });
  registry.method("Scene", "denoise_quality", {"string"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (a[0].s == "fast") scene->denoise_quality = ccl::DENOISER_QUALITY_FAST;
    else if (a[0].s == "balanced") scene->denoise_quality = ccl::DENOISER_QUALITY_BALANCED;
    else if (a[0].s == "high") scene->denoise_quality = ccl::DENOISER_QUALITY_HIGH;
    else throw dslvm::Error("denoise_quality must be fast, balanced, or high");
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_preset", {"string"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (a[0].s == "draft") {
      scene->preset_samples = 8;
      scene->preset_bounces = 2;
      scene->denoise = false;
    } else if (a[0].s == "final") {
      if (!ccl::openimagedenoise_supported())
        throw dslvm::Error("final render preset requires CPU OpenImageDenoise support");
      scene->preset_samples = 64;
      scene->preset_bounces = 8;
      scene->denoise = true;
    } else {
      throw dslvm::Error("render preset must be 'draft' or 'final'");
    }
    scene->max_bounces = scene->preset_bounces;
    return dslvm::Value::none();
  });
  registry.method("Scene", "use_preset", {"string"}, "void", [](void *p, dslvm::Args &a) {
    apply_preset(*static_cast<Scene *>(p), find_builtin_preset(a[0].s));
    return dslvm::Value::none();
  });
  registry.method("Scene", "load_preset", {"string"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].s.empty() || a[0].s.front() == '-') throw dslvm::Error("PRESET_INVALID", "preset file path must not be empty or begin with '-'");
    std::error_code ec;
    if (!std::filesystem::is_regular_file(a[0].s, ec) || ec) throw dslvm::Error("PRESET_INVALID", "preset file must be a readable regular file");
    const uintmax_t bytes = std::filesystem::file_size(a[0].s, ec);
    if (ec || bytes == 0 || bytes > 128u * 1024u) throw dslvm::Error("PRESET_INVALID", "preset JSON file must be between 1 byte and 128 KiB");
    std::ifstream input(a[0].s, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    try {
      PresetJson external = PresetJson::parse(data);
      if (external.is_object() && external.contains("version")) {
        if (external.at("version").get<int>() != 1 || !external.contains("preset"))
          throw dslvm::Error("PRESET_INVALID", "external preset envelope must have version 1 and a preset object");
        external = external.at("preset");
      }
      apply_preset(*static_cast<Scene *>(p), external);
    }
    catch (const dslvm::Error &) { throw; }
    catch (const std::exception &error) { throw dslvm::Error("PRESET_INVALID", std::string("could not parse preset JSON: ") + error.what()); }
    return dslvm::Value::none();
  });
  registry.method("Scene", "preset_param", {"string", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->active_preset.empty()) throw dslvm::Error("PRESET_INVALID", "call scene.use_preset or scene.load_preset before preset_param");
    if (!std::isfinite(a[1].f)) throw dslvm::Error("PRESET_INVALID", "preset parameter value must be finite");
    const std::string &name = a[0].s;
    if (name == "samples") {
      if (a[1].f < 1 || a[1].f > 65536 || std::floor(a[1].f) != a[1].f) throw dslvm::Error("PRESET_INVALID", "preset samples must be an integer in [1,65536]");
      scene->preset_samples = int(a[1].f);
    } else if (name == "bounces") {
      if (a[1].f < 0 || a[1].f > 64 || std::floor(a[1].f) != a[1].f) throw dslvm::Error("PRESET_INVALID", "preset bounces must be an integer in [0,64]");
      scene->preset_bounces = scene->max_bounces = int(a[1].f);
    } else if (name == "threads") {
      if (a[1].f < 1 || a[1].f > 64 || std::floor(a[1].f) != a[1].f) throw dslvm::Error("PRESET_INVALID", "preset threads must be an integer in [1,64]");
      scene->render_threads = int(a[1].f);
    } else if (name == "world_strength") {
      if (a[1].f < 0 || a[1].f > 100) throw dslvm::Error("PRESET_INVALID", "world_strength must be in [0,100]");
      scene->world_strength = float(a[1].f);
    } else if (name == "camera_fov_degrees") {
      if (a[1].f < 5 || a[1].f > 170) throw dslvm::Error("PRESET_INVALID", "camera_fov_degrees must be in [5,170]");
      scene->camera.fov_radians = float(a[1].f * M_PI / 180.0);
    } else if (name == "orthographic_vertical_span") {
      if (a[1].f <= 0 || a[1].f > 1'000'000) throw dslvm::Error("PRESET_INVALID", "orthographic_vertical_span must be within (0,1000000]");
      scene->camera.ortho_scale = float(a[1].f);
    } else {
      throw dslvm::Error("PRESET_INVALID", "unsupported numeric preset parameter '" + name + "'; supported: samples, bounces, threads, world_strength, camera_fov_degrees, orthographic_vertical_span");
    }
    return dslvm::Value::none();
  });
  registry.method("Scene", "material_from_palette", {"string"}, "Material", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (!scene->palette.is_object() || !scene->palette.contains(a[0].s)) throw dslvm::Error("PRESET_INVALID", "palette entry '" + a[0].s + "' is unavailable; choose one from the active preset");
    const auto &entry = scene->palette.at(a[0].s);
    auto material = std::make_unique<Material>();
    const auto color = entry.value("color", std::vector<float>{0.8f,0.8f,0.8f});
    const auto emission = entry.value("emission", std::vector<float>{0.0f,0.0f,0.0f});
    if (color.size() != 3 || emission.size() != 3) throw dslvm::Error("PRESET_INVALID", "palette color and emission need three channels");
    for (size_t i=0;i<3;++i) {
      if (!std::isfinite(color[i]) || color[i] < 0 || color[i] > 1 || !std::isfinite(emission[i]) || emission[i] < 0 || emission[i] > 1)
        throw dslvm::Error("PRESET_INVALID", "palette channels must be in [0,1]");
      material->color[i]=color[i]; material->emission[i]=emission[i];
    }
    material->metallic=entry.value("metallic",0.0f); material->roughness=entry.value("roughness",0.5f);
    material->transmission=entry.value("transmission",0.0f); material->emission_strength=entry.value("emission_strength",0.0f);
    Material *result=material.get(); scene->materials.push_back(std::move(material));
    return dslvm::Value::object("Material", result);
  });
  registry.method("Scene", "set_view_transform", {"string"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].s != "Standard" && a[0].s != "Filmic" && a[0].s != "AgX")
      throw dslvm::Error("view transform must be Standard, Filmic, or AgX");
    static_cast<Scene *>(p)->view_transform = a[0].s;
    return dslvm::Value::none();
  });
  registry.method("Scene", "world_color", {"float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &value : a) if (!std::isfinite(value.f)) throw dslvm::Error("world color values must be finite");
    for (size_t i = 0; i < 3; ++i)
      if (a[i].f < 0.0 || a[i].f > 1.0) throw dslvm::Error("world color channels must be in [0,1]");
    if (a[3].f < 0.0 || a[3].f > 10000.0) throw dslvm::Error("world strength must be in [0,10000]");
    scene->world_color = {float(a[0].f), float(a[1].f), float(a[2].f)};
    scene->world_strength = float(a[3].f);
    return dslvm::Value::none();
  });
  registry.method("Scene", "sky_texture", {"float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("sky parameters must be finite");
    if (a[0].f < 0.0 || a[0].f > 90.0 || std::abs(a[1].f) > 360.0 || a[2].f < 0.0 || a[2].f > 100.0)
      throw dslvm::Error("sky elevation must be 0..90 degrees, rotation +/-360 degrees, intensity 0..100");
    if (!scene->environment_image.empty()) throw dslvm::Error("sky_texture and environment_texture are mutually exclusive");
    scene->sky_enabled = true;
    scene->sky_elevation = float(a[0].f * M_PI / 180.0);
    scene->sky_rotation = float(a[1].f * M_PI / 180.0);
    scene->sky_intensity = float(a[2].f);
    return dslvm::Value::none();
  });
  registry.method("Scene", "environment_texture", {"string", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->sky_enabled) throw dslvm::Error("environment_texture and sky_texture are mutually exclusive");
    if (!std::isfinite(a[1].f) || std::abs(a[1].f) > 360.0 || !std::isfinite(a[2].f) || a[2].f < 0 || a[2].f > 100.0)
      throw dslvm::Error("environment rotation must be +/-360 degrees and strength 0..100");
    scene->environment_image = checked_image_path(a[0].s, "environment texture", {".png", ".exr", ".hdr"}, 64u * 1024u * 1024u);
    scene->environment_rotation = float(a[1].f * M_PI / 180.0);
    scene->environment_strength = float(a[2].f);
    return dslvm::Value::none();
  });
  registry.method("Scene", "transparent_background", {"bool"}, "void", [](void *p, dslvm::Args &a) {
    static_cast<Scene *>(p)->transparent_background = a[0].b;
    return dslvm::Value::none();
  });
  registry.method("Scene", "point_light", {"float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->point_lights.size() >= 1024) throw dslvm::Error("point light limit exceeded (1024)");
    for (const auto &value : a) if (!std::isfinite(value.f)) throw dslvm::Error("point light values must be finite");
    for (size_t i = 0; i < 3; ++i) {
      if (std::abs(a[i].f) > 1'000'000.0) throw dslvm::Error("point light position exceeds scene bounds");
      if (a[i + 3].f < 0.0 || a[i + 3].f > 1.0) throw dslvm::Error("point light color channels must be in [0,1]");
    }
    if (a[6].f < 0.0 || a[6].f > 1'000'000.0) throw dslvm::Error("point light strength must be in [0,1000000]");
    PointLightData light;
    light.position = {float(a[0].f), float(a[1].f), float(a[2].f)};
    light.color = {float(a[3].f), float(a[4].f), float(a[5].f)};
    light.strength = float(a[6].f);
    remove_preset_area_rig(*scene);
    scene->point_lights.push_back(light);
    return dslvm::Value::none();
  });
  registry.method("Scene", "point_light_keyframe", {"int", "int", "float", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (a[0].i < 0 || size_t(a[0].i) >= scene->point_lights.size()) throw dslvm::Error("point_light_keyframe index is outside the existing point lights");
    if (a[1].i < 0 || a[1].i > 1'000'000) throw dslvm::Error("light keyframe must be in [0,1000000]");
    if (!std::isfinite(a[2].f) || a[2].f < 0 || a[2].f > 1'000'000) throw dslvm::Error("keyframed point light strength must be in [0,1000000]");
    if (a[3].s != "linear" && a[3].s != "ease") throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    auto &keys = scene->point_lights[size_t(a[0].i)].strength_keys;
    PointLightData::StrengthKey key{int(a[1].i), float(a[2].f), a[3].s};
    auto it = std::find_if(keys.begin(), keys.end(), [&](const auto &entry) { return entry.frame == a[1].i; });
    if (it == keys.end()) keys.push_back(key); else *it = key;
    std::sort(keys.begin(), keys.end(), [](const auto &x, const auto &y) { return x.frame < y.frame; });
    return dslvm::Value::none();
  });
  registry.method("Scene", "area_light", {"float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->area_lights.size() >= 128) throw dslvm::Error("area light limit exceeded (128)");
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("area light values must be finite");
    for (int i = 0; i < 3; ++i) if (std::abs(a[i].f) > 1'000'000 || std::abs(a[i+3].f) > 1'000'000 || a[i+6].f < 0 || a[i+6].f > 1) throw dslvm::Error("invalid area light position, target, or color");
    if (a[9].f < 0 || a[9].f > 1'000'000 || a[10].f <= 0 || a[10].f > 10000) throw dslvm::Error("area light strength or size outside supported bounds");
    AreaLightData light;
    light.position = {float(a[0].f), float(a[1].f), float(a[2].f)};
    light.target = {float(a[3].f), float(a[4].f), float(a[5].f)};
    light.color = {float(a[6].f), float(a[7].f), float(a[8].f)};
    light.strength = float(a[9].f); light.size = light.size_y = float(a[10].f);
    remove_preset_area_rig(*scene);
    scene->area_lights.push_back(light);
    return dslvm::Value::none();
  });
  registry.method("Scene", "area_light_shape", {"float","float","float","float","float","float","float","float","float","float","float","float","string"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->area_lights.size() >= 128) throw dslvm::Error("area light limit exceeded (128)");
    for (size_t i = 0; i < 12; ++i) if (!std::isfinite(a[i].f)) throw dslvm::Error("area light values must be finite");
    for (int i = 0; i < 3; ++i) {
      if (std::abs(a[i].f) > 1'000'000 || std::abs(a[i+3].f) > 1'000'000 || a[i+6].f < 0 || a[i+6].f > 1)
        throw dslvm::Error("invalid area light position, target, or color");
    }
    if (a[9].f < 0 || a[9].f > 1'000'000 || a[10].f <= 0 || a[10].f > 10000 || a[11].f <= 0 || a[11].f > 10000)
      throw dslvm::Error("area light strength or dimensions outside supported bounds");
    if (a[12].s != "rectangle" && a[12].s != "disk" && a[12].s != "ellipse")
      throw dslvm::Error("area light shape must be rectangle, disk, or ellipse");
    AreaLightData light;
    for (int i=0;i<3;++i) { light.position[i]=float(a[i].f); light.target[i]=float(a[i+3].f); light.color[i]=float(a[i+6].f); }
    light.strength=float(a[9].f); light.size=float(a[10].f); light.size_y=float(a[11].f);
    light.shape = a[12].s == "rectangle" ? 0 : a[12].s == "disk" ? 1 : 2;
    if (light.shape == 1) light.size_y = light.size;
    remove_preset_area_rig(*scene);
    scene->area_lights.push_back(light);
    return dslvm::Value::none();
  });
  registry.method("Scene", "sun_light", {"float", "float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p); if(scene->sun_lights.size()>=128) throw dslvm::Error("sun light limit exceeded (128)");
    for(const auto &v:a) if(!std::isfinite(v.f)) throw dslvm::Error("sun light values must be finite");
    const double len=std::sqrt(a[0].f*a[0].f+a[1].f*a[1].f+a[2].f*a[2].f);
    if(len<1.0e-8 || len>1'000'000) throw dslvm::Error("sun direction must be nonzero and bounded");
    for(int i=3;i<6;++i) if(a[i].f<0||a[i].f>1) throw dslvm::Error("sun color channels must be in [0,1]");
    if(a[6].f<0||a[6].f>1'000'000||a[7].f<0||a[7].f>90) throw dslvm::Error("sun strength or angle outside supported bounds");
    SunLightData light; for(int i=0;i<3;++i){light.direction[i]=float(a[i].f/len);light.color[i]=float(a[i+3].f);} light.strength=float(a[6].f);light.angle=float(a[7].f*M_PI/180.0);remove_preset_area_rig(*scene);scene->sun_lights.push_back(light);
    return dslvm::Value::none();
  });
  registry.method("Scene", "spot_light", {"float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p); if(scene->spot_lights.size()>=128) throw dslvm::Error("spot light limit exceeded (128)");
    for(const auto &v:a) if(!std::isfinite(v.f)) throw dslvm::Error("spot light values must be finite");
    for(int i=0;i<6;++i) if(std::abs(a[i].f)>1'000'000) throw dslvm::Error("spot light position or target exceeds scene bounds");
    double dx=a[3].f-a[0].f,dy=a[4].f-a[1].f,dz=a[5].f-a[2].f;
    if(dx*dx+dy*dy+dz*dz<1.0e-12) throw dslvm::Error("spot light position and target must differ");
    for(int i=6;i<9;++i) if(a[i].f<0||a[i].f>1) throw dslvm::Error("spot light color channels must be in [0,1]");
    if(a[9].f<0||a[9].f>1'000'000||a[10].f<1||a[10].f>170||a[11].f<0||a[11].f>1) throw dslvm::Error("spot light strength, angle, or blend outside supported bounds");
    SpotLightData light; for(int i=0;i<3;++i){light.position[i]=float(a[i].f);light.target[i]=float(a[i+3].f);light.color[i]=float(a[i+6].f);} light.strength=float(a[9].f);light.angle=float(a[10].f*M_PI/180.0);light.blend=float(a[11].f);remove_preset_area_rig(*scene);scene->spot_lights.push_back(light);
    return dslvm::Value::none();
  });
  registry.method("Scene", "ground_plane", {"float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("ground plane values must be finite");
    if (a[1].f <= 0 || a[1].f > 1'000'000) throw dslvm::Error("ground plane size outside supported bounds");
    for (int i = 2; i < 5; ++i) if (a[i].f < 0 || a[i].f > 1) throw dslvm::Error("ground color channels must be in [0,1]");
    auto material = std::make_unique<Material>();
    material->color = {float(a[2].f), float(a[3].f), float(a[4].f)};
    material->roughness = 0.85f;
    Material *mat = material.get(); scene->materials.push_back(std::move(material));
    auto mesh = std::make_unique<Mesh>();
    mesh->vertices = {{{-a[1].f,-a[1].f,a[0].f},{a[1].f,-a[1].f,a[0].f},{a[1].f,a[1].f,a[0].f},{-a[1].f,a[1].f,a[0].f}}};
    mesh->triangles = {{{0,1,2},{0,2,3}}}; mesh->material = mat; mesh->ground_plane = true;
    adopt_mesh(*scene, std::move(mesh));
    return dslvm::Value::none();
  });
  registry.method("Scene", "checker_ground", {"float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if(!std::isfinite(a[0].f)||!std::isfinite(a[1].f)||a[0].f<=0||a[0].f>10000||a[1].f<0.1||a[1].f>1000)throw dslvm::Error("checker ground size must be (0,10000] and tile scale 0.1..1000");
    auto material=std::make_unique<Material>();material->color={0.035f,0.045f,0.06f};material->texture_color2={0.72f,0.75f,0.78f};material->texture_scale=float(a[1].f);material->texture_kind=1;material->roughness=0.82f;Material *mat=material.get();scene->materials.push_back(std::move(material));
    const double s=a[0].f;auto mesh=std::make_unique<Mesh>();mesh->vertices={{{-s,-s,-0.02},{s,-s,-0.02},{s,s,-0.02},{-s,s,-0.02}}};mesh->triangles={{{0,2,1},{0,3,2}}};mesh->uvs={{0,0},{1,1},{1,0},{0,0},{0,1},{1,1}};mesh->material=mat;mesh->ground_plane=true;adopt_mesh(*scene, std::move(mesh));return dslvm::Value::none();
  });
  registry.method("Scene", "wall_plane_y", {"float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("wall plane values must be finite");
    if (std::abs(a[0].f) > 1'000'000 || a[1].f <= 0 || a[1].f > 1'000'000) throw dslvm::Error("wall plane position or size outside supported bounds");
    for (int i = 2; i < 5; ++i) if (a[i].f < 0 || a[i].f > 1) throw dslvm::Error("wall color channels must be in [0,1]");
    auto material = std::make_unique<Material>();
    material->color = {float(a[2].f), float(a[3].f), float(a[4].f)};
    material->emission = material->color;
    material->emission_strength = 0.3f;
    material->roughness = 0.9f;
    Material *mat = material.get(); scene->materials.push_back(std::move(material));
    auto mesh = std::make_unique<Mesh>(); const double s = a[1].f, y = a[0].f;
    mesh->vertices = {{{-s,y,-s},{s,y,-s},{s,y,s},{-s,y,s}}};
    mesh->triangles = {{{0,1,2},{0,2,3}}}; mesh->material = mat; mesh->ground_plane = true;
    adopt_mesh(*scene, std::move(mesh));
    return dslvm::Value::none();
  });
  registry.method("Scene", "material", {"float", "float", "float", "float", "float", "float", "float", "float", "float", "float"}, "Material", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->materials.size() >= 4096) throw dslvm::Error("material limit exceeded (4096)");
    for (size_t i = 0; i < a.size(); ++i)
      if (!std::isfinite(a[i].f)) throw dslvm::Error("material values must be finite");
    for (size_t i = 0; i < 3; ++i)
      if (a[i].f < 0.0 || a[i].f > 1.0 || a[i + 6].f < 0.0 || a[i + 6].f > 1.0)
        throw dslvm::Error("material color and emission channels must be in [0,1]");
    if (a[3].f < 0.0 || a[3].f > 1.0 || a[4].f < 0.0 || a[4].f > 1.0 || a[5].f < 0.0 || a[5].f > 1.0 || a[9].f < 0.0 || a[9].f > 10000.0)
      throw dslvm::Error("material metallic, roughness, and transmission must be in [0,1], emission strength in [0,10000]");
    auto material = std::make_unique<Material>();
    material->color = {float(a[0].f), float(a[1].f), float(a[2].f)};
    material->metallic = float(a[3].f);
    material->roughness = float(a[4].f);
    material->transmission = float(a[5].f);
    material->emission = {float(a[6].f), float(a[7].f), float(a[8].f)};
    material->emission_strength = float(a[9].f);
    Material *result = material.get();
    scene->materials.push_back(std::move(material));
    return dslvm::Value::object("Material", result);
  });
  registry.method("Material", "subsurface", {"float", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *material = static_cast<Material *>(p);
    for (const auto &v : a) if (!std::isfinite(v.f)) throw dslvm::Error("subsurface values must be finite");
    if (a[0].f < 0.0 || a[0].f > 1.0 || a[1].f < 0.0 || a[1].f > 1.0 || a[2].f < 0.0 || a[2].f > 1.0 || a[3].f < 0.0 || a[3].f > 1.0 || a[4].f < 0.001 || a[4].f > 10.0)
      throw dslvm::Error("subsurface weight/radius must be in [0,1] and scale in [0.001,10]");
    material->subsurface = float(a[0].f);
    material->subsurface_radius = {float(a[1].f), float(a[2].f), float(a[3].f)};
    material->subsurface_scale = float(a[4].f);
    return dslvm::Value::none();
  });
  registry.method("Material", "volume", {"string", "float", "float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *material = static_cast<Material *>(p);
    if (a[0].s != "absorption" && a[0].s != "scatter") throw dslvm::Error("volume kind must be absorption or scatter");
    for (size_t i = 1; i < a.size(); ++i) if (!std::isfinite(a[i].f)) throw dslvm::Error("volume values must be finite");
    if (a[1].f < 0.0 || a[1].f > 100.0 || a[2].f < 0.0 || a[2].f > 1.0 || a[3].f < 0.0 || a[3].f > 1.0 || a[4].f < 0.0 || a[4].f > 1.0)
      throw dslvm::Error("volume density must be in [0,100] and color channels in [0,1]");
    material->volume_kind = a[0].s == "absorption" ? 1 : 2;
    material->volume_only = true;
    material->volume_density = float(a[1].f);
    material->volume_color = {float(a[2].f), float(a[3].f), float(a[4].f)};
    return dslvm::Value::none();
  });
  auto patterned_material = [](void *p, dslvm::Args &a, int kind) {
    auto *scene=static_cast<Scene *>(p); if(scene->materials.size()>=4096) throw dslvm::Error("material limit exceeded (4096)");
    for(const auto &v:a) if(!std::isfinite(v.f)) throw dslvm::Error("procedural material values must be finite");
    for(int i=0;i<6;++i) if(a[i].f<0||a[i].f>1) throw dslvm::Error("procedural color channels must be in [0,1]");
    if(a[6].f<0.1||a[6].f>1000||a[7].f<0||a[7].f>1) throw dslvm::Error("procedural scale or roughness outside supported bounds");
    auto material=std::make_unique<Material>();
    material->color={float(a[0].f),float(a[1].f),float(a[2].f)};
    material->texture_color2={float(a[3].f),float(a[4].f),float(a[5].f)};
    material->texture_scale=float(a[6].f);material->roughness=float(a[7].f);material->texture_kind=kind;
    Material *result=material.get();scene->materials.push_back(std::move(material));
    return dslvm::Value::object("Material",result);
  };
  registry.method("Scene", "checker_material", {"float", "float", "float", "float", "float", "float", "float", "float"}, "Material", [patterned_material](void *p,dslvm::Args &a){return patterned_material(p,a,1);});
  registry.method("Scene", "noise_material", {"float", "float", "float", "float", "float", "float", "float", "float"}, "Material", [patterned_material](void *p,dslvm::Args &a){return patterned_material(p,a,2);});
  registry.method("Scene", "ramp_material", {"float","float","float","float","float","float","float","float"}, "Material", [patterned_material](void *p,dslvm::Args &a){return patterned_material(p,a,4);});
  registry.method("Material", "bump", {"float","float","float"}, "void", [](void *p,dslvm::Args &a){auto*m=static_cast<Material*>(p);for(const auto&v:a)if(!std::isfinite(v.f))throw dslvm::Error("bump parameters must be finite");if(a[0].f<0||a[0].f>1||a[1].f<0||a[1].f>2||a[2].f<0.1||a[2].f>1000)throw dslvm::Error("bump strength must be 0..1, distance 0..2 scene units, texture scale 0.1..1000");m->bump_enabled=true;m->bump_strength=float(a[0].f);m->bump_distance=float(a[1].f);m->bump_scale=float(a[2].f);return dslvm::Value::none();});
  registry.method("Material", "texture_maps", {"string","string","string","string"}, "void", [](void *p, dslvm::Args &a) {
    auto *material = static_cast<Material *>(p);
    std::array<std::string *,4> destinations{&material->normal_map_path,&material->roughness_map_path,
                                             &material->metallic_map_path,&material->ao_map_path};
    static const char *const labels[4] = {"normal map", "roughness map", "metallic map", "AO map"};
    uint64_t total_bytes = 0;
    for (size_t i=0;i<a.size();++i) {
      if (a[i].s.empty()) { *destinations[i] = {}; continue; }
      *destinations[i] = checked_image_path(a[i].s, labels[i], {".png", ".jpg", ".jpeg", ".exr"}, 64u * 1024u * 1024u);
      std::error_code ec;
      total_bytes += std::filesystem::file_size(a[i].s, ec);
      if (ec || total_bytes > 256u * 1024u * 1024u) throw dslvm::Error("IMAGE_LIMIT", "texture maps must total at most 256 MiB (64 MiB per map)");
    }
    return dslvm::Value::none();
  });
  registry.method("Material", "mix_with", {"Material","float"}, "void", [](void *p,dslvm::Args &a) {
    auto *material=static_cast<Material *>(p);
    auto *other=static_cast<Material *>(a[0].obj);
    if (!other || other==material || other->mix_material || other->texture_kind != 0 || other->bump_enabled ||
        !other->normal_map_path.empty() || !other->roughness_map_path.empty() || !other->metallic_map_path.empty() ||
        !other->ao_map_path.empty() || other->backface_culling)
      throw dslvm::Error("mix_with needs a distinct plain material as its second layer");
    if (!std::isfinite(a[1].f)||a[1].f<0||a[1].f>1) throw dslvm::Error("mix factor must be in [0,1]");
    material->mix_material=other; material->mix_factor=float(a[1].f); material->mix_factor_image.clear();
    return dslvm::Value::none();
  });
  registry.method("Material", "mix_with_texture", {"Material","string"}, "void", [](void *p,dslvm::Args &a) {
    auto *material=static_cast<Material *>(p);
    auto *other=static_cast<Material *>(a[0].obj);
    if (!other || other==material || other->mix_material || other->texture_kind != 0 || other->bump_enabled ||
        !other->normal_map_path.empty() || !other->roughness_map_path.empty() || !other->metallic_map_path.empty() ||
        !other->ao_map_path.empty() || other->backface_culling)
      throw dslvm::Error("mix_with_texture needs a distinct plain material as its second layer");
    const std::string image = checked_image_path(a[1].s, "mix factor texture", {".png", ".jpg", ".jpeg", ".exr"}, 64u * 1024u * 1024u);
    material->mix_material=other;material->mix_factor_image=image;
    return dslvm::Value::none();
  });
  registry.method("Material", "backface_culling", {"bool"}, "void", [](void *p,dslvm::Args &a) {
    static_cast<Material *>(p)->backface_culling=a[0].b;
    return dslvm::Value::none();
  });
  registry.method("Scene", "image_material", {"string", "float"}, "Material", [](void *p, dslvm::Args &a) {
    const std::string image = checked_image_path(a[0].s, "image_material input", {".png", ".jpg", ".jpeg", ".exr"}, 64u * 1024u * 1024u);
    if (!std::isfinite(a[1].f) || a[1].f < 0.0 || a[1].f > 1.0)
      throw dslvm::Error("image_material roughness must be in [0,1]");
    auto *scene = static_cast<Scene *>(p);
    if (scene->materials.size() >= 4096) throw dslvm::Error("material limit exceeded (4096)");
    auto material = std::make_unique<Material>();
    material->texture_kind = 3;
    material->texture_scale = 1.0f;
    material->roughness = float(a[1].f);
    material->image_path = image;
    Material *result = material.get();
    scene->materials.push_back(std::move(material));
    return dslvm::Value::object("Material", result);
  });
  auto import = [](void *p, dslvm::Args &args, bool stl) {
    auto *scene = static_cast<Scene *>(p);
    if (args[0].s.empty()) throw dslvm::Error("mesh input path must not be empty");
    if (scene->meshes.size() >= 4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    auto mesh = stl ? std::make_unique<Mesh>(read_stl(args[0].s)) : obj_import::read(*scene, args[0].s);
    Mesh *result = mesh.get();
    adopt_mesh(*scene, std::move(mesh));
    return dslvm::Value::object("Mesh", result);
  };
  registry.method("Scene", "import_obj", {"string"}, "Mesh", [import](void *p, dslvm::Args &a) { return import(p, a, false); });
  registry.method("Scene", "import_stl", {"string"}, "Mesh", [import](void *p, dslvm::Args &a) { return import(p, a, true); });
  registry.method("Scene", "import_ply", {"string"}, "Mesh", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (scene->meshes.size() >= 4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    auto mesh = std::make_unique<Mesh>(read_ply_ascii(a[0].s));
    Mesh *result = mesh.get();
    adopt_mesh(*scene, std::move(mesh));
    return dslvm::Value::object("Mesh", result);
  });
  registry.method("Scene", "import_gltf", {"string"}, "Mesh", [](void *p, dslvm::Args &a) {
    return dslvm::Value::object("Mesh", gltf_import::read(*static_cast<Scene *>(p), a[0].s));
  });
  registry.method("Mesh", "set_material", {"Material"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    if (!a[0].obj) throw dslvm::Error("set_material requires a material created by this scene");
    mesh->material = static_cast<Material *>(a[0].obj);
    return dslvm::Value::none();
  });
  registry.method("Mesh", "set_smooth", {"bool"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    if (a[0].b && !mesh->vertices.empty()) {
      std::map<std::array<double, 3>, uint32_t> unique;
      std::vector<std::array<double, 3>> welded;
      std::vector<uint32_t> remap(mesh->vertices.size());
      welded.reserve(mesh->vertices.size());
      for (size_t i = 0; i < mesh->vertices.size(); ++i) {
        auto [it, inserted] = unique.emplace(mesh->vertices[i], uint32_t(welded.size()));
        if (inserted) welded.push_back(mesh->vertices[i]);
        remap[i] = it->second;
      }
      for (auto &triangle : mesh->triangles)
        for (uint32_t &index : triangle) index = remap[index];
      mesh->vertices.swap(welded);
    }
    mesh->smooth = a[0].b;
    return dslvm::Value::none();
  });
  registry.method("Mesh", "translate", {"float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    std::array<float, 3> proposed;
    for (size_t i = 0; i < 3; ++i) {
      if (!std::isfinite(a[i].f) || std::abs(a[i].f) > 1'000'000.0) throw dslvm::Error("mesh translation must be finite and within scene bounds");
      proposed[i] = mesh->translation[i] + float(a[i].f);
      if (std::abs(proposed[i]) > 1'000'000.0f) throw dslvm::Error("mesh translation exceeds scene bounds");
    }
    mesh->translation = proposed;
    return dslvm::Value::none();
  });
  registry.method("Mesh", "scale", {"float", "float", "float"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    std::array<float, 3> proposed;
    for (size_t i = 0; i < 3; ++i) {
      if (!std::isfinite(a[i].f) || std::abs(a[i].f) < 0.001 || std::abs(a[i].f) > 1000.0)
        throw dslvm::Error("mesh scale components must have magnitude in [0.001,1000]");
      proposed[i] = mesh->scale[i] * float(a[i].f);
      if (std::abs(proposed[i]) < 0.001f || std::abs(proposed[i]) > 1000.0f) throw dslvm::Error("combined mesh scale is outside [0.001,1000]");
    }
    mesh->scale = proposed;
    return dslvm::Value::none();
  });
  registry.method("Mesh", "keyframe_transform", {"int", "float", "float", "float", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    if (a[0].i < 0 || a[0].i > 1'000'000) throw dslvm::Error("keyframe frame must be in [0,1000000]");
    if (a[4].s != "linear" && a[4].s != "ease") throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    std::array<float,3> value{};
    for (size_t i=0;i<3;++i) { if (!std::isfinite(a[i+1].f) || std::abs(a[i+1].f)>1'000'000) throw dslvm::Error("keyframed position must be finite and within scene bounds"); value[i]=float(a[i+1].f); }
    auto &keys=mesh->transform_keys;
    auto it=std::find_if(keys.begin(),keys.end(),[&](const auto &key){return key.frame==a[0].i;});
    Mesh::TransformKey key{int(a[0].i),value,a[4].s};
    if(it==keys.end()) keys.push_back(key); else *it=key;
    std::sort(keys.begin(),keys.end(),[](const auto &x,const auto &y){return x.frame<y.frame;});
    return dslvm::Value::none();
  });
  registry.method("Mesh", "keyframe_visible", {"int", "bool", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    if (a[0].i < 0 || a[0].i > 1'000'000) throw dslvm::Error("visibility keyframe must be in [0,1000000]");
    if (a[2].s != "linear" && a[2].s != "ease") throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    auto &keys = mesh->visibility_keys;
    auto it = std::find_if(keys.begin(), keys.end(), [&](const auto &key) { return key.frame == a[0].i; });
    Mesh::VisibilityKey key{int(a[0].i), a[1].b};
    if (it == keys.end()) keys.push_back(key); else *it = key;
    std::sort(keys.begin(), keys.end(), [](const auto &x, const auto &y) { return x.frame < y.frame; });
    return dslvm::Value::none();
  });
  registry.method("Material", "keyframe_color", {"int", "float", "float", "float", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *material=static_cast<Material *>(p);
    if(a[0].i<0||a[0].i>1'000'000)throw dslvm::Error("keyframe frame must be in [0,1000000]");
    if(a[4].s!="linear"&&a[4].s!="ease")throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    std::array<float,3> value{};
    for(size_t i=0;i<3;++i){if(!std::isfinite(a[i+1].f)||a[i+1].f<0||a[i+1].f>1)throw dslvm::Error("keyframed color channels must be in [0,1]");value[i]=float(a[i+1].f);}
    auto &keys=material->color_keys;
    auto it=std::find_if(keys.begin(),keys.end(),[&](const auto &key){return key.frame==a[0].i;});
    Material::ColorKey key{int(a[0].i),value,a[4].s};
    if(it==keys.end())keys.push_back(key);else *it=key;
    std::sort(keys.begin(),keys.end(),[](const auto &x,const auto &y){return x.frame<y.frame;});
    return dslvm::Value::none();
  });
  registry.method("Material", "keyframe_emission_strength", {"int", "float", "string"}, "void", [](void *p, dslvm::Args &a) {
    auto *material = static_cast<Material *>(p);
    if (a[0].i < 0 || a[0].i > 1'000'000) throw dslvm::Error("emission keyframe must be in [0,1000000]");
    if (a[2].s != "linear" && a[2].s != "ease") throw dslvm::Error("interpolation must be 'linear' or 'ease'");
    if (!std::isfinite(a[1].f) || a[1].f < 0 || a[1].f > 1'000'000) throw dslvm::Error("keyframed emission strength must be in [0,1000000]");
    auto &keys = material->emission_keys;
    auto it = std::find_if(keys.begin(), keys.end(), [&](const auto &key) { return key.frame == a[0].i; });
    Material::StrengthKey key{int(a[0].i), float(a[1].f), a[2].s};
    if (it == keys.end()) keys.push_back(key); else *it = key;
    std::sort(keys.begin(), keys.end(), [](const auto &x, const auto &y) { return x.frame < y.frame; });
    return dslvm::Value::none();
  });
  registry.method("Mesh", "rotate_x", {"float"}, "void", [](void *p, dslvm::Args &a) {
    if (!std::isfinite(a[0].f) || std::abs(a[0].f) > 360000.0) throw dslvm::Error("mesh X rotation must be finite and bounded");
    auto *mesh = static_cast<Mesh *>(p);
    if (mesh->geometry_source) throw dslvm::Error("cannot rotate an instance; rotate its source mesh before instancing");
    const double angle = a[0].f * M_PI / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    auto rotate = [c,s](auto &v) { const auto y=v[1], z=v[2]; v[1]=c*y-s*z; v[2]=s*y+c*z; };
    for (auto &v : mesh->vertices) rotate(v);
    for (auto &n : mesh->corner_normals) rotate(n);
    return dslvm::Value::none();
  });
  registry.method("Mesh", "rotate_y", {"float"}, "void", [](void *p, dslvm::Args &a) {
    if (!std::isfinite(a[0].f) || std::abs(a[0].f) > 360000.0) throw dslvm::Error("mesh Y rotation must be finite and bounded");
    auto *mesh = static_cast<Mesh *>(p);
    if (mesh->geometry_source) throw dslvm::Error("cannot rotate an instance; rotate its source mesh before instancing");
    const double angle = a[0].f * M_PI / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    auto rotate = [c,s](auto &v) { const auto x=v[0], z=v[2]; v[0]=c*x+s*z; v[2]=-s*x+c*z; };
    for (auto &v : mesh->vertices) rotate(v);
    for (auto &n : mesh->corner_normals) rotate(n);
    return dslvm::Value::none();
  });
  registry.method("Mesh", "rotate_z", {"float"}, "void", [](void *p, dslvm::Args &a) {
    if (!std::isfinite(a[0].f) || std::abs(a[0].f) > 360000.0) throw dslvm::Error("mesh Z rotation must be finite and bounded");
    auto *mesh = static_cast<Mesh *>(p);
    if (mesh->geometry_source) throw dslvm::Error("cannot rotate an instance; rotate its source mesh before instancing");
    const double angle = a[0].f * M_PI / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    auto rotate = [c,s](auto &v) { const auto x=v[0], y=v[1]; v[0]=c*x-s*y; v[1]=s*x+c*y; };
    for (auto &v : mesh->vertices) rotate(v);
    for (auto &n : mesh->corner_normals) rotate(n);
    return dslvm::Value::none();
  });
  registry.method("Scene", "instance_mesh", {"Mesh", "float", "float", "float", "float", "float", "float"}, "Mesh", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    auto *source = static_cast<Mesh *>(a[0].obj);
    const auto owned = std::find_if(scene->meshes.begin(), scene->meshes.end(), [source](const auto &item) { return item.get() == source; });
    if (owned == scene->meshes.end()) throw dslvm::Error("instance_mesh source must belong to this scene");
    if (scene->meshes.size() >= 4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    auto instance = std::make_unique<Mesh>();
    instance->geometry_source = source->geometry_source ? source->geometry_source : source;
    instance->material = instance->geometry_source->material;
    // Shading follows the shared geometry; Mesh.set_smooth on the instance still overrides it.
    instance->smooth = instance->geometry_source->smooth;
    for (size_t i = 0; i < 6; ++i) if (!std::isfinite(a[i + 1].f)) throw dslvm::Error("instance transform must be finite");
    for (size_t i = 0; i < 3; ++i) {
      if (std::abs(a[i + 1].f) > 1'000'000.0) throw dslvm::Error("instance position exceeds scene bounds");
      if (std::abs(a[i + 4].f) < 0.001 || std::abs(a[i + 4].f) > 1000.0) throw dslvm::Error("instance scale must have magnitude in [0.001,1000]");
      instance->translation[i] = float(a[i + 1].f);
      instance->scale[i] = float(a[i + 4].f);
    }
    Mesh *result = instance.get();
    adopt_mesh(*scene, std::move(instance));
    return dslvm::Value::object("Mesh", result);
  });
  registry.method("Scene", "render_png", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    const auto *scene = static_cast<const Scene *>(p);
    if (std::filesystem::path(a[0].s).extension() != ".png") throw dslvm::Error("render_png output must use the .png extension");
    if (a[1].i < 0 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096)
      throw dslvm::Error("render_png samples and dimensions are outside supported bounds");
    render_image(*scene, a[0].s, a[1].i == 0 ? scene->preset_samples : int(a[1].i), int(a[2].i), int(a[3].i));
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_sequence", {"string", "int", "int", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if(a[1].i<0||a[2].i<a[1].i||a[2].i-a[1].i>239)throw dslvm::Error("render_sequence frame range must be ordered, non-negative, and contain at most 240 frames");
    if(a[3].i<0||a[3].i>65536)throw dslvm::Error("render_sequence samples must be 0 (use preset) or within [1,65536]");
    if(a[4].i<16||a[4].i>4096||a[5].i<16||a[5].i>4096)throw dslvm::Error("render_sequence dimensions must each be within [16,4096]");
    const uint64_t pixels_per_frame=uint64_t(a[4].i)*uint64_t(a[5].i);
    if(pixels_per_frame>16'000'000)throw dslvm::Error("render_sequence per-frame pixel budget is 16,000,000; requested "+std::to_string(pixels_per_frame)+" pixels per frame; lower width or height");
    const uint64_t frame_count=uint64_t(a[2].i-a[1].i)+1;
    const uint64_t total_pixels=pixels_per_frame*frame_count;
    if(total_pixels>64'000'000) {
      const uint64_t max_frames=64'000'000/pixels_per_frame;
      throw dslvm::Error("render_sequence total-pixel budget is 64,000,000; requested "+std::to_string(frame_count)+" frames at "+std::to_string(a[4].i)+"x"+std::to_string(a[5].i)+" = "+std::to_string(total_pixels)+" pixels; use <= "+std::to_string(max_frames)+" frames per call");
    }
    const int sequence_samples = a[3].i == 0 ? scene->preset_samples : int(a[3].i);
    const AnimationBase base = capture_animation(*scene);
    const int old_timeout=scene->render_timeout_seconds;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(old_timeout);
    try {
      for(int frame=int(a[1].i);frame<=int(a[2].i);++frame){
        const auto remaining=std::chrono::duration_cast<std::chrono::seconds>(deadline-std::chrono::steady_clock::now()).count();
        if(remaining<1)throw dslvm::Error("RENDER_TIMEOUT","render_sequence exceeded its total "+std::to_string(old_timeout)+" second time budget before frame "+std::to_string(frame));
        scene->render_timeout_seconds=int(remaining);
        apply_animation(*scene, frame, base);
        render_image(*scene,sequence_path(a[0].s,frame),sequence_samples,int(a[4].i),int(a[5].i));
      }
    } catch (...) { scene->render_timeout_seconds=old_timeout; restore_animation(*scene, base); throw; }
    scene->render_timeout_seconds=old_timeout;
    restore_animation(*scene, base);
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_apng", {"string", "int", "int", "int", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene=static_cast<Scene *>(p);
    if(std::filesystem::path(a[0].s).extension()!=".apng"||a[0].s.empty()||a[0].s.front()=='-')throw dslvm::Error("render_apng output must be a .apng path not beginning with '-'");
    if(a[1].i<0||a[2].i<a[1].i||a[2].i-a[1].i>239||a[6].i<1||a[6].i>240)throw dslvm::Error("render_apng frame range must be ordered, non-negative, and contain at most 240 frames; fps must be in [1,240]");
    if(a[3].i<1||a[3].i>65536)throw dslvm::Error("render_apng samples must be within [1,65536]");
    if(a[4].i<16||a[4].i>4096||a[5].i<16||a[5].i>4096)throw dslvm::Error("render_apng dimensions must each be within [16,4096]");
    const uint64_t apng_pixels=uint64_t(a[4].i)*uint64_t(a[5].i), apng_frames=uint64_t(a[2].i-a[1].i)+1;
    if(apng_pixels>16'000'000)throw dslvm::Error("render_apng per-frame pixel budget is 16,000,000; requested "+std::to_string(apng_pixels)+" pixels per frame; lower width or height");
    if(apng_pixels*apng_frames>64'000'000)throw dslvm::Error("render_apng total-pixel budget is 64,000,000; requested "+std::to_string(apng_frames)+" frames x "+std::to_string(apng_pixels)+" pixels/frame = "+std::to_string(apng_pixels*apng_frames)+" pixels; use at most "+std::to_string(64'000'000/apng_pixels)+" frames per call");
    const AnimationBase base = capture_animation(*scene);
    const int old_timeout=scene->render_timeout_seconds;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(old_timeout);
    std::string made_dir;try{made_dir=lmblender::platform::make_temp_dir("lmblender-apng").string();}catch(const std::exception &){throw dslvm::Error("could not create APNG staging directory");}const char *made=made_dir.c_str();
    std::vector<std::string> frames;
    try{for(int frame=int(a[1].i);frame<=int(a[2].i);++frame){const auto remaining=std::chrono::duration_cast<std::chrono::seconds>(deadline-std::chrono::steady_clock::now()).count();if(remaining<1)throw dslvm::Error("RENDER_TIMEOUT","render_apng exceeded its total "+std::to_string(old_timeout)+" second time budget before frame "+std::to_string(frame));scene->render_timeout_seconds=int(remaining);apply_animation(*scene,frame,base);std::string path=std::string(made)+"/frame-"+std::to_string(frame)+".png";render_image(*scene,path,int(a[3].i),int(a[4].i),int(a[5].i));frames.push_back(path);}write_apng_from_pngs(a[0].s,frames,int(a[6].i));}
    catch(...){scene->render_timeout_seconds=old_timeout;restore_animation(*scene,base);std::error_code ec;std::filesystem::remove_all(made,ec);throw;}
    scene->render_timeout_seconds=old_timeout;restore_animation(*scene,base);std::error_code ec;std::filesystem::remove_all(made,ec);return dslvm::Value::none();
  });
  registry.method("Scene", "render_exr", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (std::filesystem::path(a[0].s).extension() != ".exr") throw dslvm::Error("render_exr output must use the .exr extension");
    if (a[1].i < 1 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096)
      throw dslvm::Error("render_exr samples and dimensions are outside supported bounds");
    render_image(*scene, a[0].s, int(a[1].i), int(a[2].i), int(a[3].i));
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_passes", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<const Scene *>(p);
    if (a[0].s.empty() || !std::filesystem::path(a[0].s).extension().empty())
      throw dslvm::Error("render_passes requires an output prefix without a file extension");
    if (a[1].i < 1 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096 ||
        uint64_t(a[2].i) * uint64_t(a[3].i) > 16'000'000)
      throw dslvm::Error("render_passes samples and dimensions are outside supported bounds");
    render_direct(*scene, a[0].s, int(a[1].i), int(a[2].i), int(a[3].i), true);
    return dslvm::Value::none();
  });
  for (const char *field : {"vertex_count", "triangle_count"}) {
    registry.method("Mesh", field, {}, "int", [field](void *p, dslvm::Args &) {
      auto *mesh = static_cast<Mesh *>(p);
      return dslvm::Value::integer(static_cast<int64_t>(std::string(field) == "vertex_count" ? mesh->vertices.size() : mesh->triangles.size()));
    });
  }
  registry.method("Mesh", "material_count", {}, "int", [](void *p, dslvm::Args &) {
    const auto *mesh = static_cast<const Mesh *>(p);
    const auto *data = mesh->geometry_source ? mesh->geometry_source : mesh;
    return dslvm::Value::integer(int64_t(data->material_slots.size()));
  });
  for (const char *field : {"min_x", "min_y", "min_z", "max_x", "max_y", "max_z"}) {
    registry.method("Mesh", field, {}, "float", [field](void *p, dslvm::Args &) {
      auto *mesh = static_cast<Mesh *>(p);
      const size_t axis = std::string(field).back() == 'x' ? 0 : std::string(field).back() == 'y' ? 1 : 2;
      const bool minimum = std::string(field).rfind("min_", 0) == 0;
      double value = mesh->vertices.front()[axis];
      for (const auto &v : mesh->vertices) value = minimum ? std::min(value, v[axis]) : std::max(value, v[axis]);
      return dslvm::Value::real(value);
    });
  }
  registry.method("Scene", "expect_int", {"int", "int"}, "void", [](void *, dslvm::Args &a) {
    if (a[0].i != a[1].i) throw dslvm::Error("integer expectation failed: got " + std::to_string(a[0].i) + ", expected " + std::to_string(a[1].i));
    return dslvm::Value::none();
  });
  registry.method("Scene", "version", {}, "string", [](void *, dslvm::Args &) {
    return dslvm::Value::string("lmblender " LMBLENDER_VERSION " (DSL bytecode v5; Cycles " CYCLES_VERSION_STRING ")");
  });
  return registry;
}
}

int main(int argc, char **argv) {
  lmblender::platform::binary_stdout();
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmblender " LMBLENDER_VERSION " (DSL bytecode v5)\nCycles " << CYCLES_VERSION_STRING << '\n';
#ifdef WITH_OPENIMAGEDENOISE
    std::cout << "OpenImageDenoise " << OIDN_VERSION_STRING << '\n';
#else
    std::cout << "OpenImageDenoise disabled\n";
#endif
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--presets") {
    for (const auto &preset : builtin_presets().at("presets"))
      std::cout << preset.at("name").get<std::string>() << "\t" << preset.at("description").get<std::string>() << '\n';
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--preset-show") {
    try { std::cout << find_builtin_preset(argv[2]).dump(2) << '\n'; return 0; }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 2; }
  }
  auto registry = make_registry();
  const auto print_embedded_schema = [] {
    std::cout.write(reinterpret_cast<const char *>(lmblender::embedded_schema),
                    static_cast<std::streamsize>(lmblender::embedded_schema_size));
    return std::cout ? 0 : 4;
  };
  const auto print_schema_summary = [&registry] {
    std::vector<std::string> types;
    std::map<std::string, std::vector<std::string>> methods;
    std::string current;
    for (const auto &signature : registry.signatures()) {
      if (signature.rfind("context ", 0) == 0 || signature.rfind("fn ", 0) == 0) {
        std::cout << signature << '\n';
      } else if (signature.rfind("method ", 0) == 0) {
        const std::string body = signature.substr(7);
        const std::string type = body.substr(0, body.find('.'));
        if (type != current) { current = type; types.push_back(type); }
        methods[type].push_back(body);
      }
    }
    for (const auto &type : types) {
      std::cout << type << ":\n";
      for (const auto &method : methods[type]) std::cout << "  " << method << '\n';
    }
    return std::cout ? 0 : 4;
  };
  if (argc == 2 && std::string(argv[1]) == "--dsl-schema") return print_schema_summary();
  if (argc == 3 && std::string(argv[1]) == "--dsl-schema" && std::string(argv[2]) == "--json")
    return print_embedded_schema();
  if (argc == 3 && std::string(argv[1]) == "--schema" && std::string(argv[2]) == "--json")
    return print_embedded_schema();
  if (argc == 2 && std::string(argv[1]) == "--schema") {
    const int result = print_embedded_schema();
    std::cout << '\n'; // Preserve the pre-R42 compatibility behavior.
    return result;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &signature : registry.signatures()) std::cout << signature << '\n';
    return 0;
  }
  if (argc != 3 || std::string(argv[1]) != "--dsl") {
    std::cerr << "usage: lmblender --version | --schema | --presets | --preset-show NAME | --dsl-signatures | --dsl BYTECODE.dslb\n";
    return 2;
  }
  try {
    const std::string schema(reinterpret_cast<const char *>(lmblender::embedded_schema),
                             lmblender::embedded_schema_size);
    const auto bytecode = dslvm::read_file(argv[2]);
    const auto program = dslvm::Program::load(bytecode, schema, registry);
    Scene scene;
    dslvm::Context context;
    context.set("scene", &scene);
    run_with_budget(program, context, registry);
    dslvm::report_ok();
    return 0;
  } catch (const dslvm::Error &error) {
    return dslvm::report_error("run", error);
  } catch (const std::exception &error) {
    return dslvm::report_app_failure(error.what());
  }
}
