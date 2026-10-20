#!/usr/bin/env python3
"""Falsifiable visual checks for the lmcad mug render (RGB PNG)."""
import math
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

path = sys.argv[1]
w, h, channels, rows = decode(path)
def px(x, y):
    return tuple(rows[y][x * channels:x * channels + 3])
def luma(rgb):
    r, g, b = rgb
    return (2126*r + 7152*g + 722*b) / 10000
# The mug is blue ceramic; this excludes the neutral gray floor/world and isolates its body.
points = []
for y in range(h):
    for x in range(w):
        r, g, b = px(x, y)
        if 170 <= x <= 330 and 45 <= y <= 245 and g > r * 1.3 and b > g * 1.05 and b > 18:
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
# Find the blue top lip and compare the dark opening immediately inside it.
top_y = min(ys)
top_x = [x for x,y,_ in points if y <= top_y + 5]
center_x = round(sum(top_x) / len(top_x))
inner = [luma(px(x,y)) for y in range(top_y+4,top_y+9) for x in range(center_x-5,center_x+1)]
rim = [luma(px(x,y)) for y in range(top_y,top_y+5) for x in range(center_x-8,center_x+8)]
inner_mean = sum(inner)/len(inner); rim_mean = sum(rim)/len(rim)
assert rim_mean > 12 and inner_mean < rim_mean * 0.80, f"mug cavity is not distinguishable: interior={inner_mean:.1f}, rim={rim_mean:.1f}"
# A lit floor patch and a cast-shadow patch are sampled below the mug. The shadow must be distinctly darker.
lit = [luma(px(x,y)) for y in range(210,226) for x in range(150,176)]
shadow = [luma(px(x,y)) for y in range(210,226) for x in range(300,326)]
lit_mean = sum(lit)/len(lit); shadow_mean = sum(shadow)/len(shadow)
assert lit_mean > shadow_mean * 1.25, f"no measurable floor shadow: lit={lit_mean:.1f}, shadow={shadow_mean:.1f}"
print(f"Mug QA: upright error={vertical_error:.1f}deg; body luminance sigma={spread:.1f}; cavity {inner_mean:.1f}/{rim_mean:.1f}; floor lit/shadow={lit_mean:.1f}/{shadow_mean:.1f}")
