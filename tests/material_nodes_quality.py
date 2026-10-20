#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import sys
from pathlib import Path
from png_metrics import decode

def pixels(path):
    w,h,c,rows=decode(path); rgb=[]
    for row in rows:
        for x in range(w): rgb.append(tuple(row[x*c:x*c+3]))
    return w,h,rgb
w,h,a=pixels(sys.argv[1]);w2,h2,b=pixels(sys.argv[2]);assert (w,h)==(w2,h2)==(640,480), 'material graph renders must be 640x480'
changes=[sum(abs(x-y) for x,y in zip(pa,pb))/3 for pa,pb in zip(a,b)]
# Restrict the metric to colored subject pixels from either render, excluding the shared floor/background.
mask=[max(p)>55 and max(p)-min(p)>14 or max(q)>55 and max(q)-min(q)>14 for p,q in zip(a,b)]
subject=[d for d,m in zip(changes,mask) if m]
mad=sum(subject)/len(subject); distinct=len({p for p,m in zip(a,mask) if m})
assert len(subject)>12000, f'expected two visible shader subjects, found {len(subject)} subject pixels'
assert mad>2.0, f'bump/ramp graph should change subject shading vs flat control; MAD={mad:.3f}/255'
assert distinct>400, f'procedural nodes should create textured tonal variation; only {distinct} colors'
print(f'material graph QA: subject pixels={len(subject)}, control MAD={mad:.3f}/255, unique colors={distinct}')
