#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""The -Y face of an image-textured primitive cube shows distinct checker tiles."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
red = yellow = other = 0
for y in range(h):
    for x in range(w):
        r, g, b = rows[y][x * channels:x * channels + 3]
        if r > 120 and g < 0.55 * r and b < 0.5 * r:
            red += 1
        elif r > 120 and g > 0.75 * r and b < 0.5 * r:
            yellow += 1
        else:
            other += 1
total = w * h
print(f"primitive UV: red {red / total:.3f}, yellow {yellow / total:.3f}, other {other / total:.3f}")
assert red > 0.25 * total and yellow > 0.25 * total, "cube side face lacks distinct checker tiles (smeared UVs)"
