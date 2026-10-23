#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import math, sys
from png_metrics import decode

# Usage: bridge_parity.py REFERENCE.png RENDER.png MIN_IOU MAX_MAD LABEL
min_iou=float(sys.argv[3]) if len(sys.argv)>3 else 0.92
max_mad=float(sys.argv[4]) if len(sys.argv)>4 else 44.0
label=sys.argv[5] if len(sys.argv)>5 else "BlenderCLI-to-lmblender"
images=[decode(path) for path in sys.argv[1:3]]
assert images[0][:2]==images[1][:2], "bridge render dimensions differ"
masks=[]
for w,h,c,rows in images:
    background=rows[0][:3]
    mask={(x//c,y) for y,row in enumerate(rows) for x in range(0,w*c,c)
          if math.sqrt(sum((row[x+k]-background[k])**2 for k in range(3)))>20}
    masks.append(mask)
intersection=masks[0]&masks[1]; union=masks[0]|masks[1]
iou=len(intersection)/len(union) if union else 0
w,h,ca,a=images[0]; cb,b=images[1][2],images[1][3]
diffs=[abs(a[y][x*ca+k]-b[y][x*cb+k]) for x,y in intersection for k in range(3)]
mad=sum(diffs)/len(diffs) if diffs else 255
assert iou>min_iou, f"{label} object silhouette parity fell below {min_iou} IoU: {iou:.4f}"
assert mad<max_mad, f"{label} common-mask color MAD exceeded {max_mad}/255: {mad:.2f}"
print(f"{label} parity: silhouette IoU={iou:.4f}; common-mask RGB MAD={mad:.2f}/255; objects pixels={len(masks[0])}/{len(masks[1])}")
