#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Auditable image checks for OBJ usemtl, UV texture, and opacity maps."""
import sys

from png_metrics import decode

width, height, channels, gallery = decode(sys.argv[1])
control_width, control_height, control_channels, control = decode(sys.argv[2])
no_bump_width, no_bump_height, no_bump_channels, no_bump = decode(sys.argv[3])
assert (width, height, channels) == (control_width, control_height, control_channels) and (width, height) == (640, 480) and channels in (3, 4), (
    "OBJ MTL gallery/control must be matching 640x480 RGB(A) images"
)
assert (width, height, channels) == (no_bump_width, no_bump_height, no_bump_channels), "OBJ MTL bump control must match gallery dimensions"

red, blue = [], []
red_delta, blue_delta = [], []
red_bump_delta = []
for y in range(100, 365):
    for x in range(45, 595):
        i = x * channels
        r, g, b = gallery[y][i:i+3]
        if x < width // 2 and r > 55 and r > g * 1.3 and r > b * 1.2:
            red.append((x, y))
            red_delta.extend(abs(gallery[y][i+c] - control[y][i+c]) for c in range(3))
            red_bump_delta.extend(abs(gallery[y][i+c] - no_bump[y][i+c]) for c in range(3))
        if x > width // 2 and b > 55 and b > r * 1.35 and b > g * 1.15:
            blue.append((x, y))
            blue_delta.extend(abs(gallery[y][i+c] - control[y][i+c]) for c in range(3))

assert len(red) > 2500, f"red material/UV-mapped cube is missing: {len(red)} red pixels"
assert len(blue) > 2500, f"blue material cube is missing: {len(blue)} blue pixels"
assert min(x for x, _ in red) > 20 and max(x for x, _ in blue) < width - 20, "OBJ MTL subjects are clipped"
red_mad = sum(red_delta) / len(red_delta)
blue_mad = sum(blue_delta) / len(blue_delta)
bump_mad = sum(red_bump_delta) / len(red_bump_delta)
assert red_mad > 7.0, f"map_Kd UV checker has too little effect relative to flat Kd: {red_mad:.2f}/255"
assert blue_mad > 1.0, f"map_d opacity has too little effect relative to opaque blue control: {blue_mad:.2f}/255"
assert bump_mad > 0.5, f"map_Bump has too little effect relative to no-bump control: {bump_mad:.2f}/255"
def bright_blue_face(x0, x1):
    return sum(
        gallery[y][x * channels] > 190
        and gallery[y][x * channels + 1] > 190
        and gallery[y][x * channels + 2] > 205
        for y in range(210, 270) for x in range(x0, x1)
    )

highlight = bright_blue_face(380, 430)
far_face = bright_blue_face(430, 480)
assert highlight > 250 and far_face < highlight * 0.3, (
    f"BlueGloss needs a localized specular highlight: lit={highlight}, far={far_face}"
)
print(f"OBJ MTL QA: red={len(red)}, blue={len(blue)}, UV texture MAD={red_mad:.2f}, alpha map MAD={blue_mad:.2f}, bump MAD={bump_mad:.2f}/255")
