#!/usr/bin/env python3
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
# The mug is blue ceramic; this excludes the neutral gray floor/world and isolates its body.
points = []
for y in range(0, int(h * 0.82)):
    for x in range(int(w * 0.30), int(w * 0.70)):
        r, g, b = px(x, y)
        if g > r * 1.3 and b > g * 1.05 and b > 18:
            points.append((x, y, luma((r, g, b))))
assert len(points) > 1000, f"blue ceramic body missing or too small: {len(points)} classified pixels"
xs = [p[0] for p in points]; ys = [p[1] for p in points]
assert min(ys) > 0 and max(ys) < h - 1, "mug body is cropped by the image border"
mx = sum(xs) / len(xs); my = sum(ys) / len(ys)
xx = sum((x-mx)**2 for x in xs) / len(xs)
yy = sum((y-my)**2 for y in ys) / len(ys)
xy = sum((x-mx)*(y-my) for x,y,_ in points) / len(points)
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
        vertices.append(struct.unpack_from("<3f", raw, base + corner * 12))
spans = [max(v[i] for v in vertices) - min(v[i] for v in vertices) for i in range(3)]
# rotate_x(90) maps the STL's +Y axis to renderer +Z; X is unchanged.
source_aspect = spans[0] * 1.2 / spans[1]
image_aspect = (max(xs) - min(xs) + 1) / (max(ys) - min(ys) + 1)
assert 0.70 <= image_aspect <= 0.92, f"rendered mug proportions disagree with CAD projection: image aspect={image_aspect:.3f}, adjusted CAD X/Y={source_aspect:.3f}"
assert abs(image_aspect - source_aspect) <= 0.42, f"rendered silhouette aspect differs from source mesh extents: image={image_aspect:.3f}, source={source_aspect:.3f}"
# Find the blue top lip and compare the dark opening immediately inside it.
top_y = min(ys)
top_x = [x for x,y,_ in points if y <= top_y + 5]
center_x = round(sum(top_x) / len(top_x))
inner = [luma(px(x,y)) for y in range(top_y+4,top_y+9) for x in range(center_x-5,center_x+1)]
rim = [luma(px(x,y)) for y in range(top_y,top_y+5) for x in range(center_x-8,center_x+8)]
inner_mean = sum(inner)/len(inner); rim_mean = sum(rim)/len(rim)
assert rim_mean > 12 and inner_mean < rim_mean * 0.80, f"mug cavity is not distinguishable: interior={inner_mean:.1f}, rim={rim_mean:.1f}"
# A lit floor patch and a cast-shadow patch are sampled below the mug. The shadow must be distinctly darker.
lit = [luma(px(x,y)) for y in range(300,316) for x in range(115,145)]
shadow = [luma(px(x,y)) for y in range(300,316) for x in range(370,400)]
lit_mean = sum(lit)/len(lit); shadow_mean = sum(shadow)/len(shadow)
assert lit_mean > shadow_mean * 1.25, f"no measurable floor shadow: lit={lit_mean:.1f}, shadow={shadow_mean:.1f}"
print(f"Mug QA: upright error={vertical_error:.1f}deg; image/CAD aspect={image_aspect:.3f}/{source_aspect:.3f}; mean luminance={mean:.1f}, p5-p95={p5:.1f}-{p95:.1f}; body sigma={spread:.1f}; cavity {inner_mean:.1f}/{rim_mean:.1f}; floor lit/shadow={lit_mean:.1f}/{shadow_mean:.1f}")
