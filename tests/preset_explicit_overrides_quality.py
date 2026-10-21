#!/usr/bin/env python3
import sys
from pathlib import Path
from png_metrics import decode

root = Path(sys.argv[1])
images = {}
for name in ("before", "camera", "light"):
    w, h, channels, rows = decode(root / f"preset-override-{name}.png")
    assert (w, h) == (320, 240), f"{name}: expected 320x240, got {w}x{h}"
    images[name] = [[tuple(row[x * channels:x * channels + 3]) for x in range(w)] for row in rows]

def mask(image):
    # Red material isolates the subject from the generated studio floor.
    return {(x, y) for y, row in enumerate(image) for x, p in enumerate(row)
            if p[0] > 55 and p[0] > p[1] * 1.4 and p[0] > p[2] * 1.4}

before, camera = mask(images["before"]), mask(images["camera"])
union = before | camera
iou = len(before & camera) / max(1, len(union))
assert iou < 0.90, f"camera_look_at after product-studio had no measurable framing effect (mask IoU={iou:.4f})"

green = [(x, y, *pixel) for y, row in enumerate(images["light"]) for x, pixel in enumerate(row)
         if pixel[1] > 55 and pixel[1] > pixel[0] * 1.55 and pixel[1] > pixel[2] * 1.45]
assert len(green) > 300, f"explicit green area light did not replace the preset rig: green-dominant pixels={len(green)}"
center = [(r, g, b) for x, y, r, g, b in green if 100 < x < 220 and 55 < y < 185]
assert len(center) > 100, f"explicit green light missed the central subject: green pixels={len(center)}"
print(f"preset overrides: camera mask IoU={iou:.4f}; explicit green light pixels={len(green)}, subject={len(center)}")
