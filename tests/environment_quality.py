import sys
from png_metrics import decode

w, h, c, rows = decode(sys.argv[1])
assert (w, h) == (640, 480), f"expected 640x480 environment render, got {w}x{h}"
def region(y0, y1):
    pixels = [row[x*c:x*c+3] for row in rows[y0:y1] for x in range(w)]
    return tuple(sum(p[k] for p in pixels)/len(pixels) for k in range(3))
top, bottom = region(0, 80), region(400, 480)
delta = sum(abs(top[k]-bottom[k]) for k in range(3))
print(f"Environment numeric QA: top RGB={tuple(round(x,1) for x in top)}, bottom RGB={tuple(round(x,1) for x in bottom)}, L1 delta={delta:.1f}/255")
assert delta >= 20, "equirectangular environment's upper/lower bands must remain distinguishable"
