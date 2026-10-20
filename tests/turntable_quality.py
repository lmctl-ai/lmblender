#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

"""Strict, auditable framing and distinct-view checks for lmcad turntables."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

root = Path(sys.argv[1])
for subject in ("mug", "bracket"):
    frames = []
    coverages = []
    bounds = []
    for i in range(8):
        path = root / f"compose-{subject}-angle-{i}.png"
        w, h, c, rows = decode(path)
        assert (w, h, c) == (128, 96, 4), f"{path}: expected 128x96 RGBA"
        flat = [v for row in rows for v in row]
        assert len(set(flat)) > 16, f"{path}: render is nearly uniform"
        if subject == "mug":
            # Ceramic is blue; ignore the bottom 6 pixels where the floor contact may tint the frame.
            points = [(x,y) for y in range(h-6) for x in range(w)
                      if rows[y][x*c+2] - rows[y][x*c] > 60]
        else:
            # The steel stands out as bright, low-chroma pixels; use the center region to avoid floor highlights.
            points = [(x,y) for y in range(8,h-8) for x in range(8,w-8)
                      if max(rows[y][x*c:x*c+3]) > 195 and
                      max(rows[y][x*c:x*c+3]) - min(rows[y][x*c:x*c+3]) < 90]
        coverage = len(points) / (w*h)
        lo_x, lo_y = min(x for x,y in points), min(y for x,y in points)
        hi_x, hi_y = max(x for x,y in points), max(y for x,y in points)
        assert 0.025 <= coverage <= 0.50, f"{path}: subject mask coverage {coverage:.3f} outside framing bounds"
        assert lo_x >= 4 and hi_x < w-4 and lo_y >= 4 and hi_y < h-4, f"{path}: subject mask touches a frame edge: {(lo_x,lo_y,hi_x,hi_y)}"
        coverages.append(coverage)
        bounds.append((lo_x,lo_y,hi_x,hi_y))
        frames.append(flat)
    deltas = []
    for i in range(8):
        a, b = frames[i], frames[(i+1)%8]
        mad = sum(abs(x-y) for x,y in zip(a,b)) / len(a)
        deltas.append(mad)
    assert min(deltas) > 1.0, f"{subject}: adjacent camera views are not distinct enough: {deltas}"
    print(f"{subject}: 8/8 views framed; mask coverage={min(coverages):.3f}..{max(coverages):.3f}; "
          f"adjacent RGB MAD min/mean={min(deltas):.2f}/{sum(deltas)/8:.2f}; bboxes={bounds}")
