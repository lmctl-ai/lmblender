#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import pathlib
import sys

from png_metrics import decode

w, h, channels, rows = decode(pathlib.Path(sys.argv[1]))
assert (w, h) == (640, 480), f"expected 640x480 backface test, got {w}x{h}"
green = red = 0
for y, row in enumerate(rows):
    for x in range(w):
        r, g, b = row[x * channels:x * channels + 3]
        if x < w // 2 and g > 45 and g > r * 1.35 and g > b * 1.35:
            green += 1
        if x >= w // 2 and r > 45 and r > g * 1.35 and r > b * 1.35:
            red += 1
assert green > 1500, f"two-sided backside panel missing on left ({green} green pixels)"
assert red < 500, f"backface-culling panel remained visible on right ({red} red pixels)"
print(f"backface culling: two-sided backside {green} green pixels; culled backside {red} red pixels")
