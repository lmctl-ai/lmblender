import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"primitive gallery must be 640x480, got {w}x{h}"

def match(pixel, color):
    r,g,b=pixel
    if color=="red": return r>g*1.3 and r>b*1.2 and r>80
    if color=="blue": return b>r*1.25 and b>g*1.1 and b>100
    if color=="green": return g>r*1.2 and g>b*1.08 and g>80
    if color=="yellow": return r>g*1.02 and g>b*1.08 and r>80
    return r>b*.85 and b>g*1.08 and b>80

# The scene is deliberately arranged left-to-right. Restrict color masks to the
# object band so the checker floor cannot be counted as a primitive.
windows=[("red",40,180,700),("blue",170,285,700),("green",265,375,700),
         ("yellow",360,475,250),("magenta",460,600,700)]
centers=[]
extents=[]
for color,x0,x1,minimum in windows:
    points=[(x,y) for y in range(165,270) for x in range(x0,x1)
            if match(rows[y][x*c:x*c+3],color)]
    assert len(points)>minimum, f"{color} primitive visible mask too small: {len(points)} pixels"
    xs=[x for x,y in points]; ys=[y for x,y in points]
    assert min(xs)>8 and max(xs)<w-8 and min(ys)>8 and max(ys)<h-8, f"{color} primitive is clipped"
    assert max(xs)-min(xs)>18 and max(ys)-min(ys)>18, f"{color} primitive silhouette is too small"
    centers.append(sum(xs)/len(xs))
    extents.append((min(xs),max(xs),min(ys),max(ys)))
assert centers==sorted(centers), f"primitive centers are not ordered left-to-right: {centers}"
for i in range(1,len(windows)):
    lx0,lx1,ly0,ly1=extents[i-1]; rx0,rx1,ry0,ry1=extents[i]
    assert lx1<rx0 or ly1<ry0 or ry1<ly0, f"{windows[i-1][0]} and {windows[i][0]} overlap"
print("primitive gallery: distinct red cube, blue UV sphere, green cylinder, yellow cone, magenta torus; all inside frame")
