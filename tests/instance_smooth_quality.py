#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""An instance of a smooth-shaded mesh must render like its smooth source.

Flat shading of the 12x6 test sphere moves the lit pixels by roughly ten
levels on average; the same smooth geometry differs only by sampling noise.
"""
import sys
from png_metrics import decode

w1, h1, c1, a = decode(sys.argv[1])
w2, h2, c2, b = decode(sys.argv[2])
assert (w1, h1) == (w2, h2), "instance and reference sizes differ"
total = 0.0
count = 0
for y in range(h1):
    for x in range(w1):
        pa = a[y][x * c1:x * c1 + 3]
        pb = b[y][x * c2:x * c2 + 3]
        if max(pa) > 20 or max(pb) > 20:
            total += sum(abs(int(pa[i]) - int(pb[i])) for i in range(3)) / 3.0
            count += 1
assert count > 2000, f"sphere not visible ({count} lit pixels)"
mad = total / count
print(f"instance smooth shading: mean abs difference {mad:.2f}/255 over {count} lit pixels")
if mad > 3.0:
    raise SystemExit("an instance of a smooth mesh renders differently from its smooth source (flat-shaded)")
