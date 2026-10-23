# SPDX-License-Identifier: MIT
"""Blender (bpy) implementation of the lmblender DSL surface used by the examples.

Each method mirrors one lmblender call with the host's semantics (Z-up scene units,
positional arguments, Cycles CPU, Standard view transform). Geometry generators are
ports of the host's C++ builders so vertex/triangle counts match exactly.
Run inside Blender:  blender -b --factory-startup --python example.py
"""
import math
import os
import time

import bpy
from mathutils import Matrix, Vector

_T0 = time.time()


def _ease(t, interp):
    return t * t * (3.0 - 2.0 * t) if interp == "ease" else t


def sample_keys(keys, frame, base):
    """Port of the host's sample_keys: keys are (frame, value tuple, interpolation)."""
    if not keys:
        return base
    keys = sorted(keys, key=lambda k: k[0])
    hi = next((i for i, k in enumerate(keys) if k[0] >= frame), len(keys))
    if hi == 0:
        return keys[0][1]
    if hi == len(keys):
        return keys[-1][1]
    if keys[hi][0] == frame:
        return keys[hi][1]
    a, b = keys[hi - 1], keys[hi]
    t = _ease((frame - a[0]) / (b[0] - a[0]), a[2])
    return tuple(x + (y - x) * t for x, y in zip(a[1], b[1]))


def sample_visibility(keys, frame, base):
    if not keys:
        return base
    keys = sorted(keys, key=lambda k: k[0])
    hi = next((i for i, k in enumerate(keys) if k[0] >= frame), len(keys))
    if hi == 0:
        return keys[0][1]
    if hi == len(keys):
        return keys[-1][1]
    if keys[hi][0] == frame:
        return keys[hi][1]
    return keys[hi - 1][1]


def _basis(direction):
    """Right/up/back columns used by the host for lights and cameras (Z-up)."""
    f = Vector(direction).normalized()
    r = f.cross(Vector((0, 0, 1)))
    if r.length_squared < 1e-8:
        r = Vector((1, 0, 0))
    r.normalize()
    u = r.cross(f).normalized()
    return r, u, -f


def _look_matrix(position, target):
    r, u, back = _basis(Vector(target) - Vector(position))
    m = Matrix((r, u, back)).transposed().to_4x4()
    m.translation = Vector(position)
    return m


