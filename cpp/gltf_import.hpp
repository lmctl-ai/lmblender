#pragma once

namespace gltf_import {
using Json = nlohmann::json;
using Bytes = std::vector<unsigned char>;
using Matrix = std::array<double, 16>; // glTF column-major

[[noreturn]] inline void fail(const char *code, const std::string &message)
{
  throw dslvm::Error(code, message);
}

inline uint32_t u32(const unsigned char *p)
{
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

inline Bytes read_bytes(const std::filesystem::path &path, const uintmax_t max_size, const char *kind)
{
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) fail("GLTF_INVALID", std::string(kind) + " must be a readable regular file");
  const uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size == 0 || size > max_size) fail("GLTF_LIMIT", std::string(kind) + " exceeds its byte limit or is empty");
  std::ifstream in(path, std::ios::binary);
  if (!in) fail("GLTF_INVALID", std::string("could not open ") + kind);
  Bytes data(static_cast<size_t>(size));
  in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!in) fail("GLTF_INVALID", std::string("could not read ") + kind);
  return data;
}

inline std::filesystem::path confined_uri(const std::filesystem::path &root, const std::string &uri,
                                          const uintmax_t max_size, const char *kind)
{
  if (uri.empty() || uri.front() == '-' || uri.find('%') != std::string::npos || uri.find('\\') != std::string::npos ||
      uri.find('?') != std::string::npos || uri.find('#') != std::string::npos || uri.rfind("data:", 0) == 0)
    fail("GLTF_UNSUPPORTED", std::string(kind) + " URI must be a plain relative file path");
  const std::filesystem::path rel(uri);
  if (rel.is_absolute()) fail("GLTF_INVALID", std::string(kind) + " URI must remain within the glTF directory");
  for (const auto &part : rel) if (part == "..") fail("GLTF_INVALID", std::string(kind) + " URI traversal is forbidden");
  std::error_code ec;
  const auto resolved = std::filesystem::canonical(root / rel, ec);
  if (ec) fail("GLTF_INVALID", std::string(kind) + " file is missing or unreadable");
  const auto canonical_root = std::filesystem::canonical(root, ec);
  if (ec) fail("GLTF_INVALID", "glTF asset directory is unavailable");
  auto root_it = canonical_root.begin(), file_it = resolved.begin();
  for (; root_it != canonical_root.end() && file_it != resolved.end() && *root_it == *file_it; ++root_it, ++file_it) {}
  if (root_it != canonical_root.end()) fail("GLTF_INVALID", std::string(kind) + " URI escapes the glTF directory");
  if (!std::filesystem::is_regular_file(resolved, ec) || ec) fail("GLTF_INVALID", std::string(kind) + " must be a regular file");
  const uintmax_t size = std::filesystem::file_size(resolved, ec);
  if (ec || size == 0 || size > max_size) fail("GLTF_LIMIT", std::string(kind) + " exceeds its byte limit or is empty");
  return resolved;
}

inline Matrix identity()
{
  Matrix m{};
  m[0] = m[5] = m[10] = m[15] = 1.0;
  return m;
}

inline Matrix multiply(const Matrix &a, const Matrix &b)
{
  Matrix out{};
  for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
    for (int k = 0; k < 4; ++k) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return out;
}

inline std::array<double, 3> transform_point(const Matrix &m, const std::array<double, 3> &p)
{
  const double x = m[0]*p[0] + m[4]*p[1] + m[8]*p[2] + m[12];
  const double y = m[1]*p[0] + m[5]*p[1] + m[9]*p[2] + m[13];
  const double z = m[2]*p[0] + m[6]*p[1] + m[10]*p[2] + m[14];
  const double w = m[3]*p[0] + m[7]*p[1] + m[11]*p[2] + m[15];
  if (!std::isfinite(w) || std::abs(w) < 1e-12) fail("GLTF_INVALID", "node transform has invalid homogeneous coordinates");
  std::array<double, 3> result{x/w, z/w, -y/w}; // right-handed Y-up glTF to Z-up renderer
  for (double v : result) if (!std::isfinite(v) || std::abs(v) > 1'000'000.0) fail("GLTF_LIMIT", "transformed glTF vertex is outside scene bounds");
  return result;
}

