#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""The textured glTF quad must be uniformly red: authored UVs, not a projection.

    gltf_uv_quality.py build/gltf-uv.png
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode  # noqa: E402

path = Path(sys.argv[1])
width, height, channels, rows = decode(path)
red = blue = lit = 0
for row in rows:
    for x in range(width):
        r, g, b = row[x * channels:x * channels + 3]
        if max(r, g, b) < 12:
            continue  # black world around the quad
        lit += 1
        if r > 60 and r > 2.5 * b and r > 2.5 * g:
            red += 1
        elif b > 60 and b > 2.5 * r and b > 2.5 * g:
            blue += 1
assert lit > 1500, f"{path.name}: quad not visible ({lit} lit pixels)"
assert red > 0.9 * lit, f"{path.name}: authored UVs ignored, only {red}/{lit} lit pixels are red"
assert blue == 0, f"{path.name}: {blue} blue pixels mean the texture was projected instead of sampled by TEXCOORD_0"
print(f"{path.name}: {red}/{lit} lit pixels red, {blue} blue; authored glTF UVs honoured")
