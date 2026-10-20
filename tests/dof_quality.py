import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"expected 640x480 DOF render, got {w}x{h}"
def mask(x,y,kind):
    p=rows[y][x*c:x*c+3]
    return (p[0] > p[1]*1.3 and p[0] > p[2]*1.2) if kind=="red" else (p[2] > p[0]*1.2 and p[2] > p[1]*1.2)
def sharpness(kind):
    edge=[]
    for y in range(1,h-1):
      for x in range(1,w-1):
        if mask(x,y,kind):
          # Color change over a two-pixel cross is high on sharply bounded objects.
          p=rows[y][x*c:x*c+3]
          q=rows[y][(x+2)*c:(x+2)*c+3]
          edge.append(sum(abs(p[k]-q[k]) for k in range(3)))
    assert len(edge)>40, f"too few {kind} object pixels to measure focus"
    return sum(edge)/len(edge)
near,far=sharpness("red"),sharpness("blue")
print(f"DOF edge contrast: focus red={near:.2f}, far blue={far:.2f}, ratio={near/max(far,1):.2f}")
assert near > far*1.15, "focus-plane object should have at least 15% greater edge contrast than the far object"
