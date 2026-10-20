import sys
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
background = rows[0][:3]
points = []
for y, row in enumerate(rows):
    for x in range(0, w * channels, channels):
        rgb = row[x:x + 3]
        if max(abs(rgb[c] - background[c]) for c in range(3)) > 30:
            points.append((x // channels, y))
assert points, "camera test has no visible foreground"
cx = sum(p[0] for p in points) / len(points)
cy = sum(p[1] for p in points) / len(points)
print(f"Camera foreground centroid=({cx:.1f}, {cy:.1f}) in {w}x{h}")
assert abs(cx - (w - 1) / 2) < w * 0.12, "camera look-at target is not horizontally framed"
assert abs(cy - (h - 1) / 2) < h * 0.12, "camera look-at target is not vertically framed"