inline Matrix node_matrix(const Json &node)
{
  if (node.contains("matrix")) {
    const Json &items = node.at("matrix");
    if (!items.is_array() || items.size() != 16) fail("GLTF_INVALID", "node matrix must contain 16 numbers");
    Matrix result{};
    for (size_t i = 0; i < 16; ++i) {
      if (!items[i].is_number()) fail("GLTF_INVALID", "node matrix has a non-number value");
      result[i] = items[i].get<double>();
      if (!std::isfinite(result[i])) fail("GLTF_INVALID", "node matrix contains a non-finite value");
    }
    return result;
  }
  std::array<double, 3> t{0,0,0}, s{1,1,1};
  std::array<double, 4> q{0,0,0,1};
  auto read_array = [](const Json &node, const char *key, auto &out) {
    if (!node.contains(key)) return;
    const Json &values = node.at(key);
    if (!values.is_array() || values.size() != out.size()) fail("GLTF_INVALID", std::string("node ") + key + " has invalid length");
    for (size_t i = 0; i < out.size(); ++i) {
      if (!values[i].is_number()) fail("GLTF_INVALID", std::string("node ") + key + " contains a non-number");
      out[i] = values[i].get<double>();
      if (!std::isfinite(out[i])) fail("GLTF_INVALID", std::string("node ") + key + " contains a non-finite value");
    }
  };
  read_array(node, "translation", t);
  read_array(node, "rotation", q);
  read_array(node, "scale", s);
  const double qn = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
  if (qn < 1e-12) fail("GLTF_INVALID", "node quaternion has zero length");
  for (double &v : q) v /= qn;
  const double x=q[0], y=q[1], z=q[2], w=q[3];
  Matrix m = identity();
  m[0]=(1-2*(y*y+z*z))*s[0]; m[1]=(2*(x*y+w*z))*s[0]; m[2]=(2*(x*z-w*y))*s[0];
  m[4]=(2*(x*y-w*z))*s[1]; m[5]=(1-2*(x*x+z*z))*s[1]; m[6]=(2*(y*z+w*x))*s[1];
  m[8]=(2*(x*z+w*y))*s[2]; m[9]=(2*(y*z-w*x))*s[2]; m[10]=(1-2*(x*x+y*y))*s[2];
  m[12]=t[0]; m[13]=t[1]; m[14]=t[2];
  return m;
}

inline double determinant3(const Matrix &m)
{
  return m[0]*(m[5]*m[10]-m[9]*m[6]) - m[4]*(m[1]*m[10]-m[9]*m[2]) + m[8]*(m[1]*m[6]-m[5]*m[2]);
}

struct BufferData { Bytes bytes; };

inline size_t component_size(const int type)
{
  if (type == 5121) return 1;
  if (type == 5123) return 2;
  if (type == 5125 || type == 5126) return 4;
  fail("GLTF_UNSUPPORTED", "accessor component type is unsupported");
}

inline size_t type_components(const std::string &type)
{
  if (type == "SCALAR") return 1;
  if (type == "VEC2") return 2;
  if (type == "VEC3") return 3;
  if (type == "VEC4") return 4;
  fail("GLTF_UNSUPPORTED", "matrix or unknown accessor type is unsupported");
}

struct AccessorData {
  const Bytes *buffer = nullptr;
  size_t offset = 0;
  size_t stride = 0;
  size_t count = 0;
  int component_type = 0;
  size_t components = 0;
};

