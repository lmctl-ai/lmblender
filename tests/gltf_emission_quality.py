#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
assert (w, h) == (640, 480), f"emissive glTF fixture rendered at unexpected size {(w, h)}"
red = green = blue = count = 0
for row in rows:
    for x in range(0, w * channels, channels):
        r, g, b = row[x:x+3]
        if r > 70 and r > g * 1.35 and r > b * 1.5:
            red += r; green += g; blue += b; count += 1
assert count > 500, f"KHR_materials_emissive_strength produced no visible warm emission mask ({count} pixels)"
assert red / count > 1.3 * max(1, green / count), f"emissive material lost its orange hue: RGB mean={red/count:.1f},{green/count:.1f},{blue/count:.1f}"
print(f"glTF emission QA: warm emissive pixels={count}, mean RGB={red/count:.1f},{green/count:.1f},{blue/count:.1f}")
