import sys, math
from png_metrics import decode
w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"expected 640x480 environment render, got {w}x{h}"
def region(x0,x1,y0,y1):
 return [rows[y][x*c:x*c+3] for y in range(y0,y1) for x in range(x0,x1)]
# The equirectangular map's bands must also appear as a reflection on the metal sphere.
ref=region(245,395,110,325)
means=[sum(p[k] for p in ref)/len(ref) for k in range(3)]
variance=sum(sum((p[k]-means[k])**2 for k in range(3)) for p in ref)/len(ref)
unique=len({tuple(p) for p in ref})
print(f"chrome environment reflection: RGB variance={variance:.1f}, unique colors={unique}")
assert variance>180 and unique>500, "chrome sphere must reflect multiple environment bands (not a flat wall/shard image)"
top=region(0,w,0,60);bottom=region(0,w,400,460)
delta=sum(abs(sum(p[k] for p in top)/len(top)-sum(p[k] for p in bottom)/len(bottom)) for k in range(3))
assert delta>30, f"environment image top-to-horizon color difference too small: {delta:.1f}"
