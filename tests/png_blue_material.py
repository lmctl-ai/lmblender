import sys
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
background = rows[0][0:3]
pixels = []
for row in rows:
    for x in range(0, w * channels, channels):
        rgb = row[x:x + 3]
        if max(abs(rgb[c] - background[c]) for c in range(3)) > 20:
            pixels.append(rgb)
assert pixels, "material render has no object pixels distinguishable from the world background"
means = [sum(pixel[c] for pixel in pixels) / len(pixels) for c in range(3)]
print(f"Material object-pixel mean RGB={tuple(round(v, 1) for v in means)}")
assert means[2] > means[0] * 1.4 and means[2] > means[1] * 1.1, "blue Principled base color was not visible in the rendered object"
print(f"World background sample RGB={tuple(background)}")
assert background[1] > background[0] and background[1] > background[2], "custom green world color was not visible in the rendered background"
