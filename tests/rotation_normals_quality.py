#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode

def rgb(path):
    w, h, c, rows = decode(path)
    return w, h, [v for row in rows for v in row]

w1, h1, a = rgb(sys.argv[1]); w2, h2, b = rgb(sys.argv[2])
assert (w1, h1) == (w2, h2) and len(a) == len(b)
mad = sum(abs(x-y) for x,y in zip(a,b)) / len(a)
assert mad < 0.001, f"rotated explicit normals diverge from pre-rotated reference: MAD={mad:.3f}"
print(f"rotated explicit-normal render parity: RGB MAD={mad:.3f}")
