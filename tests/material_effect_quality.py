import sys
from png_metrics import decode

def pixels(path):
    w,h,c,rows=decode(path)
    assert (w,h,c) == (640,480,4), f"{path}: expected 640x480 RGBA"
    rgb=[tuple(row[x:x+3]) for row in rows for x in range(0,len(row),c)]
    return [v for pixel in rgb for v in pixel], len(set(rgb))

(a,_),(b,unique)=map(pixels,sys.argv[1:])
mad=sum(abs(x-y) for x,y in zip(a,b))/len(a)
print(f"{sys.argv[2]}: mean absolute pixel delta vs opaque baseline={mad:.3f}/255; {unique} unique RGB colors")
assert unique >= 100, f"{sys.argv[2]}: output is visually flat ({unique} unique RGB colors), object/lighting result absent"
assert mad >= 0.35, f"{sys.argv[2]}: effect is not measurably different from opaque baseline (MAD {mad:.3f})"
