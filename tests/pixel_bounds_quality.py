#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Assert the brightest RGB channel of a PNG lies in [min, max] (0..255).

    pixel_bounds_quality.py IMAGE MIN MAX
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode  # noqa: E402

path, lower, upper = Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
width, height, channels, rows = decode(path)
peak = max(row[x * channels + c] for row in rows for x in range(width) for c in range(min(3, channels)))
assert lower <= peak <= upper, f"{path.name}: brightest channel {peak} is outside [{lower}, {upper}]"
print(f"{path.name}: brightest channel {peak} within [{lower}, {upper}]")