inline AccessorData accessor(const Json &root, const std::vector<BufferData> &buffers, const size_t index,
                             const std::string &expected_type, const int expected_component)
{
  if (!root.contains("accessors") || !root.at("accessors").is_array() || index >= root.at("accessors").size())
    fail("GLTF_INVALID", "accessor index is out of range");
  const Json &a = root.at("accessors")[index];
  if (a.contains("sparse")) fail("GLTF_UNSUPPORTED", "sparse accessors are not supported");
  if (!a.contains("bufferView") || !a.contains("count") || !a.contains("type") || !a.contains("componentType"))
    fail("GLTF_INVALID", "accessor is missing required fields");
  if (!a.at("type").is_string() || a.at("type").get<std::string>() != expected_type)
    fail("GLTF_INVALID", "accessor has an unexpected element type");
  const int ct = a.at("componentType").get<int>();
  if (expected_component && ct != expected_component) fail("GLTF_UNSUPPORTED", "accessor component type is unsupported for this attribute");
  const size_t components = type_components(expected_type), cs = component_size(ct);
  const size_t count = a.at("count").get<size_t>();
  if (count == 0 || count > 10'000'000) fail("GLTF_LIMIT", "accessor element count exceeds the 10 million limit");
  const size_t view_index = a.at("bufferView").get<size_t>();
  if (!root.contains("bufferViews") || !root.at("bufferViews").is_array() || view_index >= root.at("bufferViews").size())
    fail("GLTF_INVALID", "bufferView index is out of range");
  const Json &view = root.at("bufferViews")[view_index];
  const size_t buffer_index = view.value("buffer", size_t(0));
  if (buffer_index >= buffers.size()) fail("GLTF_INVALID", "bufferView buffer index is out of range");
  const size_t view_offset = view.value("byteOffset", size_t(0));
  const size_t view_length = view.at("byteLength").get<size_t>();
  const size_t accessor_offset = a.value("byteOffset", size_t(0));
  const size_t elem_size = cs * components;
  const size_t stride = view.value("byteStride", elem_size);
  if (stride < elem_size || stride > 252 || accessor_offset > view_length || (count - 1) > (view_length - accessor_offset) / stride || elem_size > view_length - accessor_offset - (count - 1) * stride)
    fail("GLTF_INVALID", "accessor byte range exceeds its bufferView");
  if (view_offset > buffers[buffer_index].bytes.size() || view_length > buffers[buffer_index].bytes.size() - view_offset)
    fail("GLTF_INVALID", "bufferView byte range exceeds its buffer");
  AccessorData result{&buffers[buffer_index].bytes, view_offset + accessor_offset, stride, count, ct, components};
  return result;
}

inline double read_component(const AccessorData &a, const size_t element, const size_t component)
{
  const size_t offset = a.offset + element * a.stride + component * component_size(a.component_type);
  const unsigned char *p = a.buffer->data() + offset;
  double value;
  if (a.component_type == 5126) { float f; std::memcpy(&f, p, sizeof(f)); value = f; }
  else if (a.component_type == 5125) value = u32(p);
  else if (a.component_type == 5123) value = uint32_t(p[0]) | uint32_t(p[1]) << 8;
  else value = p[0];
  if (!std::isfinite(value)) fail("GLTF_INVALID", "accessor contains a non-finite value");
  return value;
}

inline std::filesystem::path gltf_image_path(const Json &root, const std::filesystem::path &asset_root,
                                              const size_t texture_index)
{
  if (!root.contains("textures") || !root.at("textures").is_array() || texture_index >= root.at("textures").size())
    fail("GLTF_INVALID", "baseColorTexture index is out of range");
  const Json &texture = root.at("textures")[texture_index];
  if (!texture.contains("source")) fail("GLTF_UNSUPPORTED", "texture without an image source is unsupported");
  const size_t image_index = texture.at("source").get<size_t>();
  if (!root.contains("images") || !root.at("images").is_array() || image_index >= root.at("images").size())
    fail("GLTF_INVALID", "texture image index is out of range");
  const Json &image = root.at("images")[image_index];
  if (!image.contains("uri") || !image.at("uri").is_string())
    fail("GLTF_UNSUPPORTED", "embedded bufferView and data-URI images are unsupported; use an external PNG/JPEG URI");
  const std::string uri = image.at("uri").get<std::string>();
  const std::string ext = std::filesystem::path(uri).extension().string();
  if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") fail("GLTF_UNSUPPORTED", "baseColorTexture supports external PNG and JPEG files only");
  const auto path = confined_uri(asset_root, uri, 64u * 1024u * 1024u, "image");
  const Bytes bytes = read_bytes(path, 64u * 1024u * 1024u, "glTF image");
  const bool png = bytes.size() >= 8 && std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0;
  const bool jpeg = bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff;
  if ((ext == ".png" && !png) || ((ext == ".jpg" || ext == ".jpeg") && !jpeg))
    fail("GLTF_INVALID", "baseColorTexture signature does not match its extension");
  return path;
}

