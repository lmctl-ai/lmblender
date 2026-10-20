import sys
from png_metrics import decode

w, h, c, rows = decode(sys.argv[1])
assert (w, h) == (640, 480), f"expected 640x480 sky render, got {w}x{h}"
def region(y0, y1):
    values = [sum(row[x*c:x*c+3]) / 3 for row in rows[y0:y1] for x in range(w)]
    assert values, f"no unclipped sky/background pixels in rows {y0}:{y1}"
    return sum(values) / len(values)
top = region(0, 80)
horizon = region(380, 460)
print(f"Sky numeric QA: top mean luminance={top:.2f}, horizon={horizon:.2f}, delta={top-horizon:.2f}")
assert abs(top-horizon) >= 2.0, "sky texture must show a measurable vertical sky gradient"
clipped = sum(1 for row in rows for x in range(w) if min(row[x*c:x*c+3]) >= 250) / (w*h)
assert clipped < 0.05, f"sky exposure clips too much of the image ({clipped:.1%})"
