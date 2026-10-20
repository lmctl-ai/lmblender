#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Assert isotropic sphere and face-on cube projections at four image aspects."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

root = Path(sys.argv[1])
for kind, predicate in (
    ("sphere", lambda r, g, b: r > 30 and r > g * 1.3 and r > b * 1.3),
    ("cube", lambda r, g, b: g > 70 and g > r * 1.7 and g > b * 1.4),
):
    for aspect in ("16x9", "4x3", "1x1", "9x16"):
        path = root / f"camera-aspect-{kind}-{aspect}.png"
        w, h, c, rows = decode(path)
        pts = []
        for y, row in enumerate(rows):
            for x in range(w):
                r, g, b = row[x*c:x*c+3]
                if predicate(r, g, b):
                    pts.append((x, y))
        assert len(pts) > 100, f"{kind} mask absent or too small in {path}: {len(pts)} pixels"
        box = (min(x for x, _ in pts), min(y for _, y in pts),
               max(x for x, _ in pts), max(y for _, y in pts))
        bw, bh = box[2] - box[0] + 1, box[3] - box[1] + 1
        ratio = bw / bh
        assert abs(ratio - 1.0) <= 0.02, (
            f"{kind} projection must remain isotropic in {path.name}: "
            f"mask={bw}x{bh}, ratio={ratio:.4f}, tolerance=2%")
        assert box[0] > 0 and box[1] > 0 and box[2] < w-1 and box[3] < h-1, (
            f"{kind} is unexpectedly clipped in {path.name}: {box} in {w}x{h}")
        print(f"{path.name}: mask={bw}x{bh}, ratio={ratio:.4f}")

for aspect in ("16x9", "4x3", "1x1", "9x16"):
    path = root / f"camera-aspect-ortho-cube-{aspect}.png"
    w, h, c, rows = decode(path)
    pts = []
    for y, row in enumerate(rows):
        for x in range(w):
            r, g, b = row[x*c:x*c+3]
            if g > 70 and g > r * 1.7 and g > b * 1.4:
                pts.append((x, y))
    assert len(pts) > 100, f"orthographic cube mask absent in {path}: {len(pts)} pixels"
    bw = max(x for x, _ in pts) - min(x for x, _ in pts) + 1
    bh = max(y for _, y in pts) - min(y for _, y in pts) + 1
    ratio = bw / bh
    assert abs(ratio - 1.0) <= 0.02, (
        f"orthographic projection must remain isotropic in {path.name}: "
        f"mask={bw}x{bh}, ratio={ratio:.4f}, tolerance=2%")
    print(f"{path.name}: mask={bw}x{bh}, ratio={ratio:.4f}")

for aspect in ("16x9", "4x3", "1x1", "9x16"):
    path = root / f"camera-sequence-sphere-{aspect}-0000.png"
    w, h, c, rows = decode(path)
    pts = [(x, y) for y, row in enumerate(rows) for x in range(w)
           for r, g, b in [row[x*c:x*c+3]] if r > 30 and r > g*1.3 and r > b*1.3]
    assert len(pts) > 100, f"render_sequence sphere missing in {path}: {len(pts)} pixels"
    bw = max(x for x, _ in pts) - min(x for x, _ in pts) + 1
    bh = max(y for _, y in pts) - min(y for _, y in pts) + 1
    ratio = bw / bh
    assert abs(ratio - 1.0) <= 0.02, (
        f"render_sequence must preserve isotropic projection in {path.name}: "
        f"mask={bw}x{bh}, ratio={ratio:.4f}, tolerance=2%")
    print(f"{path.name}: mask={bw}x{bh}, ratio={ratio:.4f}")