inline Material *make_material(Scene &scene, const Json &root, const int material_index,
                               const std::filesystem::path &asset_root)
{
  auto material = std::make_unique<Material>();
  material->color = {1.0f,1.0f,1.0f};
  material->metallic = 1.0f;
  material->roughness = 1.0f;
  if (material_index >= 0) {
    if (!root.contains("materials") || !root.at("materials").is_array() || size_t(material_index) >= root.at("materials").size())
      fail("GLTF_INVALID", "primitive material index is out of range");
    const Json &source = root.at("materials")[material_index];
    if (source.contains("extensions")) fail("GLTF_UNSUPPORTED", "glTF material extensions are not supported");
    const Json pbr = source.value("pbrMetallicRoughness", Json::object());
    if (pbr.contains("baseColorFactor")) {
      const Json &factor = pbr.at("baseColorFactor");
      if (!factor.is_array() || factor.size() != 4) fail("GLTF_INVALID", "baseColorFactor must contain four numbers");
      for (int i=0;i<3;++i) {
        if (!factor[i].is_number()) fail("GLTF_INVALID", "baseColorFactor contains a non-number");
        const double v = factor[i].get<double>();
        if (!std::isfinite(v) || v < 0.0 || v > 1.0) fail("GLTF_INVALID", "baseColorFactor components must be in [0,1]");
        material->color[i] = float(v);
      }
    }
    material->metallic = pbr.value("metallicFactor", 1.0f);
    material->roughness = pbr.value("roughnessFactor", 1.0f);
    if (!std::isfinite(material->metallic) || material->metallic < 0 || material->metallic > 1 ||
        !std::isfinite(material->roughness) || material->roughness < 0 || material->roughness > 1)
      fail("GLTF_INVALID", "metallicFactor and roughnessFactor must be in [0,1]");
    if (pbr.contains("baseColorTexture")) {
      const Json &texture = pbr.at("baseColorTexture");
      if (texture.value("texCoord", 0) != 0) fail("GLTF_UNSUPPORTED", "only TEXCOORD_0 base-color textures are supported");
      const size_t texture_index = texture.at("index").get<size_t>();
      material->texture_kind = 3;
      material->image_path = std::filesystem::canonical(gltf_image_path(root, asset_root, texture_index)).string();
      material->image_factor_enabled = true;
    }
  }
  if (scene.materials.size() >= 4096) fail("GLTF_LIMIT", "glTF material limit exceeded (4096)");
  Material *result = material.get();
  scene.materials.push_back(std::move(material));
  return result;
}

inline void append_primitive(Scene &scene, const Json &root, const std::vector<BufferData> &buffers,
                             const Json &primitive, const Matrix &world, size_t &total_vertices, size_t &total_triangles,
                             const std::filesystem::path &asset_root, Mesh *&first_mesh)
{
  if (primitive.value("mode", 4) != 4) fail("GLTF_UNSUPPORTED", "only TRIANGLES glTF primitives are supported");
  if (primitive.contains("targets")) fail("GLTF_UNSUPPORTED", "morph targets are not supported");
  if (!primitive.contains("attributes") || !primitive.at("attributes").is_object() || !primitive.at("attributes").contains("POSITION"))
    fail("GLTF_INVALID", "glTF primitive has no POSITION attribute");
  const Json &attributes = primitive.at("attributes");
  const size_t position_index = attributes.at("POSITION").get<size_t>();
  const AccessorData positions = accessor(root,buffers,position_index,"VEC3",5126);
  if (total_vertices + positions.count > 10'000'000) fail("GLTF_LIMIT", "glTF decoded vertex budget exceeded (10 million)");
  total_vertices += positions.count;
  AccessorData uvs;
  const bool has_uv = attributes.contains("TEXCOORD_0");
  if (has_uv) {
    uvs = accessor(root,buffers,attributes.at("TEXCOORD_0").get<size_t>(),"VEC2",5126);
    if (uvs.count != positions.count) fail("GLTF_INVALID", "TEXCOORD_0 count differs from POSITION count");
  }
  std::vector<uint32_t> indices;
  if (primitive.contains("indices")) {
    const AccessorData idx = accessor(root,buffers,primitive.at("indices").get<size_t>(),"SCALAR",0);
    if (idx.component_type != 5121 && idx.component_type != 5123 && idx.component_type != 5125)
      fail("GLTF_UNSUPPORTED", "indices must use unsigned byte, short, or int components");
    if (idx.count % 3 != 0 || idx.count > 60'000'000) fail("GLTF_INVALID", "triangle index accessor has invalid length");
    indices.reserve(idx.count);
    for (size_t i=0;i<idx.count;++i) {
      const double v=read_component(idx,i,0);
      if (v >= positions.count) fail("GLTF_INVALID", "triangle index is out of range");
      indices.push_back(uint32_t(v));
    }
  } else {
    if (positions.count % 3 != 0) fail("GLTF_INVALID", "non-indexed triangle vertex count is not divisible by three");
    indices.resize(positions.count);
    for (size_t i=0;i<positions.count;++i) indices[i]=uint32_t(i);
  }
  if (indices.empty() || indices.size()/3 > 20'000'000 || total_triangles + indices.size()/3 > 20'000'000)
    fail("GLTF_LIMIT", "glTF triangle budget exceeded (20 million)");
  total_triangles += indices.size()/3;
  auto mesh=std::make_unique<Mesh>();
  mesh->smooth=true;
  mesh->vertices.reserve(positions.count);
  for (size_t i=0;i<positions.count;++i) {
    const std::array<double,3> p{read_component(positions,i,0),read_component(positions,i,1),read_component(positions,i,2)};
    mesh->vertices.push_back(transform_point(world,p));
  }
  Material *material = nullptr;
  if (primitive.contains("material")) material=make_material(scene,root,primitive.at("material").get<int>(),asset_root);
  else material=make_material(scene,root,-1,asset_root);
  mesh->material=material;
  const bool mirrored = determinant3(world) < 0;
  mesh->triangles.reserve(indices.size()/3);
  for(size_t i=0;i<indices.size();i+=3) {
    uint32_t a=indices[i],b=indices[i+1],c=indices[i+2];
    if(mirrored)std::swap(b,c);
    mesh->triangles.push_back({a,b,c});
    if(has_uv) for(uint32_t index : {a,b,c}) mesh->uvs.push_back({float(read_component(uvs,index,0)),1.0f-float(read_component(uvs,index,1))});
  }
  if(scene.meshes.size()>=4096)fail("GLTF_LIMIT","glTF object limit exceeded (4096)");
  Mesh *result=mesh.get();
  scene.meshes.push_back(std::move(mesh));
  if(!first_mesh)first_mesh=result;
}

