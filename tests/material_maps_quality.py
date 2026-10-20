#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import pathlib
import sys

from png_metrics import decode

root = pathlib.Path(sys.argv[1])

def pixels(path):
    width, height, channels, rows = decode(path)
    return width, height, channels, rows

mapped = pixels(root / "material-maps.png")
control = pixels(root / "material-maps-control.png")
assert mapped[:3] == control[:3], "material-map render and control dimensions must match"
w, h, channels, rows = mapped
control_rows = control[3]
roi = []
for y in range(170, 320):
    for x in range(165, 310):
        a = rows[y][x * channels:x * channels + 3]
        b = control_rows[y][x * channels:x * channels + 3]
        roi.extend(abs(a[i] - b[i]) for i in range(3))
mad = sum(roi) / len(roi)
assert mad > 4.0, f"normal/roughness/metallic/AO maps produced too little image change ({mad:.2f}/255)"

layered = pixels(root / "material-maps.png")
factor = pixels(root / "material-mix-factor.png")
assert layered[:3] == factor[:3], "layer-mix outputs must have matching dimensions"
_, _, ch, tex_rows = layered
_, _, _, factor_rows = factor
mask_gold = mask_blue = 0
for y in range(180, 310):
    for x in range(335, 465):
        r, g, b = tex_rows[y][x * ch:x * ch + 3]
        if r > 55 and r > b * 1.28:
            mask_gold += 1
        if b > 55 and b > r * 1.25:
            mask_blue += 1
assert mask_gold > 1500 and mask_blue > 1500, (
    f"texture-driven shader mix lacks both gold and blue regions ({mask_gold}, {mask_blue})"
)
deltas = []
for y in range(180, 310):
    for x in range(335, 465):
        a = tex_rows[y][x * ch:x * ch + 3]
        b = factor_rows[y][x * ch:x * ch + 3]
        deltas.extend(abs(a[i] - b[i]) for i in range(3))
mix_mad = sum(deltas) / len(deltas)
assert mix_mad > 5.0, f"texture factor and constant factor render too similarly ({mix_mad:.2f}/255)"
print(f"material map ROI MAD={mad:.2f}/255; texture-mix gold/blue pixels={mask_gold}/{mask_blue}; texture-vs-factor MAD={mix_mad:.2f}/255")
