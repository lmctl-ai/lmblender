#!/usr/bin/env python3
"""Image QA for the perforated lmcad plate composition."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
def rgb(x,y): return tuple(rows[y][x*c:x*c+3])
def lum(v): return (2126*v[0]+7152*v[1]+722*v[2])/10000
red=[]
for y in range(h):
    for x in range(w):
        r,g,b=rgb(x,y)
        if r>g*1.3 and r>35 and g>b*1.25: red.append((x,y))
assert len(red)>5000, f"copper plate color mask too small: {len(red)} pixels"
x0=min(x for x,y in red); x1=max(x for x,y in red); y0=min(y for x,y in red); y1=max(y for x,y in red)
assert x0>0 and y0>0 and x1<w-1 and y1<h-1, "plate is cropped by image boundary"
# Check all eight expected opening centers against nearby copper. This is more
# robust than connected components because view-transform compression blends
# antialiased hole edges into surrounding metal.
plate_w=x1-x0; plate_h=y1-y0
hole_x=[x0+round(plate_w*f) for f in (0.13,0.37,0.61,0.86)]
hole_y=[y0+round(plate_h*f) for f in (0.24,0.70)]
opening_scores=[]
for cy in hole_y:
    for cx in hole_x:
        r,g,b=rgb(cx,cy)
        assert abs(r-g)<35 and abs(g-b)<25, f"expected neutral backdrop through opening at {(cx,cy)}, got {(r,g,b)}"
        neighbors=[rgb(cx+dx,cy) for dx in (-7,7)]
        assert all(q[0]>q[1]*1.2 and q[1]>q[2]*1.15 for q in neighbors), f"opening at {(cx,cy)} lacks copper rim contrast: {neighbors}"
        opening_scores.append((cx,cy))
# The plate casts a measurable shadow onto its lit studio wall.
lit=sum(lum(rgb(x,y)) for y in range(160,220) for x in range(150,180))/ (60*30)
shadow=sum(lum(rgb(x,y)) for y in range(160,220) for x in range(315,335))/ (60*20)
assert lit>shadow*1.2, f"no measurable wall shadow: lit={lit:.1f}, shadow={shadow:.1f}"
print(f"Plate QA: copper pixels={len(red)}; openings={len(opening_scores)}; wall lit/shadow={lit:.1f}/{shadow:.1f}")
