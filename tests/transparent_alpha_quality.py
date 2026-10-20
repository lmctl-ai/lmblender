import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"expected 640x480 RGBA render, got {w}x{h}"
assert c==4, f"transparent film PNG must have RGBA channels, got {c}"
alphas=[row[x*c+3] for row in rows for x in range(w)]
transparent=sum(a<16 for a in alphas)
opaque=sum(a>240 for a in alphas)
print(f"Transparent-film QA: transparent={transparent}, opaque={opaque}, total={w*h}")
assert transparent > w*h//3, "transparent world pixels should occupy at least one third of frame"
assert 100 < opaque < w*h//2, "rendered smooth sphere should contribute a nontrivial opaque silhouette"
