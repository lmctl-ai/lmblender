#!/usr/bin/env python3
import pathlib
import sys

from png_metrics import decode

root = pathlib.Path(sys.argv[1])

def red_bounds(name):
    width, height, channels, rows = decode(root / name)
    xs = []
    for y, row in enumerate(rows):
        for x in range(width):
            r, g, b = row[x * channels:x * channels + 3]
            if r > 45 and r > g * 1.35 and r > b * 1.25:
                xs.append(x)
    assert len(xs) > 500, f"{name}: red subject mask too small ({len(xs)} pixels)"
    return min(xs), max(xs), len(xs)

blur = red_bounds("motion-blur-2.png")
control = red_bounds("motion-control.png")
blur_width = blur[1] - blur[0] + 1
control_width = control[1] - control[0] + 1
assert blur_width > control_width + 12, (
    f"motion streak was not measurable: blurred width {blur_width}px, "
    f"sharp width {control_width}px"
)
print(f"motion blur: silhouette width {control_width}px -> {blur_width}px (+{blur_width-control_width}px)")