inline Mesh *read(Scene &scene, const std::string &filename)
{
  if(filename.empty()||filename.front()=='-')fail("GLTF_INVALID","glTF path must be non-empty and must not begin with '-'");
  const std::filesystem::path input(filename);
  const std::string ext=input.extension().string();
  if(ext!=".gltf"&&ext!=".glb")fail("GLTF_INVALID","glTF input extension must be .gltf or .glb");
  const Bytes source=read_bytes(input,256u*1024u*1024u,"glTF input");
  Json root;
  Bytes glb_binary;
  if(ext==".glb") {
    if(source.size()<20||u32(source.data())!=0x46546c67||u32(source.data()+4)!=2||u32(source.data()+8)!=source.size())
      fail("GLTF_INVALID","invalid GLB magic, version, or total length");
    size_t offset=12; bool json_seen=false;
    while(offset+8<=source.size()) {
      const uint32_t length=u32(source.data()+offset), type=u32(source.data()+offset+4); offset+=8;
      if(length>source.size()-offset)fail("GLTF_INVALID","GLB chunk exceeds file length");
      if(type==0x4e4f534a) {
        if(json_seen)fail("GLTF_INVALID","GLB has multiple JSON chunks");
        try { root=Json::parse(source.begin()+offset,source.begin()+offset+length); }
        catch (const std::exception &) { fail("GLTF_INVALID", "GLB JSON chunk is malformed"); }
        json_seen=true;
      } else if(type==0x004e4942) {
        if(!glb_binary.empty())fail("GLTF_INVALID","GLB has multiple BIN chunks");
        glb_binary.assign(source.begin()+offset,source.begin()+offset+length);
      }
      offset+=length;
    }
    if(offset!=source.size()||!json_seen)fail("GLTF_INVALID","GLB is truncated or has no JSON chunk");
  } else {
    try { root=Json::parse(source.begin(),source.end()); }
    catch(const std::exception &) { fail("GLTF_INVALID","glTF JSON is malformed"); }
  }
  try {
    if(!root.is_object()||!root.contains("asset")||root.at("asset").value("version",std::string())!="2.0")
      fail("GLTF_INVALID","input is not a glTF 2.0 asset");
    if(root.contains("extensionsRequired")&&!root.at("extensionsRequired").empty())
      fail("GLTF_UNSUPPORTED","required glTF extensions are not supported");
    const std::filesystem::path absolute_input=std::filesystem::canonical(input);
    const std::filesystem::path asset_root=absolute_input.parent_path();
    std::vector<BufferData> buffers;
    if(root.contains("buffers")) {
      const Json &items=root.at("buffers");
      if(!items.is_array()||items.size()>16)fail("GLTF_LIMIT","glTF supports at most 16 buffers");
      uintmax_t total=0;
      for(size_t i=0;i<items.size();++i) {
        const Json &buffer=items[i]; const size_t declared=buffer.at("byteLength").get<size_t>();
        if(declared==0||declared>512u*1024u*1024u||total+declared>512u*1024u*1024u)fail("GLTF_LIMIT","glTF total buffer budget exceeded (512 MiB)");
        Bytes bytes;
        if(buffer.contains("uri")) {
          if(ext==".glb"&&i==0&&buffer.at("uri").is_string()) {
            const auto path=confined_uri(asset_root,buffer.at("uri").get<std::string>(),512u*1024u*1024u,"buffer");
            bytes=read_bytes(path,512u*1024u*1024u,"glTF buffer");
          } else {
            if(!buffer.at("uri").is_string())fail("GLTF_INVALID","buffer URI must be a string");
            const auto path=confined_uri(asset_root,buffer.at("uri").get<std::string>(),512u*1024u*1024u,"buffer");
            bytes=read_bytes(path,512u*1024u*1024u,"glTF buffer");
          }
        } else if(ext==".glb"&&i==0&&!glb_binary.empty()) bytes=glb_binary;
        else fail("GLTF_INVALID","buffer has no external URI or GLB BIN chunk");
        if(bytes.size()<declared)fail("GLTF_INVALID","glTF buffer is shorter than byteLength");
        total+=declared; buffers.push_back({std::move(bytes)});
      }
    }
    if(!root.contains("meshes")||!root.at("meshes").is_array()||root.at("meshes").empty())fail("GLTF_INVALID","glTF contains no meshes");
    if(!root.contains("nodes")||!root.at("nodes").is_array()||root.at("nodes").size()>100'000)fail("GLTF_LIMIT","glTF node count exceeds 100000 or is missing");
    const Json &nodes=root.at("nodes");
    std::set<size_t> child_nodes;
    for(const Json &node:nodes)if(node.contains("children")) {
      if(!node.at("children").is_array())fail("GLTF_INVALID","node children must be an array");
      for(const Json &child:node.at("children")) {const size_t index=child.get<size_t>();if(index>=nodes.size())fail("GLTF_INVALID","child node index is out of range");child_nodes.insert(index);}
    }
    std::vector<size_t> roots;
    if(root.contains("scenes")&&!root.at("scenes").empty()) {
      const size_t scene_index=root.value("scene",size_t(0));
      if(scene_index>=root.at("scenes").size())fail("GLTF_INVALID","default scene index is out of range");
      const Json &scene_def=root.at("scenes")[scene_index];
      if(!scene_def.contains("nodes")||!scene_def.at("nodes").is_array())fail("GLTF_INVALID","scene root nodes are missing");
      for(const Json &index:scene_def.at("nodes"))roots.push_back(index.get<size_t>());
    } else {
      for(size_t i=0;i<nodes.size();++i)if(!child_nodes.contains(i))roots.push_back(i);
    }
    size_t total_vertices=0,total_triangles=0,visited=0;
    std::set<size_t> active;
    Mesh *first_mesh=nullptr;
    std::function<void(size_t,const Matrix&)> visit=[&](const size_t index,const Matrix &parent) {
      if(index>=nodes.size())fail("GLTF_INVALID","scene node index is out of range");
      if(active.contains(index))fail("GLTF_INVALID","node hierarchy contains a cycle");
      if(++visited>200'000)fail("GLTF_LIMIT","glTF traversal budget exceeded");
      active.insert(index);
      const Json &node=nodes[index]; const Matrix world=multiply(parent,node_matrix(node));
      if(node.contains("mesh")) {
        const size_t mesh_index=node.at("mesh").get<size_t>();
        if(mesh_index>=root.at("meshes").size())fail("GLTF_INVALID","node mesh index is out of range");
        const Json &mesh=root.at("meshes")[mesh_index];
        if(!mesh.contains("primitives")||!mesh.at("primitives").is_array())fail("GLTF_INVALID","mesh primitives are missing");
        for(const Json &primitive:mesh.at("primitives"))append_primitive(scene,root,buffers,primitive,world,total_vertices,total_triangles,asset_root,first_mesh);
      }
      if(node.contains("children"))for(const Json &child:node.at("children"))visit(child.get<size_t>(),world);
      active.erase(index);
    };
    for(size_t index:roots)visit(index,identity());
    if(!first_mesh)fail("GLTF_INVALID","selected glTF scene has no triangle mesh nodes");
    return first_mesh;
  } catch(const dslvm::Error &) { throw; }
    catch(const std::exception &) { fail("GLTF_INVALID","glTF contains malformed or missing fields"); }
}
} // namespace gltf_import
