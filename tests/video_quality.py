#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import sys
from pathlib import Path

from png_metrics import decode


def pixels(path):
    width, height, channels, rows = decode(path)
    values = [tuple(row[i:i+3]) for row in rows for i in range(0, width*channels, channels)]
    lum = [(r*2126 + g*7152 + b*722)/10000 for r,g,b in values]
    mean = sum(lum)/len(lum)
    std = (sum((v-mean)**2 for v in lum)/len(lum))**0.5
    return width, height, values, std


def delta(a, b):
    return sum(sum(abs(x-y) for x,y in zip(p,q)) for p,q in zip(a,b))/(len(a)*3)


def red_centroid(frame):
    width, height, values, _ = frame
    selected=[(i%width,i//width) for i,(r,g,b) in enumerate(values) if r>120 and r>g*1.45 and r>b*1.35]
    if not selected: return None
    return (sum(x for x,_ in selected)/len(selected),sum(y for _,y in selected)/len(selected),len(selected))


def main(directory):
    root=Path(directory)
    results={}
    for level in range(1,5):
        paths=[root/f"level{level}-{frame}.png" for frame in range(24)]
        decoded=[pixels(path) for path in paths]
        assert all(w==640 and h==360 for w,h,_,_ in decoded), f"level {level}: expected 24 640x360 frames"
        assert all(std>8 for *_,std in decoded), f"level {level}: at least one frame is too flat/blank"
        animation_delta=max(delta(decoded[0][2],frame[2]) for frame in decoded[1:])
        if level == 1:
            assert animation_delta>0.4, "level 1 camera peek must visibly reframe the Coder and Research groups"
        else:
            start, finish=red_centroid(decoded[0]),red_centroid(decoded[-1])
            assert start and finish and abs(finish[0]-start[0])>60, f"level {level}: red message token must travel at least 60 pixels; {start} -> {finish}"
        results[level]=decoded
        if level == 1:
            print(f"level 1: 24 640x360 frames; camera peek RGB delta={animation_delta:.2f}/255")
        else:
            print(f"level {level}: message token x centroid {start[0]:.1f}->{finish[0]:.1f}px ({abs(finish[0]-start[0]):.1f}px travel)")
    final=results[4][-1][2]
    green=sum(1 for r,g,b in final if g>80 and g>r*1.35 and g>b*1.15)
    assert green>100, f"pipeline final frame: green checkmark missing or too small ({green} green pixels)"
    print(f"level 4 final: green check region={green} pixels")
    flow=results[3][0][2]
    width=640
    occupied=[]
    for cx in (64, 224, 416, 576):
        count=0
        for y in range(80,290):
            for x in range(cx-35,cx+36):
                r,g,b=flow[y*width+x]
                if max(r,g,b)>85 and max(r,g,b)-min(r,g,b)>12: count+=1
        occupied.append(count)
    assert min(occupied)>80, f"human/lead/team/result cards not all visibly present: {occupied}"
    print(f"level 3 flow card-region pixels (human,lead,team,result): {occupied}")


if __name__=="__main__":
    main(sys.argv[1])
