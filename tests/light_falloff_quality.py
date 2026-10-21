#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from png_metrics import decode

w,h,c,rows=decode(sys.argv[1])
assert (w,h)==(640,480), f"light falloff scene resolution changed: {w}x{h}"
regions={"point-near":[],"point-far":[],"area-near":[],"area-far":[]}
for y,row in enumerate(rows):
    for x in range(w):
        r,g,b=row[x*c:x*c+3]; lum=(r+g+b)/3
        # The fixture uses a 30-degree vertical FOV to leave a clear gap
        # between the four projected receivers. Boundaries are intentionally
        # away from their edges so small CPU/denoiser differences cannot
        # reclassify the far sphere as the near receiver.
        if x<180 and r>g*1.45 and r>b*1.5 and r>35: regions["point-near"].append(lum)
        elif 180<=x<330 and r>g*1.45 and r>b*1.5 and r>25: regions["point-far"].append(lum)
        elif 330<=x<470 and b>r*1.35 and b>g*1.1 and b>35: regions["area-near"].append(lum)
        elif x>=470 and b>r*1.35 and b>g*1.1 and b>25: regions["area-far"].append(lum)
for name,pixels in regions.items(): assert len(pixels)>80, f"missing {name} diffuse receiver ({len(pixels)} pixels)"
means={name:sum(p)/len(p) for name,p in regions.items()}
assert means["point-near"]>means["point-far"]*1.18, f"point light falloff not measurable: {means}"
assert means["area-near"]>means["area-far"]*1.12, f"area light falloff not measurable: {means}"
print("light falloff QA: " + ", ".join(f"{k}={v:.1f}" for k,v in means.items()))
