#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Compare an outlined cube against the same render without edge strokes."""
import sys
from png_metrics import decode

w,h,c,outlined=decode(sys.argv[1])
cw,ch,cc,control=decode(sys.argv[2])
assert (w,h,c)==(cw,ch,cc) and (w,h)==(960,720), "loft/outline render size changed"
dark_edges=red=cyan=0
for y in range(230,500):
    for x in range(140,420):
        i=x*c
        r,g,b=outlined[y][i:i+3]
        cr,cg,cb=control[y][i:i+3]
        dark_edges+=cr>100 and cr>cg*1.4 and r<70 and g<70 and b<70
        red+=r>100 and r>g*1.5 and r>b*1.5
for row in outlined:
    for i in range(0,len(row),c):
        r,g,b=row[i:i+3]
        cyan+=g>r*1.4 and b>r*1.4 and g>80
assert dark_edges>1800, f"toon edge strokes are missing or too faint: {dark_edges} darkened red-edge pixels"
assert red>15000, f"outlined cube's red core is obscured: {red} pixels"
assert cyan>8000, f"lofted cyan profile is missing: {cyan} pixels"
print(f"loft/outline QA: {dark_edges} dark edge pixels, {red} red core pixels, {cyan} cyan loft pixels")
