#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""A 1/30-unit checker floor seen from a few units away must not alias into moire."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

w, h, c, rows = decode(sys.argv[1])


def high_frequency(y0, y1):
    total = count = 0
    for y in range(int(y0 * h), int(y1 * h)):
        row = rows[y]
        for x in range(1, w):
            total += abs(sum(row[x * c:x * c + 3]) - sum(row[(x - 1) * c:(x - 1) * c + 3])) / 3
            count += 1
    return total / count


near = high_frequency(0.85, 1.0)
print(f"checker moire: near-floor neighbour difference {near:.1f} (v0.3.6 aliased at 32.6)")
assert near < 15.0, f"sub-pixel checker floor aliases into moire ({near:.1f})"
