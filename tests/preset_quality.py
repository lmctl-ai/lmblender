#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Auditable content/coverage checks for each named presentation preset."""
import json
import sys
from pathlib import Path
from png_metrics import decode

root = Path(sys.argv[1])
requirements = {
    "diagram-dark-16x9-draft": ("orange", "blue"),
    "diagram-dark-16x9-final": ("orange", "blue"),
    "product-studio": ("blue",),
    "turntable-neutral": ("blue",),
    "glass-showcase": ("blue",),
    "technical-clean": ("orange",),
}
for name, expected in requirements.items():
    path = root / f"preset-{name}.png"
    w, h, channels, rows = decode(path)
    assert (w, h) == (320, 240), f"{name}: expected 320x240, got {w}x{h}"
    pixels = [(x, y, *row[x*channels:x*channels+3])
              for y, row in enumerate(rows) for x in range(w)]
    lum = [(r+g+b)/3 for _, _, r, g, b in pixels]
    mean, spread = sum(lum)/len(lum), max(lum)-min(lum)
    assert 8 < mean < 247, f"{name}: implausible mean luminance {mean:.2f}"
    assert spread > 35, f"{name}: insufficient contrast {spread:.1f}"
    masks = {
        "orange": [(x,y) for x,y,r,g,b in pixels if r>80 and r>g*1.35 and r>b*1.25],
        "blue": [(x,y) for x,y,r,g,b in pixels if b>100 and b>r*1.5 and b>g],
    }
    for color in expected:
        pts = masks[color]
        assert len(pts) > 100, f"{name}: expected visible {color} palette object, got {len(pts)} pixels"
        x0,x1=min(x for x,_ in pts),max(x for x,_ in pts)
        y0,y1=min(y for _,y in pts),max(y for _,y in pts)
        assert x0>0 and x1<w-1 and y0>0 and y1<h-1, f"{name}: {color} subject clipped at {(x0,y0,x1,y1)}"
    print(f"{name}: luminance mean={mean:.1f}, spread={spread:.1f}; " +
          ", ".join(f"{c} pixels={len(masks[c])}" for c in expected))

listed = (root / "presets-list.txt").read_text()
for name in requirements:
    assert name in listed, f"--presets omitted {name}"
shown = json.loads((root / "preset-product-studio.json").read_text())
assert shown["name"] == "product-studio" and shown["render"]["tier"] == "final"
assert shown["render"]["threads"] == 4 and shown["ground"]["enabled"] is True
print("preset CLI list/show JSON checks passed")
