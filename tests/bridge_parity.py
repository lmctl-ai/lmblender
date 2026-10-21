#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import math, sys
from png_metrics import decode

images=[decode(path) for path in sys.argv[1:3]]
assert images[0][:3]==images[1][:3], "bridge render dimensions/channels differ"
masks=[]
for w,h,c,rows in images:
    background=rows[0][:3]
    mask={(x//c,y) for y,row in enumerate(rows) for x in range(0,w*c,c)
          if math.sqrt(sum((row[x+k]-background[k])**2 for k in range(3)))>20}
    masks.append(mask)
intersection=masks[0]&masks[1]; union=masks[0]|masks[1]
iou=len(intersection)/len(union) if union else 0
w,h,c,a=images[0]; b=images[1][3]
diffs=[abs(a[y][x*c+k]-b[y][x*c+k]) for x,y in intersection for k in range(3)]
mad=sum(diffs)/len(diffs) if diffs else 255
assert iou>0.92, f"BlenderCLI-to-lmblender object silhouette parity fell below 0.92 IoU: {iou:.4f}"
assert mad<44, f"BlenderCLI-to-lmblender common-mask color MAD exceeded 44/255: {mad:.2f}"
print(f"bridge parity: silhouette IoU={iou:.4f}; common-mask RGB MAD={mad:.2f}/255; objects pixels={len(masks[0])}/{len(masks[1])}")
