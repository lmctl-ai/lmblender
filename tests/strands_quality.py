#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
green=[];gold=[]
for y,row in enumerate(rows):
    for x in range(w):
        r,g,b=row[x*c:x*c+3]
        if g > r*1.25 and g > b*1.15 and g > 45: green.append((x,y))
        if r > b*1.35 and g > b*1.15 and r > 65 and x > w//2: gold.append((x,y))
assert (w,h)==(960,720), f"strand scene resolution changed: {w}x{h}"
assert len(green)>700, f"grass strand cluster is not visibly green: {len(green)} pixels"
assert len(gold)>1000, f"gold fur-ball and outward strands are not visibly present: {len(gold)} pixels"
assert min(x for x,y in green)>10 and max(x for x,y in gold)<w-10, "strand subjects are clipped"
print(f"strand showcase QA: green grass pixels={len(green)}, gold fur pixels={len(gold)}, frame={w}x{h}")
