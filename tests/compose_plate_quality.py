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
# The eight open holes reveal neutral lit backdrop pixels through the copper surface.
dark=set()
for y in range(y0,y1+1):
    for x in range(x0,x1+1):
        r,g,b=rgb(x,y)
        if r < g * 1.5: dark.add((x,y))
components=[]
while dark:
    seed=dark.pop(); stack=[seed]; size=0; cx=[]; cy=[]
    while stack:
        x,y=stack.pop(); size+=1; cx.append(x); cy.append(y)
        for q in ((x+1,y),(x-1,y),(x,y+1),(x,y-1)):
            if q in dark: dark.remove(q); stack.append(q)
    if 20<=size<=200 and max(cx)-min(cx)<=15 and max(cy)-min(cy)<=16: components.append(size)
assert len(components)==8, f"expected 8 distinct backdrop openings, found {len(components)} regions: {sorted(components)}"
# The plate casts a measurable shadow onto its lit studio wall.
lit=sum(lum(rgb(x,y)) for y in range(160,220) for x in range(150,180))/ (60*30)
shadow=sum(lum(rgb(x,y)) for y in range(160,220) for x in range(315,335))/ (60*20)
assert lit>shadow*1.2, f"no measurable wall shadow: lit={lit:.1f}, shadow={shadow:.1f}"
print(f"Plate QA: copper pixels={len(red)}; openings={len(components)}; wall lit/shadow={lit:.1f}/{shadow:.1f}")
