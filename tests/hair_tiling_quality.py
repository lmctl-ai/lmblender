#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Adjacent hair_patch tiles are seeded differently: their top views must not correlate."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

w, h, c, rows = decode(sys.argv[1])
half = w // 2
left, right = [], []
for y in range(h):
    for x in range(half):
        left.append(sum(rows[y][x * c:x * c + 3]) / 3)
        right.append(sum(rows[y][(x + half) * c:(x + half) * c + 3]) / 3)
n = len(left)
ml, mr = sum(left) / n, sum(right) / n
cov = sum((a - ml) * (b - mr) for a, b in zip(left, right))
corr = cov / (sum((a - ml) ** 2 for a in left) * sum((b - mr) ** 2 for b in right)) ** 0.5
print(f"hair tiling: left/right correlation {corr:.3f}, mean coverage {ml:.1f}/{mr:.1f}")
assert abs(corr) < 0.3, f"adjacent hair patches repeat the same layout (correlation {corr:.3f})"
assert ml > 40 and mr > 40, "hair patches lost coverage"
