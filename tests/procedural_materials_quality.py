#!/usr/bin/env python3
"""Verify checker and mapped noise show distinct, non-flat material pixels."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode
w,h,c,rows=decode(sys.argv[1])
sx,sy=w/512.0,h/384.0
def pixels(box):
    x0,y0,x1,y1=box
    x0,x1=round(x0*sx),round(x1*sx)
    y0,y1=round(y0*sy),round(y1*sy)
    return [tuple(rows[y][x*c:x*c+3]) for y in range(y0,y1) for x in range(x0,x1)]
checker=pixels((175,100,255,280)); noise=pixels((280,100,400,280))
red=[p for p in checker if p[0]>p[1]*1.4 and p[0]>p[2]*1.3 and p[0]>40]
blue=[p for p in noise if p[2]>p[0]*1.3 and p[2]>p[1]*0.9 and p[2]>30]
assert len(red)>500, f"checker object lacks saturated red checker cells: {len(red)} pixels"
assert len(blue)>1000, f"noise material lacks blue object pixels: {len(blue)} pixels"
assert len({tuple(v//8 for v in p) for p in red})>20, "checker render has insufficient color variation"
assert len({tuple(v//8 for v in p) for p in blue})>40, "noise texture appears flat instead of spatially varying"
print(f"Procedural material QA: checker red pixels={len(red)}, noise blue pixels={len(blue)}, noise color bins={len({tuple(v//8 for v in p) for p in blue})}")