class Material:
    def __init__(self, scene, color=(0.8, 0.8, 0.8), metallic=0.0, roughness=0.5, transmission=0.0,
                 emission=(0.0, 0.0, 0.0), emission_strength=0.0):
        self.scene = scene
        self.color = tuple(color)
        self.metallic, self.roughness, self.transmission = metallic, roughness, transmission
        self.emission, self.emission_strength = tuple(emission), emission_strength
        self.alpha = 1.0
        self.texture_kind, self.color2, self.texture_scale = 0, (1.0, 1.0, 1.0), 5.0
        self.image_path = ""
        self.maps = ["", "", "", ""]  # normal, roughness, metallic, ao
        self.subsurface_weight, self.subsurface_radius, self.subsurface_scale = 0.0, (1.0, 0.2, 0.1), 0.1
        self.volume_kind, self.volume_density, self.volume_color = 0, 0.0, (1.0, 1.0, 1.0)
        self.bump_enabled, self.bump_args = False, (0.35, 0.12, 18.0)
        self.mix_material, self.mix_factor, self.mix_factor_image = None, 0.5, ""
        self.cull = False
        self.color_keys, self.emission_keys = [], []
        self.bl = None

    # DSL methods
    def subsurface(self, weight, rr, rg, rb, scale):
        self.subsurface_weight, self.subsurface_radius, self.subsurface_scale = weight, (rr, rg, rb), scale

    def volume(self, kind, density, r, g, b):
        self.volume_kind = 1 if kind == "absorption" else 2
        self.volume_density, self.volume_color = density, (r, g, b)

    def bump(self, strength, distance, scale):
        self.bump_enabled, self.bump_args = True, (strength, distance, scale)

    def texture_maps(self, normal, rough, metal, ao):
        self.maps = [normal, rough, metal, ao]

    def mix_with(self, other, factor):
        self.mix_material, self.mix_factor, self.mix_factor_image = other, factor, ""

    def mix_with_texture(self, other, path):
        self.mix_material, self.mix_factor_image = other, path

    def backface_culling(self, enabled):
        self.cull = enabled

    def keyframe_color(self, frame, r, g, b, interp):
        self.color_keys = [k for k in self.color_keys if k[0] != frame] + [(frame, (r, g, b), interp)]

    def keyframe_emission_strength(self, frame, value, interp):
        self.emission_keys = [k for k in self.emission_keys if k[0] != frame] + [(frame, (value,), interp)]

    # bpy node graph, rebuilt before every render so later DSL edits are honoured
    def build(self, color=None, emission_strength=None):
        if self.bl is None:
            self.bl = bpy.data.materials.new("lmb")
            self.bl.use_nodes = True
        nt = self.bl.node_tree
        nt.nodes.clear()
        n, links = nt.nodes, nt.links
        out = n.new("ShaderNodeOutputMaterial")
        color = color or self.color
        es = self.emission_strength if emission_strength is None else emission_strength

        def principled(m, col, ems):
            p = n.new("ShaderNodeBsdfPrincipled")
            p.inputs["Base Color"].default_value = (*col, 1.0)
            p.inputs["Metallic"].default_value = m.metallic
            p.inputs["Roughness"].default_value = m.roughness
            p.inputs["Alpha"].default_value = m.alpha
            p.inputs["Specular IOR Level"].default_value = 0.5
            p.inputs["Transmission Weight"].default_value = m.transmission
            p.inputs["Subsurface Weight"].default_value = m.subsurface_weight
            p.inputs["Subsurface Radius"].default_value = m.subsurface_radius
            p.inputs["Subsurface Scale"].default_value = m.subsurface_scale
            p.inputs["Emission Color"].default_value = (*m.emission, 1.0)
            p.inputs["Emission Strength"].default_value = ems
            return p

        p = principled(self, color, es)
        obj_coords = n.new("ShaderNodeTexCoord")  # standalone Cycles "generated" == object space

        def image(path, data=False):
            t = n.new("ShaderNodeTexImage")
            t.image = bpy.data.images.load(os.path.abspath(path), check_existing=True)
            if data:
                t.image.colorspace_settings.name = "Non-Color"
            t.interpolation, t.extension = "Linear", "REPEAT"
            links.new(obj_coords.outputs["UV"], t.inputs["Vector"])
            return t

        base = None
        if self.texture_kind == 1:
            t = n.new("ShaderNodeTexChecker")
            t.inputs["Color1"].default_value = (*color, 1)
            t.inputs["Color2"].default_value = (*self.color2, 1)
            t.inputs["Scale"].default_value = self.texture_scale
            links.new(obj_coords.outputs["Object"], t.inputs["Vector"])
            base = t.outputs["Color"]
        elif self.texture_kind == 2:
            t = n.new("ShaderNodeTexNoise")
            t.inputs["Scale"].default_value = self.texture_scale
            t.inputs["Detail"].default_value = 3.0
            t.inputs["Roughness"].default_value = 0.65
            links.new(obj_coords.outputs["Object"], t.inputs["Vector"])
            mix = n.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.inputs["A"].default_value = (*color, 1)
            mix.inputs["B"].default_value = (*self.color2, 1)
            links.new(t.outputs["Fac"], mix.inputs["Factor"])
            base = mix.outputs["Result"]
        elif self.texture_kind == 3:
            base = image(self.image_path).outputs["Color"]
        elif self.texture_kind == 4:
            t = n.new("ShaderNodeTexNoise")
            t.inputs["Scale"].default_value = self.texture_scale
            t.inputs["Detail"].default_value = 2.0
            links.new(obj_coords.outputs["Object"], t.inputs["Vector"])
            ramp = n.new("ShaderNodeValToRGB")
            ramp.color_ramp.elements[0].color = (*color, 1)
            ramp.color_ramp.elements[1].color = (*self.color2, 1)
            links.new(t.outputs["Fac"], ramp.inputs["Fac"])
            base = ramp.outputs["Color"]
        normal, rough, metal, ao = self.maps
        if rough:
            links.new(image(rough, True).outputs["Color"], p.inputs["Roughness"])
        if metal:
            links.new(image(metal, True).outputs["Color"], p.inputs["Metallic"])
        if ao:
            mul = n.new("ShaderNodeMix")
            mul.data_type, mul.blend_type = "RGBA", "MULTIPLY"
            mul.inputs["Factor"].default_value = 1.0
            mul.inputs["A"].default_value = (*color, 1)
            if base:
                links.new(base, mul.inputs["A"])
            links.new(image(ao, True).outputs["Color"], mul.inputs["B"])
            base = mul.outputs["Result"]
        if base:
            links.new(base, p.inputs["Base Color"])
        if normal:
            nm = n.new("ShaderNodeNormalMap")
            links.new(image(normal, True).outputs["Color"], nm.inputs["Color"])
            links.new(nm.outputs["Normal"], p.inputs["Normal"])
        if self.bump_enabled:
            strength, distance, scale = self.bump_args
            h = n.new("ShaderNodeTexNoise")
            h.inputs["Scale"].default_value = scale
            h.inputs["Detail"].default_value = 4.0
            h.inputs["Roughness"].default_value = 0.7
            links.new(obj_coords.outputs["Object"], h.inputs["Vector"])
            b = n.new("ShaderNodeBump")
            b.inputs["Strength"].default_value = strength
            b.inputs["Distance"].default_value = distance
            links.new(h.outputs["Fac"], b.inputs["Height"])
            links.new(b.outputs["Normal"], p.inputs["Normal"])
        if self.volume_kind:
            v = n.new("ShaderNodeVolumeAbsorption" if self.volume_kind == 1 else "ShaderNodeVolumeScatter")
            v.inputs["Color"].default_value = (*self.volume_color, 1)
            v.inputs["Density"].default_value = self.volume_density
            links.new(v.outputs["Volume"], out.inputs["Volume"])
            links.new(n.new("ShaderNodeBsdfTransparent").outputs["BSDF"], out.inputs["Surface"])
            return self.bl
        surface = p.outputs["BSDF"]
        if self.mix_material:
            o = self.mix_material
            layer = principled(o, o.color, o.emission_strength)
            mix = n.new("ShaderNodeMixShader")
            mix.inputs["Fac"].default_value = self.mix_factor
            if self.mix_factor_image:
                links.new(image(self.mix_factor_image, True).outputs["Color"], mix.inputs["Fac"])
            links.new(surface, mix.inputs[1])
            links.new(layer.outputs["BSDF"], mix.inputs[2])
            surface = mix.outputs["Shader"]
        if self.cull:
            geo = n.new("ShaderNodeNewGeometry")
            mix = n.new("ShaderNodeMixShader")
            links.new(geo.outputs["Backfacing"], mix.inputs["Fac"])
            links.new(surface, mix.inputs[1])
            links.new(n.new("ShaderNodeBsdfTransparent").outputs["BSDF"], mix.inputs[2])
            surface = mix.outputs["Shader"]
        links.new(surface, out.inputs["Surface"])
        return self.bl


