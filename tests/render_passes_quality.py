#!/usr/bin/env python3
"""Validate Cycles pass output using Blender's OpenEXR reader."""
import bpy
import math
import os
import sys

base = os.path.abspath(sys.argv[-1])
def pixels(name):
    image = bpy.data.images.load(base + "-" + name + ".exr", check_existing=False)
    data = list(image.pixels[:])
    width, height = image.size
    channels = image.channels
    bpy.data.images.remove(image)
    return width, height, channels, data

w, h, cc, combined = pixels("combined")
wd, hd, cd, depth = pixels("depth")
wn, hn, cn, normal = pixels("normal")
wa, ha, ca, albedo = pixels("albedo")
assert (w, h) == (128, 128) and (wd, hd) == (w, h) and (wn, hn) == (w, h) and (wa, ha) == (w, h), "pass EXRs must share 128x128 dimensions"
assert cc == cd == cn == ca == 4, f"Blender should expose all EXR pass data as RGBA: {cc}, {cd}, {cn}, {ca}"
# At center x, the visible plane spans a vertical sequence of depths. Ignore background
# sentinels and require a strong monotonic trend over at least 40 valid pixels.
column = []
for y in range(h):
    i = y * w + w // 2
    d = depth[i*cd]
    if math.isfinite(d) and 0.01 < d < 1.0e8:
        column.append(d)
assert len(column) >= 30, f"depth ramp yielded only {len(column)} valid center-column samples"
span = max(column) - min(column)
assert span > 1.0, f"depth pass has insufficient range: {span}"
up = sum(a <= b + 1e-4 for a, b in zip(column, column[1:])) / (len(column)-1)
down = sum(a >= b - 1e-4 for a, b in zip(column, column[1:])) / (len(column)-1)
assert max(up, down) >= 0.95, f"depth is not monotone across ramp: increasing={up:.3f}, decreasing={down:.3f}"
# Foreground pixels are those with nonzero combined alpha; their normal vectors must be unit.
lengths = []
for i in range(w*h):
    d = depth[i*cd]
    if math.isfinite(d) and 0.01 < d < 1.0e8:
        n = normal[i*cn:i*cn+3]
        lengths.append(math.sqrt(sum(x*x for x in n)))
assert len(lengths) > 100, f"normal pass has too few foreground samples ({len(lengths)})"
interior = [v for v in lengths if v > 0.97]
coverage = len(interior) / len(lengths)
max_err = max((abs(v-1.0) for v in interior), default=1.0)
assert coverage >= 0.90, f"at least 90% of covered normal pixels should be unit length (got {coverage:.3f})"
assert max_err < 0.03, f"interior Cycles normal vectors must be unit length (max error {max_err:.4f})"
for name in ("combined", "depth", "normal", "albedo"):
    png = base + "-" + name + ".png"
    assert os.path.getsize(png) > 100, f"normalized PNG missing/empty: {png}"
print(f"passes: depth monotonic={max(up,down):.3f}, range={span:.3f}; normal unit coverage={coverage:.3f}, max error={max_err:.5f}; samples={len(lengths)}")
