#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from png_metrics import decode

def load(path):
    width, height, channels, rows = decode(path)
    return width, height, channels, rows

def mean_region(im, x0, y0, x1, y1):
    w, h, ch, rows = im
    total = count = 0
    for y in range(y0, min(y1, h)):
        row = rows[y]
        for x in range(x0, min(x1, w)):
            off = x * ch
            total += (row[off] * 2126 + row[off + 1] * 7152 + row[off + 2] * 722) / 10000
            count += 1
    return total / max(1, count)

frames = [load(path) for path in sys.argv[1:4]]
assert all(im[:3] == frames[0][:3] for im in frames), "animation outputs must share dimensions and channels"
# Left third contains the keyed visibility object. Its blue mask must vanish only at frame 1.
blue = []
for im in frames:
    w, h, ch, rows = im
    count = 0
    for row in rows:
        for x in range(min(160, w)):
            off = x * ch
            r, g, b = row[off:off+3]
            if b > 90 and b > r * 1.4: count += 1
    blue.append(count)
assert blue[0] > 120 and blue[2] > 120, f"visibility object should appear at frames 0 and 2, got {blue}"
assert blue[1] < 15, f"keyframe_visible(false) must remove the mesh at frame 1, got {blue[1]} pixels"
# Emissive sphere is centered. Emission key strength must create a clear center-frame luminance increase.
y = [mean_region(im, 205, 95, 275, 175) for im in frames]
assert y[1] > y[0] * 1.35 and y[1] > y[2] * 1.35, f"keyframed emission should brighten frame 1, got {y}"
# A wider vertical FOV at frame 1 makes the static right sphere measurably smaller.
areas = []
for im in frames:
    w, h, ch, rows = im
    count = 0
    for yy in range(80, min(210, h)):
        row = rows[yy]
        for x in range(260, min(360, w)):
            off = x * ch
            if sum(row[off:off+3]) / 3 > 80: count += 1
    areas.append(count)
assert areas[0] > 40 and areas[2] > 40 and areas[1] > 20 and areas[1] < areas[0] * 0.85 and areas[1] < areas[2] * 0.85, f"animated vertical FOV should zoom out at frame 1, got {areas}"
print(f"animation QA PASS: visibility={blue}, center_luminance={y}, right_roi_area={areas}")
