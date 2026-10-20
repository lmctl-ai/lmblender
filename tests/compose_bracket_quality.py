#!/usr/bin/env python3
"""Numeric image checks for three lmcad bracket turntable views."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

path, view = sys.argv[1:3]
w, h, channels, rows = decode(path)
assert (w, h) == (512, 384), f"{view}: unexpected image size {(w, h)}"
values = []
steel_pixels = 0
for y in range(70, 320):
    for x in range(140, 372):
        r, g, b = rows[y][x * channels:x * channels + 3]
        values.append(0.2126 * r + 0.7152 * g + 0.0722 * b)
        if b > r * 1.15 and b - r > 10:
            steel_pixels += 1
values.sort()
mean = sum(values) / len(values)
stddev = (sum((value - mean) ** 2 for value in values) / len(values)) ** 0.5
spread = values[int(0.95 * len(values))] - values[int(0.05 * len(values))]
assert steel_pixels > 5000, f"{view}: steel-colored bracket region missing ({steel_pixels} px)"
assert stddev > 25 and spread > 60, f"{view}: image lacks object/light contrast (stddev={stddev:.1f}, p95-p5={spread:.1f})"
print(f"bracket {view}: steel={steel_pixels} px, ROI sigma={stddev:.1f}, p95-p5={spread:.1f}")
