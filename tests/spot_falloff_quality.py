#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"spot test resolution changed: {w}x{h}"
red=[];green=[]
for y,row in enumerate(rows):
    for x in range(w):
        r,g,b=row[x*c:x*c+3]
        if x<w//2 and r>g*1.4 and r>b*1.5 and r>35: red.append((r+g+b)/3)
        if x>=w//2 and g>r*1.4 and g>b*1.05 and g>25: green.append((r+g+b)/3)
assert len(red)>1000 and len(green)>1000, f"spot receivers not visible: red={len(red)}, green={len(green)}"
red_mean=sum(red)/len(red);green_mean=sum(green)/len(green)
assert red_mean>green_mean*1.6, f"soft spot cone did not distinguish its aimed subject: on/off mean={red_mean:.1f}/{green_mean:.1f}"
print(f"spot falloff QA: aimed red subject={red_mean:.1f}, outside green subject={green_mean:.1f}, ratio={red_mean/green_mean:.2f}")
