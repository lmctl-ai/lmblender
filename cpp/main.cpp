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
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/pass.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "session/buffers.h"
#include "session/session.h"
#include "app/oiio_output_driver.h"
#include "app/cycles_xml.h"
#include "util/log.h"
#include "util/path.h"
#include "util/system.h"

namespace {
struct Mesh {
  std::vector<std::array<double, 3>> vertices;
  std::vector<std::array<uint32_t, 3>> triangles;
};
struct Scene { std::vector<std::unique_ptr<Mesh>> meshes; };

void render_png(const Scene &source, const std::string &output, int samples, int width, int height)
{
  if (source.meshes.empty()) throw dslvm::Error("render_png requires at least one imported mesh");
  if (output.empty() || output.front() == '-' || std::filesystem::path(output).extension() != ".png")
    throw dslvm::Error("render_png output must be a PNG path that does not begin with '-'");
  if (samples < 1 || samples > 65536 || width < 16 || width > 4096 || height < 16 || height > 4096 ||
      uint64_t(width) * uint64_t(height) > 16'000'000)
    throw dslvm::Error("render_png samples or dimensions exceed supported limits");
  const auto parent = std::filesystem::path(output).parent_path();
  std::error_code ec;
  if (!parent.empty() && !std::filesystem::is_directory(parent, ec))
    throw dslvm::Error("render_png output directory does not exist");

  std::array<double, 3> lo{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  std::array<double, 3> hi{-lo[0], -lo[1], -lo[2]};
  for (const auto &mesh : source.meshes) for (const auto &v : mesh->vertices)
    for (size_t axis = 0; axis < 3; ++axis) { lo[axis] = std::min(lo[axis], v[axis]); hi[axis] = std::max(hi[axis], v[axis]); }
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

  std::ostringstream xml;
  xml << "<cycles>\n"
      << "<transform translate=\"" << (center.x) << " " << (center.y - radius * 4.0f) << " " << center.z
      << "\" rotate=\"180 0 0.70710678 0.70710678\"><camera width=\"" << width << "\" height=\"" << height
      << "\" fov=\"0.8\"/></transform>\n"
      << "<transform translate=\"" << (center.x - radius * 2.5f) << " " << (center.y - radius * 3.2f) << " " << (center.z + radius * 4.0f)
      << "\"><light light_type=\"point\" strength=\"550\"/></transform>\n"
      << "<shader name=\"surface\"><emission name=\"emit\" color=\"0.62 0.66 0.72\" strength=\"1\"/>"
      << "<connect from=\"emit Emission\" to=\"output Surface\"/></shader>\n"
      << "<background color=\"0.16 0.18 0.22\" strength=\"0.45\"/>\n";
  size_t total_triangles = 0;
  for (const auto &source_mesh : source.meshes) {
    if (source_mesh->vertices.size() > 10'000'000 || source_mesh->triangles.size() > 20'000'000)
      throw dslvm::Error("render_png mesh exceeds Cycles geometry budget");
    total_triangles += source_mesh->triangles.size();
    if (total_triangles > 20'000'000) throw dslvm::Error("render_png combined triangle budget exceeded");
    xml << "<state shader=\"surface\"><mesh P=\"";
    for (const auto &v : source_mesh->vertices) xml << v[0] << " " << v[1] << " " << v[2] << " ";
    xml << "\" verts=\"";
    for (const auto &triangle : source_mesh->triangles) xml << triangle[0] << " " << triangle[1] << " " << triangle[2] << " ";
    xml << "\" nverts=\"";
    for (size_t i = 0; i < source_mesh->triangles.size(); ++i) xml << "3 ";
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

  const auto tmp = output + ".tmp-" + std::to_string(getpid()) + ".png";
  session->set_output_driver(ccl::make_unique<ccl::OIIOOutputDriver>(tmp, "combined", [](const ccl::string &) {}));
  ccl::BufferParams buffers;
  buffers.width = buffers.full_width = width;
  buffers.height = buffers.full_height = height;
  session->reset(params, buffers);
  session->start();
  session->wait();
  session.reset();
  std::filesystem::rename(tmp, output, ec);
  if (ec) {
    std::filesystem::remove(tmp);
    throw dslvm::Error("Cycles rendered the PNG but could not publish the output");
  }
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
  registry.method("Scene", "render_png", {"string", "int", "int", "int"}, "void", [](void *p, dslvm::Args &a) {
    const auto *scene = static_cast<const Scene *>(p);
    if (a[1].i < 1 || a[1].i > 65536 || a[2].i < 16 || a[2].i > 4096 || a[3].i < 16 || a[3].i > 4096)
      throw dslvm::Error("render_png samples and dimensions are outside supported bounds");
    render_png(*scene, a[0].s, int(a[1].i), int(a[2].i), int(a[3].i));
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
