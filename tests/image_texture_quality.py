#!/usr/bin/env python3
"""Check image-texture colors survive the Cycles image loading path."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
red = yellow = 0
top = [0, 0]
front = [0, 0]
for y in range(h):
    for x in range(w):
        r, g, b = rows[y][x * channels:x * channels + 3]
        # The rendered object is near the center; this excludes its neutral floor.
        if 45 < x < w - 45 and 20 < y < h - 35:
            is_red = r > 100 and r > g * 1.55 and r > b * 1.5
            is_yellow = r > 100 and g > 100 and r > b * 1.5 and g > b * 1.4
            if is_red:
                red += 1
                if 60 < y < 110 and 115 < x < 250: top[0] += 1
                if 105 < y < 180 and 115 < x < 225: front[0] += 1
            if is_yellow:
                yellow += 1
                if 60 < y < 110 and 115 < x < 250: top[1] += 1
                if 105 < y < 180 and 115 < x < 225: front[1] += 1
assert red > 500, f"red image-texture tiles not visible: {red} pixels"
assert yellow > 500, f"yellow image-texture tiles not visible: {yellow} pixels"
assert min(top) > 200, f"top face lacks both checker colors: {top}"
assert min(front) > 200, f"front face lacks both checker colors: {front}"
print(f"Image texture QA: red={red} pixels; yellow={yellow} pixels; top={top}; front={front}")
