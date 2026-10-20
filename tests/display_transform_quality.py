#!/usr/bin/env python3
"""Compare uniform linear emission through lmblender and Blender's OCIO view."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

names = ("standard", "filmic", "agx")
paths = sys.argv[1:]
assert len(paths) == 6, "expected lmblender Standard/Filmic/AgX then Blender goldens"
for name, ours_path, ref_path in zip(names, paths[:3], paths[3:]):
    w, h, c, ours = decode(ours_path)
    rw, rh, rc, ref = decode(ref_path)
    assert (w,h) == (64,64) and (rw,rh) == (64,64), f"{name}: golden dimensions differ"
    errors=[]
    for y in range(16,48):
        for x in range(16,48):
            for channel in range(3):
                errors.append(ours[y][x*c+channel] - ref[y][x*rc+channel])
    rmse=(sum(value*value for value in errors)/len(errors))**0.5
    ours_mean=sum(ours[y][x*c+ch] for y in range(16,48) for x in range(16,48) for ch in range(3))/len(errors)
    ref_mean=sum(ref[y][x*rc+ch] for y in range(16,48) for x in range(16,48) for ch in range(3))/len(errors)
    assert rmse <= 3.0, f"{name}: RGB RMSE {rmse:.2f}/255 exceeds 3"
    print(f"{name}: center RGB RMSE={rmse:.2f}/255; means {ours_mean:.2f} vs Blender {ref_mean:.2f}")
