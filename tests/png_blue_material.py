import sys
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
pixels = []
for row in rows:
    for x in range(0, w * channels, channels):
        rgb = row[x:x + 3]
        if max(rgb) > 40:
            pixels.append(rgb)
assert pixels, "material render has no object pixels above the background"
means = [sum(pixel[c] for pixel in pixels) / len(pixels) for c in range(3)]
print(f"Material object-pixel mean RGB={tuple(round(v, 1) for v in means)}")
assert means[2] > means[0] * 1.4 and means[2] > means[1] * 1.1, "blue Principled base color was not visible in the rendered object"