class Mesh:
    def __init__(self, scene, obj, smooth=False):
        self.scene, self.obj, self.smooth = scene, obj, smooth
        self.translation, self.scale_v = Vector((0, 0, 0)), Vector((1, 1, 1))
        self.material, self.visible, self.ground = None, True, False
        self.transform_keys, self.visibility_keys = [], []
        self.source = None
        for p in obj.data.polygons:
            p.use_smooth = smooth

    def translate(self, dx, dy, dz):
        self.translation += Vector((dx, dy, dz))

    def scale(self, sx, sy, sz):
        self.scale_v = Vector((self.scale_v.x * sx, self.scale_v.y * sy, self.scale_v.z * sz))

    def rotate_x(self, degrees):
        self.obj.data.transform(Matrix.Rotation(math.radians(degrees), 4, "X"))

    def set_material(self, material):
        self.material = material

    def set_smooth(self, smooth):
        self.smooth = smooth
        for p in self.obj.data.polygons:
            p.use_smooth = smooth

    def keyframe_transform(self, frame, x, y, z, interp):
        self.transform_keys = [k for k in self.transform_keys if k[0] != frame] + [(frame, (x, y, z), interp)]

    def keyframe_visible(self, frame, visible, interp):
        self.visibility_keys = [k for k in self.visibility_keys if k[0] != frame] + [(frame, visible)]

    def vertex_count(self):
        return len(self.obj.data.vertices)

    def triangle_count(self):
        self.obj.data.calc_loop_triangles()
        return len(self.obj.data.loop_triangles)

    def material_count(self):
        return len([m for m in self.obj.data.materials if m])

    def _world_coords(self):
        m = Matrix.Translation(self.translation) @ Matrix.Diagonal((*self.scale_v, 1))
        return [m @ v.co for v in self.obj.data.vertices]

    def min_x(self): return min(c.x for c in self._world_coords())
    def min_y(self): return min(c.y for c in self._world_coords())
    def min_z(self): return min(c.z for c in self._world_coords())
    def max_x(self): return max(c.x for c in self._world_coords())
    def max_y(self): return max(c.y for c in self._world_coords())
    def max_z(self): return max(c.z for c in self._world_coords())


def _mesh_object(name, verts, tris, uvs=None):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(t) for t in tris])
    if uvs:
        layer = me.uv_layers.new(name="UVMap")
        for i, loop in enumerate(layer.data):
            loop.uv = uvs[i]
    me.update()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def _planar_uvs(verts, tris):
    lo = [min(v[k] for v in verts) for k in range(3)]
    hi = [max(v[k] for v in verts) for k in range(3)]
    out = []
    for t in tris:
        for i in t:
            q = verts[i]
            out.append(((q[0] - lo[0]) / max(hi[0] - lo[0], 1e-9), (q[1] - lo[1]) / max(hi[1] - lo[1], 1e-9)))
    return out


