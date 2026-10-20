#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

"""Compare a perspective reference and the same scene in orthographic projection."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

def orange_bbox(path):
    w,h,c,rows=decode(path); pts=[]
    for y,row in enumerate(rows):
        for x in range(w):
            r,g,b=row[x*c:x*c+3]
            if r>g*1.3 and r>40: pts.append((x,y))
    assert len(pts)>1000, f"orange test object missing from {path}"
    box=(min(x for x,y in pts),min(y for x,y in pts),max(x for x,y in pts),max(y for x,y in pts))
    assert box[0]>0 and box[1]>0 and box[2]<w-1 and box[3]<h-1, f"object cropped in {path}: {box}"
    return box
perspective=orange_bbox(sys.argv[1]); ortho=orange_bbox(sys.argv[2])
p_h=perspective[3]-perspective[1]; o_h=ortho[3]-ortho[1]
assert o_h>p_h*1.5, f"orthographic scale did not produce the expected stable larger framing: perspective={p_h}, ortho={o_h}"
print(f"Projection QA: perspective object height={p_h}px; orthographic={o_h}px; ratio={o_h/p_h:.2f}")
