#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import sys
from png_metrics import decode

path=sys.argv[1]
w,h,channels,rows=decode(path)
assert (w,h)==(640,480), f"glTF render must be 640x480, got {w}x{h}"
mask=[]
for y,row in enumerate(rows):
    for x in range(w):
        r,g,b=row[x*channels:x*channels+3]
        if max(r,g,b)-min(r,g,b)>32 and r>b*1.15 and r>40:
            mask.append((x,y,(r,g,b)))
assert len(mask)>2500, f"textured colored surface coverage too small: {len(mask)} pixels"
left=[p for p in mask if p[0]<w//2]
right=[p for p in mask if p[0]>=w//2]
assert len(left)>800 and len(right)>800, f"both transformed glTF nodes must render on opposite sides: left={len(left)}, right={len(right)}"
mx1=sum(p[0] for p in left)/len(left); mx2=sum(p[0] for p in right)/len(right)
assert mx2-mx1>100, f"node translations were not preserved: centroid gap {mx2-mx1:.1f}px"
colors={(p[2][0]//24,p[2][1]//24,p[2][2]//24) for p in mask}
assert len(colors)>=3, f"baseColorTexture and baseColorFactor variation missing: {len(colors)} color bins"
red=sum(r>g*1.6 and r>b*1.8 and r>75 for _,_,(r,g,b) in mask)
yellow=sum(r>g*1.05 and g>b*1.55 and g>75 for _,_,(r,g,b) in mask)
assert red>1000 and yellow>1000, f"checker must have distinct saturated red/yellow cells: red={red}, yellow={yellow}"
print(f"glTF image QA {w}x{h}: {len(mask)} colored subject pixels; node centroid gap={mx2-mx1:.1f}px; {len(colors)} color bins")
