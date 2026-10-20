# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import sys, math
from png_metrics import decode
w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"expected 640x480 DOF render, got {w}x{h}"
def color_mask(p,name):
 r,g,b=p[:3]
 return {"red":r>g*1.35 and r>b*1.25,"green":g>r*1.3 and g>b*1.2,"blue":b>r*1.3 and b>g*1.1}[name]
def luma(x,y):
 p=rows[y][x*c:x*c+3];return .2126*p[0]+.7152*p[1]+.0722*p[2]
centers={"red":(230,275),"green":(320,240),"blue":(375,210)}
def edge_contrast(name):
 vals=[]
 cx,cy=centers[name]
 for y in range(max(1,cy-55),min(h-1,cy+56)):
  for x in range(max(1,cx-50),min(w-1,cx+51)):
   if color_mask(rows[y][x*c:x*c+3],name) and not all(color_mask(rows[yy][xx*c:xx*c+3],name) for xx,yy in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))):
    vals.append(abs(luma(x+1,y)-luma(x-1,y))+abs(luma(x,y+1)-luma(x,y-1)))
 assert len(vals)>80, f"{name} sphere boundary mask too small for an edge measurement ({len(vals)} pixels)"
 return sum(vals)/len(vals),len(vals)
scores={n:edge_contrast(n) for n in ("red","green","blue")}
print("DOF mean boundary gradient (higher = sharper): "+", ".join(f"{n}={v[0]:.2f} ({v[1]} edge px)" for n,v in scores.items()))
assert scores["green"][0]>scores["red"][0]*1.15, "focus green sphere must have at least 15% steeper boundary gradient than near red sphere"
assert scores["green"][0]>scores["blue"][0]*1.15, "focus green sphere must have at least 15% steeper boundary gradient than far blue sphere"