class Scene:
    def __init__(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.sc = bpy.context.scene
        self.meshes, self.materials = [], []
        self.point_lights, self.area_lights, self.sun_lights, self.spot_lights = [], [], [], []
        self.world_rgb, self.world_strength = (0.16, 0.18, 0.22), 0.45
        self.sky, self.environment, self.transparent = None, None, False
        self.camera = None  # dict(position, target, fov, ortho, ortho_scale, fit)
        self.aperture, self.focus_distance = 0.0, 10.0
        self.max_bounces, self.seed, self.denoise = 12, 0, False
        self.clamp = (0.0, 0.0)
        self.threads = 4
        self.view_transform = "Standard"
        self.motion = (False, 1.0)
        self.camera_keys, self.optics_keys = [], []
        self.timeout = 3600
        self.denoise_quality = "HIGH"

    # ---- geometry -------------------------------------------------------
    def _add(self, name, verts, tris, smooth, uvs=None):
        obj = _mesh_object(name, verts, tris, uvs if uvs is not None else _planar_uvs(verts, tris))
        mesh = Mesh(self, obj, smooth)
        self.meshes.append(mesh)
        return mesh

    def plane(self, size):
        s = size * 0.5
        return self._add("plane", [(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [(0, 1, 2), (0, 2, 3)], False)

    def cube(self, size):
        s = size * 0.5
        v = [(-s, -s, -s), (s, -s, -s), (s, s, -s), (-s, s, -s), (-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s)]
        f = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5), (2, 3, 7),
             (2, 7, 6), (3, 0, 4), (3, 4, 7)]
        return self._add("cube", v, f, False)

    def uv_sphere(self, r, seg, rings, smooth):
        v, f = [], []
        for y in range(rings + 1):
            phi = math.pi * y / rings
            for x in range(seg):
                th = 2 * math.pi * x / seg
                v.append((r * math.sin(phi) * math.cos(th), r * math.sin(phi) * math.sin(th), r * math.cos(phi)))
        for y in range(rings):
            for x in range(seg):
                a, b = y * seg + x, y * seg + (x + 1) % seg
                c, d = (y + 1) * seg + (x + 1) % seg, (y + 1) * seg + x
                if y > 0:
                    f.append((a, d, b))
                if y < rings - 1:
                    f.append((b, d, c))
        return self._add("uv_sphere", v, f, smooth)

    def _cyl(self, kind, r, h, seg, smooth):
        v, f = [], []
        for z in range(2):
            for x in range(seg):
                th = 2 * math.pi * x / seg
                rr = 0 if (kind == "cone" and z == 1) else r
                v.append((rr * math.cos(th), rr * math.sin(th), h * 0.5 if z else -h * 0.5))
        bottom, top = 2 * seg, 2 * seg + 1
        v += [(0, 0, -h * 0.5), (0, 0, h * 0.5)]
        for x in range(seg):
            nx = (x + 1) % seg
            f.append((x, nx, seg + nx))
            if kind == "cylinder":
                f.append((x, seg + nx, seg + x))
            f.append((bottom, nx, x))
            if kind == "cylinder":
                f.append((top, seg + x, seg + nx))
        return self._add(kind, v, f, smooth)

    # ---- swept tubes (ports of the host's append_sweep and its callers) ----
    @staticmethod
    def _sweep(verts, tris, path, profile, base_r, tip_r, cap):
        base, sides = len(verts), len(profile)
        prev_u = None
        for i, p in enumerate(path):
            before, after = path[i - 1 if i else i], path[i + 1 if i + 1 < len(path) else i]
            t = (Vector(after) - Vector(before)).normalized()
            if i == 0:
                ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((0, 1, 0))
                u = t.cross(ref).normalized()
            else:
                u = (prev_u - prev_u.dot(t) * t).normalized()
            prev_u = u
            v = t.cross(u)
            r = base_r + (tip_r - base_r) * i / (len(path) - 1)
            for q in profile:
                verts.append(tuple(Vector(p) + r * (u * q[0] + v * q[1])))
        for i in range(len(path) - 1):
            for j in range(sides):
                n = (j + 1) % sides
                a, b = base + i * sides + j, base + i * sides + n
                c, d = base + (i + 1) * sides + n, base + (i + 1) * sides + j
                tris += [(a, b, c), (a, c, d)]
        if cap:
            last = base + (len(path) - 1) * sides
            for j in range(1, sides - 1):
                tris += [(base, base + j + 1, base + j), (last, last + j, last + j + 1)]

    @staticmethod
    def _points(text):
        return [tuple(float(x) for x in item.split(",")) for item in text.split(";") if item.strip()]

    @staticmethod
    def _circle(sides):
        return [(math.cos(2 * math.pi * j / sides), math.sin(2 * math.pi * j / sides)) for j in range(sides)]

    def strand(self, points, root, tip, sides):
        v, f = [], []
        self._sweep(v, f, self._points(points), self._circle(sides), root, tip, True)
        return self._add("strand", v, f, True, [])

    def hair_sphere(self, x, y, z, r, count, length, width):
        v, f = [], []
        for i in range(count):
            h = 1.0 - 2.0 * (i + 0.5) / count
            t = i * 2.399963229728653
            radial = math.sqrt(max(0.0, 1.0 - h * h))
            d = Vector((radial * math.cos(t), radial * math.sin(t), h))
            side = d.cross(Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((0, 1, 0))).normalized()
            path = []
            for s in (0.0, 0.35, 0.72, 1.0):
                dist, bend = r + length * s, length * 0.72 * s * s
                path.append(tuple(Vector((x, y, z)) + dist * d + bend * side - Vector((0, 0, length * 0.28 * s * s))))
            self._sweep(v, f, path, self._circle(8), width, width * 0.08, True)
        return self._add("hair_sphere", v, f, True, [])

    def hair_patch(self, x, y, z, size, count, height, width):
        v, f = [], []
        for i in range(count):
            u = math.fmod((i + 0.5) * 0.7548776662466927, 1.0)
            w = math.fmod((i + 0.5) * 0.5698402909980532, 1.0)
            px, py = x + (u - 0.5) * size, y + (w - 0.5) * size
            h = height * (0.72 + 0.28 * math.fmod((i + 1) * 0.618033988749895, 1.0))
            lean = 0.22 * h
            path = [(px, py, z), (px + 0.02 * h, py, z + 0.4 * h), (px + 0.11 * h, py + 0.04 * h, z + 0.76 * h),
                    (px + lean, py + 0.08 * h, z + h)]
            self._sweep(v, f, path, self._circle(6), width, width * 0.07, True)
        return self._add("hair_patch", v, f, True, [])

    def loft(self, path, profile, cap):
        prof = self._points(profile)
        area = sum(prof[i][0] * prof[(i + 1) % len(prof)][1] - prof[(i + 1) % len(prof)][0] * prof[i][1]
                   for i in range(len(prof)))
        if area < 0:
            prof.reverse()
        v, f = [], []
        self._sweep(v, f, self._points(path), prof, 1.0, 1.0, cap)
        return self._add("loft", v, f, False, [])

    def toon_outline(self, mesh, thickness, r, g, b):
        src = mesh.source or mesh
        me = src.obj.data
        verts = [tuple(x.co) for x in me.vertices]
        me.calc_loop_triangles()
        edges = {}
        for t in me.loop_triangles:
            ids = list(t.vertices)
            p0, p1, p2 = (Vector(verts[i]) for i in ids)
            n = (p1 - p0).cross(p2 - p0)
            if n.length < 1e-12:
                continue
            n.normalize()
            for j in range(3):
                x, y = ids[j], ids[(j + 1) % 3]
                edges.setdefault((min(x, y), max(x, y)), []).append(n)
        v, f = [], []
        for (a, b2), adj in sorted(edges.items()):
            if len(adj) == 2 and adj[0].dot(adj[1]) >= 0.8660254:
                continue
            if (Vector(verts[a]) - Vector(verts[b2])).length < 1e-8:
                continue
            self._sweep(v, f, [verts[a], verts[b2]], self._circle(8), thickness, thickness, True)
        strokes = self._add("toon_outline", v, f, True, [])
        strokes.translation, strokes.scale_v = Vector(mesh.translation), Vector(mesh.scale_v)
        strokes.material = Material(self, (r, g, b), roughness=1.0)
        return strokes

    def cylinder(self, r, h, seg, smooth):
        return self._cyl("cylinder", r, h, seg, smooth)

    def cone(self, r, h, seg, smooth):
        return self._cyl("cone", r, h, seg, smooth)

    def torus(self, R, r, seg, rings, smooth):
        v, f = [], []
        for y in range(rings):
            for x in range(seg):
                a, b = 2 * math.pi * x / seg, 2 * math.pi * y / rings
                rr = R + r * math.cos(b)
                v.append((rr * math.cos(a), rr * math.sin(a), r * math.sin(b)))
        for y in range(rings):
            for x in range(seg):
                a, b = y * seg + x, y * seg + (x + 1) % seg
                c, d = ((y + 1) % rings) * seg + (x + 1) % seg, ((y + 1) % rings) * seg + x
                f += [(a, b, c), (a, c, d)]
        return self._add("torus", v, f, smooth)

    def instance_mesh(self, mesh, x, y, z, sx, sy, sz):
        obj = bpy.data.objects.new("instance", mesh.obj.data)  # linked duplicate shares mesh data
        bpy.context.scene.collection.objects.link(obj)
        inst = Mesh(self, obj, mesh.smooth)
        inst.material, inst.source = mesh.material, mesh
        inst.translation, inst.scale_v = Vector((x, y, z)), Vector((sx, sy, sz))
        self.meshes.append(inst)
        return inst

    def _import(self, op, path, **kw):
        before = set(bpy.data.objects)
        op(filepath=os.path.abspath(path), **kw)
        new = [o for o in bpy.data.objects if o not in before]
        for o in new:
            if o.type != "MESH":
                bpy.data.objects.remove(o)
        meshes = [o for o in new if o.name in bpy.data.objects and o.type == "MESH"]
        bpy.ops.object.select_all(action="DESELECT")
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        obj.parent = None
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        mesh = Mesh(self, obj, any(p.use_smooth for p in obj.data.polygons))
        self.meshes.append(mesh)
        return mesh

    def import_obj(self, path):
        return self._import(bpy.ops.wm.obj_import, path, forward_axis="Y", up_axis="Z")

    def import_stl(self, path):
        return self._import(bpy.ops.wm.stl_import, path, forward_axis="Y", up_axis="Z")

    def import_ply(self, path):
        return self._import(bpy.ops.wm.ply_import, path, forward_axis="Y", up_axis="Z")

    def import_gltf(self, path):
        return self._import(bpy.ops.import_scene.gltf, path)

    def ground_plane(self, z, size, r, g, b):
        m = self.material(r, g, b, 0.0, 0.85, 0.0, 0.0, 0.0, 0.0, 0.0)
        mesh = self._add("ground", [(-size, -size, z), (size, -size, z), (size, size, z), (-size, size, z)],
                         [(0, 1, 2), (0, 2, 3)], False)
        mesh.material, mesh.ground = m, True

    def checker_ground(self, size, scale):
        m = Material(self, (0.035, 0.045, 0.06), roughness=0.82)
        m.texture_kind, m.color2, m.texture_scale = 1, (0.72, 0.75, 0.78), scale
        self.materials.append(m)
        s = size
        mesh = self._add("checker_ground", [(-s, -s, -0.02), (s, -s, -0.02), (s, s, -0.02), (-s, s, -0.02)],
                         [(0, 2, 1), (0, 3, 2)], False, [(0, 0), (1, 1), (1, 0), (0, 0), (0, 1), (1, 1)])
        mesh.material, mesh.ground = m, True

    def wall_plane_y(self, y, size, r, g, b):
        m = self.material(r, g, b, 0.0, 0.9, 0.0, r, g, b, 0.3)
        s = size
        mesh = self._add("wall", [(-s, y, -s), (s, y, -s), (s, y, s), (-s, y, s)], [(0, 1, 2), (0, 2, 3)], False)
        mesh.material, mesh.ground = m, True

    # ---- materials ------------------------------------------------------
    def material(self, r, g, b, metallic, roughness, transmission, er, eg, eb, es):
        m = Material(self, (r, g, b), metallic, roughness, transmission, (er, eg, eb), es)
        self.materials.append(m)
        return m

    def _pattern(self, kind, a):
        m = Material(self, a[0:3], roughness=a[7])
        m.texture_kind, m.color2, m.texture_scale = kind, tuple(a[3:6]), a[6]
        self.materials.append(m)
        return m

    def checker_material(self, *a): return self._pattern(1, a)
    def noise_material(self, *a): return self._pattern(2, a)
    def ramp_material(self, *a): return self._pattern(4, a)

    def image_material(self, path, roughness):
        m = Material(self, roughness=roughness)
        m.texture_kind, m.texture_scale, m.image_path = 3, 1.0, path
        self.materials.append(m)
        return m

    # ---- lights / world -------------------------------------------------
    def world_color(self, r, g, b, strength):
        self.world_rgb, self.world_strength = (r, g, b), strength

    def sky_texture(self, elevation, rotation, intensity):
        self.sky = (math.radians(elevation), math.radians(rotation), intensity)

    def environment_texture(self, path, rotation, strength):
        self.environment = (path, math.radians(rotation), strength)

    def transparent_background(self, enabled):
        self.transparent = enabled

    def point_light(self, x, y, z, r, g, b, strength):
        self.point_lights.append({"p": (x, y, z), "c": (r, g, b), "s": strength, "keys": []})

    def point_light_keyframe(self, index, frame, strength, interp):
        keys = self.point_lights[index]["keys"]
        self.point_lights[index]["keys"] = [k for k in keys if k[0] != frame] + [(frame, (strength,), interp)]

    def area_light(self, x, y, z, tx, ty, tz, r, g, b, strength, size):
        self.area_lights.append({"p": (x, y, z), "t": (tx, ty, tz), "c": (r, g, b), "s": strength,
                                 "size": (size, size), "shape": "SQUARE"})

    def area_light_shape(self, x, y, z, tx, ty, tz, r, g, b, strength, sx, sy, shape):
        bl = {"rectangle": "RECTANGLE", "disk": "DISK", "ellipse": "ELLIPSE"}[shape]
        self.area_lights.append({"p": (x, y, z), "t": (tx, ty, tz), "c": (r, g, b), "s": strength,
                                 "size": (sx, sx if shape == "disk" else sy), "shape": bl})

    def sun_light(self, dx, dy, dz, r, g, b, strength, angle):
        self.sun_lights.append({"d": (dx, dy, dz), "c": (r, g, b), "s": strength, "angle": math.radians(angle)})

    def spot_light(self, x, y, z, tx, ty, tz, r, g, b, strength, angle, blend):
        self.spot_lights.append({"p": (x, y, z), "t": (tx, ty, tz), "c": (r, g, b), "s": strength,
                                 "angle": math.radians(angle), "blend": blend})

    def light_clamp(self, direct, indirect):
        self.clamp = (direct, indirect)

    # ---- camera ---------------------------------------------------------
    def camera_look_at(self, px, py, pz, tx, ty, tz, fov):
        self._camera_authored = None  # a new explicit camera replaces the authored one
        self.camera = {"p": (px, py, pz), "t": (tx, ty, tz), "fov": math.radians(fov), "ortho": False,
                       "span": 10.0, "fit": False}

    def camera_orthographic(self, px, py, pz, tx, ty, tz, span):
        self._camera_authored = None  # a new explicit camera replaces the authored one
        self.camera = {"p": (px, py, pz), "t": (tx, ty, tz), "fov": math.radians(45.0), "ortho": True,
                       "span": span, "fit": False}

    def fit_camera(self, fov):
        self._camera_authored = None  # a new explicit camera replaces the authored one
        lo, hi = self._bounds()
        self.camera = {"p": (0, 0, 0), "t": tuple((lo + hi) * 0.5), "fov": math.radians(fov), "ortho": False,
                       "span": 10.0, "fit": True}

    def camera_dof(self, aperture, focus):
        self.aperture, self.focus_distance = aperture, focus

    def camera_keyframe(self, frame, px, py, pz, tx, ty, tz, interp):
        self.camera_keys = [k for k in self.camera_keys if k[0] != frame] + [(frame, (px, py, pz, tx, ty, tz), interp)]

    def camera_optics_keyframe(self, frame, fov, span, interp):
        self.optics_keys = [k for k in self.optics_keys if k[0] != frame] + [(frame, (math.radians(fov), span), interp)]

    def motion_blur(self, enabled, shutter):
        self.motion = (enabled, shutter)

    # ---- render settings ------------------------------------------------
    def render_settings(self, bounces, seed, denoise):
        self.max_bounces, self.seed, self.denoise = bounces, seed, denoise

    def denoise_quality(self, quality):
        self.denoise_quality = quality.upper()

    def set_threads(self, n):
        self.threads = n

    def set_view_transform(self, name):
        self.view_transform = name

    def render_timeout(self, seconds):
        self.timeout = seconds

    def version(self):
        return "Blender " + bpy.app.version_string

    def expect_int(self, actual, expected):
        status = "ok" if actual == expected else "MISMATCH"
        print(f"expect_int {status}: actual={actual} expected={expected}")

    # ---- scene assembly -------------------------------------------------
    def _bounds(self):
        lo = Vector((math.inf,) * 3)
        hi = Vector((-math.inf,) * 3)
        for m in self.meshes:
            if m.ground:
                continue
            for c in m._world_coords():
                lo = Vector(map(min, lo, c))
                hi = Vector(map(max, hi, c))
        return lo, hi

    def _apply_frame(self, frame):
        """Set every keyed value for `frame`; frame None means a still render, which (like the
        host's render_png) uses the authored values and ignores keys."""
        for obj in [m.obj for m in self.meshes] + [self.sc.camera]:
            obj.animation_data_clear()  # drop motion-blur keys from an earlier render
        if frame is None:
            sample = lambda keys, f, base: base
        else:
            sample = sample_keys
        for m in self.meshes:
            loc = sample(m.transform_keys, frame, tuple(m.translation))
            m.obj.location = loc
            m.obj.scale = m.scale_v
            vis = m.visible and (frame is None or sample_visibility(m.visibility_keys, frame, True))
            m.obj.hide_render = not vis
            mat = m.material or (m.source.material if m.source else None)
            if mat is not None:
                col = sample(mat.color_keys, frame, mat.color)
                es = sample(mat.emission_keys, frame, (mat.emission_strength,))[0]
                bl = mat.build(col, es)
                if not m.obj.material_slots:
                    m.obj.data.materials.append(None)
                for slot in m.obj.material_slots:  # object-linked so instances sharing mesh data differ
                    slot.link, slot.material = "OBJECT", bl
            elif not m.obj.data.materials:
                m.obj.data.materials.append(self._default_surface())
        for L in self.point_lights:
            L["obj"].data.energy = sample(L["keys"], frame, (L["s"],))[0]
        if self.camera_keys or self.optics_keys:
            # Sample from the authored camera so a later still render gets it back, as in the host.
            if getattr(self, "_camera_authored", None) is None:
                self._camera_authored = dict(self.camera)
            authored = self._camera_authored
            self.camera = dict(authored)
            if self.camera_keys:
                c = sample(self.camera_keys, frame, (*authored["p"], *authored["t"]))
                self.camera["p"], self.camera["t"] = c[:3], c[3:]
            if self.optics_keys:
                fov, span = sample(self.optics_keys, frame, (authored["fov"], authored["span"]))
                self.camera["fov"], self.camera["span"] = fov, span
        self._place_camera()
        if self.motion[0] and frame is not None:
            self._motion_keys(frame)

    def _motion_keys(self, frame):
        """Host motion blur samples transforms at frame-0.5, frame, frame+0.5; bake those as Blender subframe keys."""
        self.sc.frame_set(frame)
        cam = self.sc.camera
        base_cam = dict(self.camera)
        for step in (-0.5, 0.0, 0.5):
            f = frame + step
            for m in self.meshes:
                if m.transform_keys:
                    m.obj.location = sample_keys(m.transform_keys, f, tuple(m.translation))
                    m.obj.keyframe_insert("location", frame=f)
            if self.camera_keys:
                c = sample_keys(self.camera_keys, f, (*base_cam["p"], *base_cam["t"]))
                cam.matrix_world = _look_matrix(c[:3], c[3:])
                cam.keyframe_insert("location", frame=f)
                cam.keyframe_insert("rotation_euler", frame=f)
        for obj in [m.obj for m in self.meshes] + [cam]:
            if obj.animation_data and obj.animation_data.action:
                for layer in getattr(obj.animation_data.action, "layers", []):
                    for strip in layer.strips:
                        for bag in strip.channelbags:
                            for fc in bag.fcurves:
                                for kp in fc.keyframe_points:
                                    kp.interpolation = "LINEAR"
        self.sc.frame_set(frame)

    def _default_surface(self):
        mat = bpy.data.materials.get("lmb-default") or bpy.data.materials.new("lmb-default")
        mat.use_nodes = True
        nt = mat.node_tree
        nt.nodes.clear()
        e = nt.nodes.new("ShaderNodeEmission")
        e.inputs["Color"].default_value = (0.62, 0.66, 0.72, 1)
        e.inputs["Strength"].default_value = 1.0
        nt.links.new(e.outputs[0], nt.nodes.new("ShaderNodeOutputMaterial").inputs["Surface"])
        return mat

    def _light(self, kind, name, color, strength):
        data = bpy.data.lights.new(name, kind)
        data.color, data.energy = color, strength
        if kind in ("POINT", "SPOT"):
            data.shadow_soft_size = 0.0
        obj = bpy.data.objects.new(name, data)
        self.sc.collection.objects.link(obj)
        return obj

    def _build_lights(self):
        lo, hi = self._bounds()
        center = (lo + hi) * 0.5
        radius = max(0.5, max(hi - lo) * 0.5)
        if not (self.point_lights or self.area_lights or self.spot_lights or self.sun_lights):
            self.point_lights.append({"p": tuple(center + Vector((-radius * 2.5, -radius * 3.2, radius * 4.0))),
                                      "c": (1, 1, 1), "s": 550.0, "keys": []})
        for L in self.point_lights:
            L["obj"] = self._light("POINT", "point", L["c"], L["s"])
            L["obj"].location = L["p"]
        for L in self.area_lights:
            o = self._light("AREA", "area", L["c"], L["s"])
            o.data.shape = L["shape"]
            o.data.size, o.data.size_y = L["size"]
            o.matrix_world = _look_matrix(L["p"], L["t"])
        for L in self.spot_lights:
            o = self._light("SPOT", "spot", L["c"], L["s"])
            o.data.spot_size, o.data.spot_blend = L["angle"], L["blend"]
            o.matrix_world = _look_matrix(L["p"], L["t"])
        for L in self.sun_lights:
            o = self._light("SUN", "sun", L["c"], L["s"])
            o.data.angle = L["angle"]
            o.matrix_world = _look_matrix((0, 0, 0), L["d"])

    def _build_world(self):
        world = bpy.data.worlds.new("lmb")
        self.sc.world = world
        world.use_nodes = True
        nt = world.node_tree
        nt.nodes.clear()
        bg = nt.nodes.new("ShaderNodeBackground")
        bg.inputs["Color"].default_value = (*self.world_rgb, 1)
        strength = self.world_strength
        if self.sky:
            sky = nt.nodes.new("ShaderNodeTexSky")
            sky.sky_type = "MULTIPLE_SCATTERING"
            sky.sun_elevation, sky.sun_rotation, sky.sun_intensity = self.sky
            nt.links.new(sky.outputs["Color"], bg.inputs["Color"])
            strength = self.world_strength * self.sky[2]
        elif self.environment:
            path, rot, strength = self.environment
            coords = nt.nodes.new("ShaderNodeTexCoord")
            mapping = nt.nodes.new("ShaderNodeMapping")
            mapping.inputs["Rotation"].default_value = (0, 0, rot)
            env = nt.nodes.new("ShaderNodeTexEnvironment")
            env.image = bpy.data.images.load(os.path.abspath(path), check_existing=True)
            nt.links.new(coords.outputs["Reflection"], mapping.inputs["Vector"])
            nt.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
            nt.links.new(env.outputs["Color"], bg.inputs["Color"])
        bg.inputs["Strength"].default_value = strength
        nt.links.new(bg.outputs["Background"], nt.nodes.new("ShaderNodeOutputWorld").inputs["Surface"])

    def _place_camera(self):
        cam = self.sc.camera
        lo, hi = self._bounds()
        center = (lo + hi) * 0.5
        w, h = self.sc.render.resolution_x, self.sc.render.resolution_y
        cam.data.sensor_fit = "VERTICAL"
        cam.data.clip_start, cam.data.clip_end = 0.01, 10000.0
        self.sc.render.pixel_aspect_x = self.sc.render.pixel_aspect_y = 1.0
        if self.camera is None:
            # Host fallback: three-quarter view with a 0.8 rad vertical field of view.
            radius = max(0.5, max(hi - lo) * 0.5)
            d = radius * 1.85 / math.tan(0.4)
            p = center + Vector((d * 0.62, -d, d * 0.8))
            cam.matrix_world = _look_matrix(p, center)
            cam.data.type, cam.data.angle_y = "PERSP", 0.8
            return
        c = self.camera
        p = c["p"]
        if c["fit"]:
            fit_radius = max(0.5, 0.5 * (hi - lo).length)
            half = min(c["fov"] * 0.5, math.atan(math.tan(c["fov"] * 0.5) * w / h))
            dist = fit_radius / math.sin(half) * 1.2
            p = Vector(c["t"]) + dist * Vector((0.50, -0.61, 0.25)).normalized()
        cam.matrix_world = _look_matrix(p, c["t"])
        if c["ortho"]:
            cam.data.type, cam.data.ortho_scale = "ORTHO", c["span"]
        else:
            cam.data.type, cam.data.angle_y = "PERSP", c["fov"]
        cam.data.dof.use_dof = self.aperture > 0
        cam.data.dof.focus_distance = self.focus_distance
        # Cycles aperture size is a radius in scene units; Blender stores an f-stop with focal length in mm.
        if self.aperture > 0:
            cam.data.dof.aperture_fstop = (cam.data.lens / 1000.0) / (2.0 * self.aperture)

    def _setup(self, samples, width, height):
        sc = self.sc
        sc.render.engine = "CYCLES"
        sc.cycles.device = "CPU"
        sc.cycles.samples = samples
        sc.cycles.use_adaptive_sampling = True
        sc.cycles.adaptive_threshold = 0.01
        sc.cycles.max_bounces = self.max_bounces
        for attr in ("diffuse_bounces", "glossy_bounces", "transmission_bounces", "volume_bounces",
                     "transparent_max_bounces"):
            setattr(sc.cycles, attr, min(7, self.max_bounces))
        sc.cycles.caustics_reflective = sc.cycles.caustics_refractive = True
        sc.cycles.blur_glossy = 0.0
        sc.cycles.sample_clamp_direct, sc.cycles.sample_clamp_indirect = self.clamp
        sc.cycles.seed = self.seed
        sc.cycles.use_denoising = self.denoise
        try:
            sc.cycles.denoiser = "OPENIMAGEDENOISE"
            sc.cycles.denoising_quality = self.denoise_quality
            sc.cycles.denoising_prefilter = "FAST"
        except (TypeError, AttributeError):
            # Some distribution builds (for example Ubuntu's Blender 4.0) ship without OIDN.
            if self.denoise:
                print("warning: this Blender has no OpenImageDenoise; rendering without denoising")
            sc.cycles.use_denoising = False
        sc.cycles.pixel_filter_type = "BOX"
        sc.cycles.filter_width = 1.0
        sc.render.threads_mode, sc.render.threads = "FIXED", min(self.threads, os.cpu_count() or 1)
        sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = width, height, 100
        # Closest bpy analogue of render_timeout: Cycles stops sampling and still saves (no RENDER_TIMEOUT error).
        sc.cycles.time_limit = self.timeout if self.timeout < 3600 else 0.0
        sc.render.film_transparent = self.transparent
        sc.render.use_motion_blur = self.motion[0]
        sc.render.motion_blur_shutter = self.motion[1]
        sc.view_settings.view_transform = self.view_transform
        sc.view_settings.look = "None"
        sc.view_settings.exposure, sc.view_settings.gamma = 0.0, 1.0
        sc.display_settings.display_device = "sRGB"
        sc.render.image_settings.file_format = "PNG"
        sc.render.image_settings.color_mode = "RGBA" if self.transparent else "RGB"
        if sc.camera is None:
            cam = bpy.data.objects.new("camera", bpy.data.cameras.new("camera"))
            sc.collection.objects.link(cam)
            sc.camera = cam
        if not getattr(self, "_built", False):
            self._build_world()
            self._build_lights()
            self._built = True
        for L in bpy.data.lights:  # standalone Cycles lights default to use_mis = false
            if hasattr(L, "cycles") and hasattr(L.cycles, "use_multiple_importance_sampling"):
                L.cycles.use_multiple_importance_sampling = False

    def _render(self, path, frame=None):
        os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
        self._apply_frame(frame)
        self.sc.render.filepath = os.path.abspath(path)
        t = time.time()
        bpy.ops.render.render(write_still=True)
        print(f"blender render {path}: {time.time() - t:.2f}s")

    def render_png(self, path, samples, width, height):
        self._setup(samples or 64, width, height)
        self._render(path)

    def render_exr(self, path, samples, width, height):
        self._setup(samples, width, height)
        self.sc.render.image_settings.file_format = "OPEN_EXR"
        self.sc.render.image_settings.color_depth = "32"
        self._render(path)

    def render_sequence(self, pattern, start, end, samples, width, height):
        self._setup(samples or 64, width, height)
        for f in range(start, end + 1):
            path = pattern.replace("{frame}", str(f)).replace("####", f"{f:04d}")
            self._render(path, f)

    def render_apng(self, path, start, end, samples, width, height, fps):
        # Blender has no APNG writer: render the frames as PNGs (assemble with ffmpeg -f apng if needed).
        self.render_sequence(path[:-5] + "-####.png", start, end, samples, width, height)

    def render_passes(self, prefix, samples, width, height):
        self._setup(samples, width, height)
        vl = self.sc.view_layers[0]
        vl.use_pass_z = vl.use_pass_normal = vl.use_pass_diffuse_color = True
        self._render(prefix + "-combined.png")
        # Blender writes all passes into one multilayer EXR (lmblender writes one EXR + PNG per pass).
        if hasattr(self.sc.render.image_settings, "media_type"):  # Blender 5.0 and newer
            self.sc.render.image_settings.media_type = "MULTI_LAYER_IMAGE"
        self.sc.render.image_settings.file_format = "OPEN_EXR_MULTILAYER"
        self.sc.render.image_settings.color_depth = "32"
        self._render(prefix + "-passes.exr")


def done():
    print(f"blender total {time.time() - _T0:.2f}s")
