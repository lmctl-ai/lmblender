#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Sanity gates for the README story renders.

usage: story_quality.py NAME PNG WIDTH HEIGHT

Every render must decode, have the expected size, be neither black nor blown
out, and carry real contrast. A few stories also check the effect they show.
"""
import sys
from png_metrics import decode

name, path, want_w, want_h = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
w, h, c, rows = decode(path)
assert (w, h) == (want_w, want_h), f"{name}: size {w}x{h}, expected {want_w}x{want_h}"


def pixel(x, y):
    row = rows[y]
    return row[x * c], row[x * c + 1], row[x * c + 2]


def mean_box(x0, y0, x1, y1):
    total = [0, 0, 0]
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = pixel(x, y)
            for i in range(3):
                total[i] += p[i]
            n += 1
    return [v / n for v in total]


lum = []
for y in range(0, h, 2):
    for x in range(0, w, 2):
        r, g, b = pixel(x, y)
        lum.append(0.2126 * r + 0.7152 * g + 0.0722 * b)
mean = sum(lum) / len(lum)
std = (sum((v - mean) ** 2 for v in lum) / len(lum)) ** 0.5
clipped = sum(1 for v in lum if v >= 250) / len(lum)
assert 4.0 <= mean <= 225.0, f"{name}: mean luminance {mean:.1f} (black or blown out)"
assert std >= 10.0, f"{name}: luminance std {std:.1f} (flat image)"
assert clipped <= 0.15, f"{name}: {clipped:.1%} of pixels clipped"
extra = ""
if name == "backlit-skin":
    # Doubling the slab thickness must shift transmitted light toward red.
    ratios = []
    for cx in (w * 0.22, w * 0.405, w * 0.595, w * 0.78):
        r, g, b = mean_box(int(cx) - 20, int(h * 0.38), int(cx) + 20, int(h * 0.48))
        ratios.append(g / max(r, 1.0))
    assert all(a > b for a, b in zip(ratios, ratios[1:])), f"{name}: green/red not falling with thickness {ratios}"
    extra = " green/red " + " > ".join(f"{v:.2f}" for v in ratios)
elif name == "cornell-box":
    left = mean_box(int(w * 0.03), int(h * 0.4), int(w * 0.12), int(h * 0.6))
    right = mean_box(int(w * 0.88), int(h * 0.4), int(w * 0.97), int(h * 0.6))
    assert left[0] > 1.5 * left[1] and right[1] > 1.3 * right[0], f"{name}: walls {left} {right}"
    extra = " red and green walls present"
print(f"{name}: {w}x{h} mean {mean:.1f} std {std:.1f} clipped {clipped:.1%}{extra}")
