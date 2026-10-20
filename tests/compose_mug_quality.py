#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

"""Falsifiable visual checks for the lmcad mug render (RGB PNG)."""
import math
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

path = sys.argv[1]
stl_path = sys.argv[2] if len(sys.argv) > 2 else "build/compose-input/build/mug.stl"
w, h, channels, rows = decode(path)
def px(x, y):
    return tuple(rows[y][x * channels:x * channels + 3])
def luma(rgb):
    r, g, b = rgb
    return (2126*r + 7152*g + 722*b) / 10000
# The mug is blue ceramic; this excludes the neutral gray floor/world. Keep a
# full silhouette mask for mesh-proportion comparison and a torso-only mask
# for the upright-axis check so the side handle cannot skew the PCA.
points = []
torso_points = []
for y in range(0, int(h * 0.82)):
    for x in range(int(w * 0.30), int(w * 0.70)):
        r, g, b = px(x, y)
        if g > r * 1.3 and b > g * 1.05 and b > 18:
            pixel = (x, y, luma((r, g, b)))
            points.append(pixel)
            if x < int(w * 0.60):
                torso_points.append(pixel)
assert len(points) > 1000, f"blue ceramic body missing or too small: {len(points)} classified pixels"
xs = [p[0] for p in points]; ys = [p[1] for p in points]
tx = [p[0] for p in torso_points]; ty = [p[1] for p in torso_points]
assert min(ys) > 0 and max(ys) < h - 1, "mug body is cropped by the image border"
mx = sum(tx) / len(tx); my = sum(ty) / len(ty)
xx = sum((x-mx)**2 for x in tx) / len(tx)
yy = sum((y-my)**2 for y in ty) / len(ty)
xy = sum((x-mx)*(y-my) for x,y,_ in torso_points) / len(torso_points)
angle = 0.5 * math.degrees(math.atan2(2*xy, xx-yy))
vertical_error = abs(90 - abs(angle))
assert vertical_error <= 10, f"mug principal axis is {vertical_error:.1f} degrees from upright"
mean = sum(v for _,_,v in points) / len(points)
spread = math.sqrt(sum(v*v for _,_,v in points) / len(points) - mean*mean)
assert spread >= 7, f"body shading has too little luminance variation: sigma={spread:.1f}/255"
lum = sorted(v for _, _, v in points)
p5 = lum[int(0.05 * (len(lum) - 1))]
p95 = lum[int(0.95 * (len(lum) - 1))]
assert mean >= 50, f"mug body is too dim: mean luminance={mean:.1f}/255"
assert p95 - p5 >= 35, f"mug body lacks readable light range: p5={p5:.1f}, p95={p95:.1f}"
# The exported STL is rotated from lmcad's Y-up convention; compare the full
# rendered silhouette aspect to the actual horizontal/vertical source spans.
raw = open(stl_path, "rb").read()
triangle_count = struct.unpack_from("<I", raw, 80)[0]
assert len(raw) == 84 + triangle_count * 50, "mug STL is truncated or malformed"
vertices = []
for tri in range(triangle_count):
    base = 84 + tri * 50 + 12
    for corner in range(3):
        x, y, z = struct.unpack_from("<3f", raw, base + corner * 12)
        # Match mug-render.lms: +Y-up STL -> +Z-up, 1.2 radial profile scale,
        # then z=7 placement. These are the actual transformed source points.
        vertices.append((x * 1.2, -z * 1.2, y + 7.0))
lo = [min(v[i] for v in vertices) for i in range(3)]
hi = [max(v[i] for v in vertices) for i in range(3)]
center = [(lo[i] + hi[i]) * 0.5 for i in range(3)]
radius = max(0.5, 0.5 * math.sqrt(sum((hi[i] - lo[i]) ** 2 for i in range(3))))
# Reproduce fit_camera(38) for a 640x480 render, then project every actual STL
# vertex through the vertical-FOV camera. Compare the pixel silhouette to this
# projection instead of assuming an axis-aligned screen-space aspect.
fov = math.radians(38.0)
aspect = 640.0 / 480.0
half_fov = min(fov * 0.5, math.atan(math.tan(fov * 0.5) * aspect))
distance = radius / math.sin(half_fov) * 1.2
direction = [0.50, -0.61, 0.25]
length = math.sqrt(sum(v * v for v in direction))
direction = [v / length for v in direction]
camera = [center[i] + distance * direction[i] for i in range(3)]
forward = [center[i] - camera[i] for i in range(3)]
length = math.sqrt(sum(v * v for v in forward))
forward = [v / length for v in forward]
right = [forward[1], -forward[0], 0.0]
length = math.sqrt(sum(v * v for v in right))
right = [v / length for v in right]
up = [right[1]*forward[2] - right[2]*forward[1],
      right[2]*forward[0] - right[0]*forward[2],
      right[0]*forward[1] - right[1]*forward[0]]
projected = []
for vertex in vertices:
    rel = [vertex[i] - camera[i] for i in range(3)]
    depth = sum(rel[i] * forward[i] for i in range(3))
    assert depth > 0, "transformed source mesh projects behind the camera"
    projected.append((sum(rel[i] * right[i] for i in range(3)) / depth / math.tan(fov * 0.5),
                      sum(rel[i] * up[i] for i in range(3)) / depth / math.tan(fov * 0.5)))
projected_aspect = ((max(v[0] for v in projected) - min(v[0] for v in projected)) /
                    (max(v[1] for v in projected) - min(v[1] for v in projected)))
image_aspect = (max(xs) - min(xs) + 1) / (max(ys) - min(ys) + 1)
assert abs(image_aspect - projected_aspect) <= 0.05, (
    f"rendered mug silhouette differs from projected transformed STL extents: "
    f"image={image_aspect:.3f}, projected={projected_aspect:.3f}, tolerance=0.05")
# Find the blue top lip and compare the dark opening immediately inside it.
top_y = min(ys)
top_x = [x for x,y,_ in points if y <= top_y + 5]
center_x = round(sum(top_x) / len(top_x))
inner = [luma(px(x,y)) for y in range(top_y+5,top_y+12) for x in range(center_x-6,center_x+1)]
rim = [luma(px(x,y)) for y in range(top_y,top_y+5) for x in range(center_x-8,center_x+8)]
inner_mean = sum(inner)/len(inner); rim_mean = sum(rim)/len(rim)
assert rim_mean > 12 and inner_mean < rim_mean * 0.80, f"mug cavity is not distinguishable: interior={inner_mean:.1f}, rim={rim_mean:.1f}"
# A lit floor patch and a cast-shadow patch are sampled below the mug. The shadow must be distinctly darker.
lit = [luma(px(x,y)) for y in range(300,316) for x in range(115,145)]
shadow = [luma(px(x,y)) for y in range(300,316) for x in range(370,400)]
lit_mean = sum(lit)/len(lit); shadow_mean = sum(shadow)/len(shadow)
assert lit_mean > shadow_mean * 1.25, f"no measurable floor shadow: lit={lit_mean:.1f}, shadow={shadow_mean:.1f}"
print(f"Mug QA: upright error={vertical_error:.1f}deg; image/projected STL aspect={image_aspect:.3f}/{projected_aspect:.3f}; mean luminance={mean:.1f}, p5-p95={p5:.1f}-{p95:.1f}; body sigma={spread:.1f}; cavity {inner_mean:.1f}/{rim_mean:.1f}; floor lit/shadow={lit_mean:.1f}/{shadow_mean:.1f}")
