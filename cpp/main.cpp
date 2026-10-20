#include "dslvm.hpp"
#include "schema_embedded.hpp"
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
#include <limits>
#include <cmath>
#include <chrono>
#include <unistd.h>

#include "device/device.h"
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
#include "app/cycles_xml.h"
#include "util/log.h"
#include "util/path.h"
#include "util/system.h"
#include "util/transform.h"

namespace {
struct Mesh {
  std::vector<std::array<double, 3>> vertices;
  std::vector<std::array<uint32_t, 3>> triangles;
  struct Material *material = nullptr;
  Mesh *geometry_source = nullptr;
  std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
  std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
  bool ground_plane = false;
};
struct Material {
  std::array<float, 3> color{0.8f, 0.8f, 0.8f};
  std::array<float, 3> emission{0.0f, 0.0f, 0.0f};
  float metallic = 0.0f;
  float roughness = 0.5f;
  float transmission = 0.0f;
  float emission_strength = 0.0f;
};
struct PointLightData {
  std::array<float, 3> position{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 100.0f;
};
struct AreaLightData {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  float strength = 100.0f;
  float size = 1.0f;
};
struct CameraConfig {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  float fov_radians = 0.785398163f;
};
struct Scene {
  std::vector<std::unique_ptr<Mesh>> meshes;
  std::vector<std::unique_ptr<Material>> materials;
  std::vector<PointLightData> point_lights;
  std::vector<AreaLightData> area_lights;
  std::array<float, 3> world_color{0.16f, 0.18f, 0.22f};
  float world_strength = 0.45f;
  bool custom_camera = false;
  CameraConfig camera;
  int render_timeout_seconds = 3600;
  int max_bounces = 12;
  int render_seed = 0;
  bool denoise = false;
};

template<typename SessionType> void run_session_with_timeout(SessionType *session, const int timeout_seconds, const std::string &temporary_output)
{
  std::atomic<bool> finished{false};
  std::atomic<bool> timed_out{false};
  std::thread watchdog([&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    while (!finished.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
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
  const auto parent = std::filesystem::path(output).parent_path();
  std::error_code ec;
  if (!parent.empty() && !std::filesystem::is_directory(parent, ec))
    throw dslvm::Error("render_png output directory does not exist");

  std::array<double, 3> lo{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  std::array<double, 3> hi{-lo[0], -lo[1], -lo[2]};
  for (const auto &mesh : source.meshes) if (!mesh->ground_plane) for (const auto &v : mesh->vertices)
    for (size_t axis = 0; axis < 3; ++axis) { lo[axis] = std::min(lo[axis], v[axis]); hi[axis] = std::max(hi[axis], v[axis]); }
  const ccl::float3 center = ccl::make_float3(float((lo[0] + hi[0]) * 0.5), float((lo[1] + hi[1]) * 0.5), float((lo[2] + hi[2]) * 0.5));
  const float radius = std::max(0.5f, float(std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]})) * 0.5f);
  const float fov = source.custom_camera ? source.camera.fov_radians : 0.8f;
  ccl::float3 camera_position;
  ccl::float3 camera_target;
  if (source.custom_camera) {
    camera_position = ccl::make_float3(source.camera.position[0], source.camera.position[1], source.camera.position[2]);
    camera_target = ccl::make_float3(source.camera.target[0], source.camera.target[1], source.camera.target[2]);
  } else {
    const float distance = radius * 1.85f / std::tan(fov * 0.5f);
    camera_position = center + ccl::make_float3(distance * 0.62f, -distance, distance * 0.8f);
    camera_target = center;
  }
  const ccl::float3 camera_forward = ccl::normalize(camera_target - camera_position);
  ccl::float3 camera_right = ccl::cross(camera_forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
  if (ccl::len_squared(camera_right) < 1.0e-8f) camera_right = ccl::cross(camera_forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
  camera_right = ccl::normalize(camera_right);
  const ccl::float3 camera_up = ccl::normalize(ccl::cross(camera_right, camera_forward));
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
  params.threads = 2;
  ccl::SceneParams scene_params;
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
  const auto scene_path = output + ".tmp-" + std::to_string(getpid()) + ".xml";
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

  const auto tmp = output + ".tmp-" + std::to_string(getpid()) + extension;
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

void render_direct(const Scene &source, const std::string &output, int samples, int width, int height)
{
  if (source.meshes.empty()) throw dslvm::Error("render_png requires at least one imported mesh");
  const std::string extension = std::filesystem::path(output).extension().string();
  if (output.empty() || output.front() == '-' || (extension != ".png" && extension != ".exr"))
    throw dslvm::Error("render output must be a PNG or EXR path that does not begin with '-'");
  if (samples < 1 || samples > 65536 || width < 16 || width > 4096 || height < 16 || height > 4096 ||
      uint64_t(width) * uint64_t(height) > 16'000'000)
    throw dslvm::Error("render_png samples or dimensions exceed supported limits");
  const auto parent = std::filesystem::path(output).parent_path();
  std::error_code ec;
  if (!parent.empty() && !std::filesystem::is_directory(parent, ec))
    throw dslvm::Error("render_png output directory does not exist");

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
  params.threads = 2;
  ccl::SceneParams scene_params;
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
  auto make_principled_shader = [scene](const Material &material) {
    ccl::Shader *shader = scene->create_node<ccl::Shader>();
    auto graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::PrincipledBsdfNode *principled = graph->create_node<ccl::PrincipledBsdfNode>();
    principled->set_base_color(ccl::make_float3(material.color[0], material.color[1], material.color[2]));
    principled->set_metallic(material.metallic);
    principled->set_roughness(material.roughness);
    principled->set_transmission_weight(material.transmission);
    principled->set_emission_color(ccl::make_float3(material.emission[0], material.emission[1], material.emission[2]));
    principled->set_emission_strength(material.emission_strength);
    graph->connect(principled->output("BSDF"), graph->output()->input("Surface"));
    shader->set_graph(std::move(graph));
    shader->tag_update(scene);
    return shader;
  };
  ccl::Shader *default_surface = make_emission_shader(ccl::make_float3(0.62f, 0.66f, 0.72f), 1.0f);
  ccl::Shader *world = scene->default_background;
  auto world_graph = ccl::make_unique<ccl::ShaderGraph>();
  ccl::BackgroundNode *background = world_graph->create_node<ccl::BackgroundNode>();
  background->set_color(ccl::make_float3(source.world_color[0], source.world_color[1], source.world_color[2]));
  background->set_strength(source.world_strength);
  world_graph->connect(background->output("Background"), world_graph->output()->input("Surface"));
  world->set_graph(std::move(world_graph));
  world->tag_update(scene);

  for (const auto &source_mesh : source.meshes) {
    const Mesh *mesh_data = source_mesh->geometry_source ? source_mesh->geometry_source : source_mesh.get();
    ccl::Shader *surface = mesh_data->material ? make_principled_shader(*mesh_data->material) : default_surface;
    ccl::array<ccl::Node *> used_shaders;
    used_shaders.push_back_slow(surface);
    ccl::Mesh *mesh = scene->create_node<ccl::Mesh>();
    mesh->resize_mesh(int(mesh_data->vertices.size()), int(mesh_data->triangles.size()));
    ccl::packed_float3 *positions = mesh->get_position_for_write();
    for (size_t i = 0; i < mesh_data->vertices.size(); ++i) {
      const auto &v = mesh_data->vertices[i];
      positions[i] = ccl::make_float3(float(v[0] * source_mesh->scale[0] + source_mesh->translation[0]),
                                      float(v[1] * source_mesh->scale[1] + source_mesh->translation[1]),
                                      float(v[2] * source_mesh->scale[2] + source_mesh->translation[2]));
    }
    ccl::array<int> &tris = mesh->get_triangles();
    for (size_t i = 0; i < mesh_data->triangles.size(); ++i)
      for (size_t j = 0; j < 3; ++j) tris[i * 3 + j] = int(mesh_data->triangles[i][j]);
    std::fill(mesh->get_shader().begin(), mesh->get_shader().end(), 0);
    std::fill(mesh->get_smooth().begin(), mesh->get_smooth().end(), false);
    mesh->set_used_shaders(used_shaders);
    mesh->tag_triangles_modified();
    mesh->tag_shader_modified();
    mesh->tag_smooth_modified();
    ccl::Object *object = scene->create_node<ccl::Object>();
    object->set_geometry(mesh);
    object->set_tfm(ccl::transform_identity());
  }

  auto add_point_light = [scene](const ccl::float3 position, const ccl::float3 color, const float strength) {
    ccl::PointLight *point = scene->create_node<ccl::PointLight>();
    point->set_strength(color * strength);
    point->set_radius(0.0f);
    ccl::Shader *light_shader = scene->create_node<ccl::Shader>();
    auto light_graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::EmissionNode *light_emission = light_graph->create_node<ccl::EmissionNode>();
    light_emission->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));
    light_emission->set_strength(1.0f);
    light_graph->connect(light_emission->output("Emission"), light_graph->output()->input("Surface"));
    light_shader->set_graph(std::move(light_graph));
    light_shader->tag_update(scene);
    ccl::array<ccl::Node *> light_shaders; light_shaders.push_back_slow(light_shader); point->set_used_shaders(light_shaders);
    ccl::Object *object = scene->create_node<ccl::Object>();
    object->set_geometry(point);
    ccl::Transform tfm = ccl::transform_translate(position);
    object->set_tfm(tfm);
  };
  auto add_area_light = [scene](const AreaLightData &light) {
    ccl::AreaLight *area = scene->create_node<ccl::AreaLight>();
    area->set_strength(ccl::make_float3(light.color[0], light.color[1], light.color[2]) * light.strength);
    area->set_sizeu(light.size);
    area->set_sizev(light.size);
    ccl::Shader *light_shader = scene->create_node<ccl::Shader>();
    auto light_graph = ccl::make_unique<ccl::ShaderGraph>();
    ccl::EmissionNode *light_emission = light_graph->create_node<ccl::EmissionNode>();
    light_emission->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f)); light_emission->set_strength(1.0f);
    light_graph->connect(light_emission->output("Emission"), light_graph->output()->input("Surface"));
    light_shader->set_graph(std::move(light_graph)); light_shader->tag_update(scene);
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
  if (source.point_lights.empty()) {
    add_point_light(ccl::make_float3(center.x - radius * 2.5f, center.y - radius * 3.2f, center.z + radius * 4.0f),
                    ccl::make_float3(1.0f, 1.0f, 1.0f), 550.0f);
  } else {
    for (const PointLightData &light : source.point_lights)
      add_point_light(ccl::make_float3(light.position[0], light.position[1], light.position[2]),
                      ccl::make_float3(light.color[0], light.color[1], light.color[2]), light.strength);
  }
  for (const AreaLightData &light : source.area_lights) add_area_light(light);

  scene->camera->set_full_width(width);
  scene->camera->set_full_height(height);
  scene->camera->set_fov(source.custom_camera ? source.camera.fov_radians : 0.8f);
  scene->camera->set_camera_type(ccl::CAMERA_PERSPECTIVE);
  scene->camera->set_nearclip(0.01f);
  scene->camera->set_farclip(10000.0f);
  ccl::Transform camera_tfm;
  if (!source.custom_camera) {
    const float distance = radius * 1.85f / std::tan(0.8f * 0.5f);
    const ccl::float3 p = center + ccl::make_float3(distance * 0.62f, -distance, distance * 0.8f);
    const ccl::float3 forward = ccl::normalize(center - p);
    ccl::float3 right = ccl::cross(forward, ccl::make_float3(0.0f, 0.0f, 1.0f));
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::cross(forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
    right = ccl::normalize(right);
    const ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
    camera_tfm = ccl::make_transform(right.x, up.x, forward.x, p.x,
                                     right.y, up.y, forward.y, p.y,
                                     right.z, up.z, forward.z, p.z);
  } else {
    const auto &p = source.camera.position;
    const auto &t = source.camera.target;
    ccl::float3 forward = ccl::normalize(ccl::make_float3(t[0] - p[0], t[1] - p[1], t[2] - p[2]));
    ccl::float3 up_ref = ccl::make_float3(0.0f, 0.0f, 1.0f);
    ccl::float3 right = ccl::cross(forward, up_ref);
    if (ccl::len_squared(right) < 1.0e-8f) right = ccl::cross(forward, ccl::make_float3(0.0f, 1.0f, 0.0f));
    right = ccl::normalize(right);
    ccl::float3 up = ccl::normalize(ccl::cross(right, forward));
    camera_tfm = ccl::make_transform(right.x, up.x, forward.x, p[0],
                                     right.y, up.y, forward.y, p[1],
                                     right.z, up.z, forward.z, p[2]);
  }
  scene->camera->set_matrix(camera_tfm);
  scene->camera->need_flags_update = true;
  scene->camera->update(scene);
  scene->integrator->set_max_bounce(source.max_bounces);
  scene->integrator->set_seed(source.render_seed);
  scene->integrator->set_use_denoise(source.denoise);

  if (scene->passes.empty()) throw dslvm::Error("Cycles did not initialize its combined render pass");
  scene->passes[0]->set_name(ccl::ustring("combined"));
  const auto tmp = output + ".tmp-" + std::to_string(getpid()) + extension;
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

void render_image(const Scene &source, const std::string &output, int samples, int width, int height)
{
  if (std::getenv("LMBLENDER_TEST_XML_ORACLE"))
    render_xml_oracle(source, output, samples, width, height);
  else
    render_direct(source, output, samples, width, height);
}

Mesh read_obj(const std::string &path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) throw dslvm::Error("OBJ input must be a readable regular file");
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > 256u * 1024u * 1024u) throw dslvm::Error("OBJ input exceeds the 256 MiB limit or cannot be read");
  std::ifstream in(path);
  if (!in) throw dslvm::Error("could not open OBJ input");
  Mesh mesh;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    std::string kind;
    row >> kind;
    if (kind == "v") {
      std::array<double, 3> p{};
      if (!(row >> p[0] >> p[1] >> p[2]) || !std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
        throw dslvm::Error("OBJ contains an invalid vertex");
      mesh.vertices.push_back(p);
      if (mesh.vertices.size() > 10'000'000) throw dslvm::Error("OBJ vertex budget exceeded");
    } else if (kind == "f") {
      std::vector<uint32_t> face;
      std::string token;
      while (row >> token) {
        const auto slash = token.find('/');
        const std::string index_text = token.substr(0, slash);
        if (index_text.empty()) throw dslvm::Error("OBJ face has no vertex index");
        long long index;
        try { index = std::stoll(index_text); } catch (...) { throw dslvm::Error("OBJ face has an invalid vertex index"); }
        if (index < 0) index = static_cast<long long>(mesh.vertices.size()) + index;
        else --index;
        if (index < 0 || static_cast<size_t>(index) >= mesh.vertices.size()) throw dslvm::Error("OBJ face index is out of range");
        face.push_back(static_cast<uint32_t>(index));
      }
      if (face.size() < 3) throw dslvm::Error("OBJ face must have at least three vertices");
      for (size_t i = 1; i + 1 < face.size(); ++i) mesh.triangles.push_back({face[0], face[i], face[i + 1]});
      if (mesh.triangles.size() > 20'000'000) throw dslvm::Error("OBJ triangle budget exceeded");
    }
  }
  if (!in.eof()) throw dslvm::Error("error reading OBJ input");
  if (mesh.vertices.empty() || mesh.triangles.empty()) throw dslvm::Error("OBJ contains no triangle mesh");
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

dslvm::Registry make_registry() {
  dslvm::Registry registry;
  registry.context("scene", "Scene");
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
    scene->custom_camera = true;
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
    const float distance = radius * 1.85f / std::tan(float(fov * M_PI / 360.0));
    scene->camera.position = {cx + distance * 0.62f, cy - distance, cz + distance * 0.8f};
    scene->camera.target = {cx, cy, cz};
    scene->camera.fov_radians = float(fov * M_PI / 180.0);
    scene->custom_camera = true;
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_timeout", {"int"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].i < 1 || a[0].i > 3600) throw dslvm::Error("render timeout must be in [1,3600] seconds");
    static_cast<Scene *>(p)->render_timeout_seconds = int(a[0].i);
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_settings", {"int", "int", "bool"}, "void", [](void *p, dslvm::Args &a) {
    if (a[0].i < 0 || a[0].i > 64) throw dslvm::Error("max bounces must be in [0,64]");
    if (a[1].i < 0 || a[1].i > 2'147'483'647) throw dslvm::Error("render seed must be in [0,2147483647]");
    if (a[2].b) throw dslvm::Error("denoising is unsupported in this standalone Cycles build; pass false");
    auto *scene = static_cast<Scene *>(p);
    scene->max_bounces = int(a[0].i);
    scene->render_seed = int(a[1].i);
    scene->denoise = a[2].b;
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
    scene->point_lights.push_back(light);
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
    light.strength = float(a[9].f); light.size = float(a[10].f);
    scene->area_lights.push_back(light);
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
    scene->meshes.push_back(std::move(mesh));
    return dslvm::Value::none();
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
    scene->meshes.push_back(std::move(mesh));
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
  auto import = [](void *p, dslvm::Args &args, bool stl) {
    auto *scene = static_cast<Scene *>(p);
    if (args[0].s.empty()) throw dslvm::Error("mesh input path must not be empty");
    if (scene->meshes.size() >= 4096) throw dslvm::Error("mesh object limit exceeded (4096)");
    auto mesh = std::make_unique<Mesh>(stl ? read_stl(args[0].s) : read_obj(args[0].s));
    Mesh *result = mesh.get();
    scene->meshes.push_back(std::move(mesh));
    return dslvm::Value::object("Mesh", result);
  };
  registry.method("Scene", "import_obj", {"string"}, "Mesh", [import](void *p, dslvm::Args &a) { return import(p, a, false); });
  registry.method("Scene", "import_stl", {"string"}, "Mesh", [import](void *p, dslvm::Args &a) { return import(p, a, true); });
  registry.method("Mesh", "set_material", {"Material"}, "void", [](void *p, dslvm::Args &a) {
    auto *mesh = static_cast<Mesh *>(p);
    if (!a[0].obj) throw dslvm::Error("set_material requires a material created by this scene");
    mesh->material = static_cast<Material *>(a[0].obj);
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
  registry.method("Mesh", "rotate_x", {"float"}, "void", [](void *p, dslvm::Args &a) {
    if (!std::isfinite(a[0].f) || std::abs(a[0].f) > 360000.0) throw dslvm::Error("mesh X rotation must be finite and bounded");
    auto *mesh = static_cast<Mesh *>(p);
    const double angle = a[0].f * M_PI / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    auto rotate = [c,s](std::array<double,3> &v) { const double y=v[1], z=v[2]; v[1]=c*y-s*z; v[2]=s*y+c*z; };
    for (auto &v : mesh->vertices) rotate(v);
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
    for (size_t i = 0; i < 6; ++i) if (!std::isfinite(a[i + 1].f)) throw dslvm::Error("instance transform must be finite");
    for (size_t i = 0; i < 3; ++i) {
      if (std::abs(a[i + 1].f) > 1'000'000.0) throw dslvm::Error("instance position exceeds scene bounds");
      if (std::abs(a[i + 4].f) < 0.001 || std::abs(a[i + 4].f) > 1000.0) throw dslvm::Error("instance scale must have magnitude in [0.001,1000]");
      instance->translation[i] = float(a[i + 1].f);
      instance->scale[i] = float(a[i + 4].f);
    }
    Mesh *result = instance.get();
    scene->meshes.push_back(std::move(instance));
    return dslvm::Value::object("Mesh", result);
  });
  registry.method("Scene", "render_png", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    const auto *scene = static_cast<const Scene *>(p);
    if (std::filesystem::path(a[0].s).extension() != ".png") throw dslvm::Error("render_png output must use the .png extension");
    if (a[1].i < 1 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096)
      throw dslvm::Error("render_png samples and dimensions are outside supported bounds");
    render_image(*scene, a[0].s, int(a[1].i), int(a[2].i), int(a[3].i));
    return dslvm::Value::none();
  });
  registry.method("Scene", "render_exr", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    auto *scene = static_cast<Scene *>(p);
    if (std::filesystem::path(a[0].s).extension() != ".exr") throw dslvm::Error("render_exr output must use the .exr extension");
    if (a[1].i < 1 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096)
      throw dslvm::Error("render_exr samples and dimensions are outside supported bounds");
    render_image(*scene, a[0].s, int(a[1].i), int(a[2].i), int(a[3].i));
    return dslvm::Value::none();
  });
  for (const char *field : {"vertex_count", "triangle_count"}) {
    registry.method("Mesh", field, {}, "int", [field](void *p, dslvm::Args &) {
      auto *mesh = static_cast<Mesh *>(p);
      return dslvm::Value::integer(static_cast<int64_t>(std::string(field) == "vertex_count" ? mesh->vertices.size() : mesh->triangles.size()));
    });
  }
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
    return dslvm::Value::string("lmblender 0.1.0 (DSL bytecode v5)");
  });
  return registry;
}
}

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--schema") {
    std::cout.write(reinterpret_cast<const char *>(lmblender::embedded_schema),
                     static_cast<std::streamsize>(lmblender::embedded_schema_size));
    std::cout << '\n';
    return 0;
  }
  auto registry = make_registry();
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &signature : registry.signatures()) std::cout << signature << '\n';
    return 0;
  }
  if (argc != 3 || std::string(argv[1]) != "--dsl") {
    std::cerr << "usage: lmblender --schema | --dsl-signatures | --dsl BYTECODE.dslb\n";
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
    program.run(context, registry);
    dslvm::report_ok();
    return 0;
  } catch (const dslvm::Error &error) {
    return dslvm::report_error("run", error);
  } catch (const std::exception &error) {
    return dslvm::report_app_failure(error.what());
  }
}
