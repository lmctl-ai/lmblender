#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Translating the Mesh returned by import_gltf must move every glTF node."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode


def checker_fraction(path):
    w, h, c, rows = decode(path)
    hit = 0
    for row in rows:
        for x in range(w):
            r, g, b = row[x * c:x * c + 3]
            hit += r > 120 and b < 90 and r - b > 80  # red or yellow checks, not the grey world
    return hit / (w * h)


kept, moved = checker_fraction(sys.argv[1]), checker_fraction(sys.argv[2])
print(f"glTF transform: checker coverage {kept:.2f} in place, {moved:.2f} after translating the import away")
assert kept > 0.2, f"gallery.glb panels not visible ({kept:.2f})"
assert moved < 0.005, f"part of the glTF stayed behind after translate ({moved:.2f}; v0.3.6 moved only the first node)"
