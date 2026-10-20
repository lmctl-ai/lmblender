#!/usr/bin/env python3
import sys
from png_metrics import decode

w1, h1, c1, a = decode(sys.argv[1])
w2, h2, c2, b = decode(sys.argv[2])
assert (w1, h1, c1) == (w2, h2, c2), "direct Cycles and XML oracle dimensions differ"
all_errors = []
object_errors = []
for row_a, row_b in zip(a, b):
    all_errors.extend(abs(x - y) for x, y in zip(row_a, row_b))
    for x in range(0, len(row_a), c1):
        if max(row_a[x:x + 3]) > 40 or max(row_b[x:x + 3]) > 40:
            object_errors.extend(abs(row_a[x + k] - row_b[x + k]) for k in range(3))
global_mad = sum(all_errors) / len(all_errors)
object_mad = sum(object_errors) / len(object_errors)
print(f"Direct Cycles vs XML oracle: full-frame MAD={global_mad:.2f}/255; object-pixel MAD={object_mad:.2f}/255")
if not object_errors or object_mad > 2:
    raise SystemExit("direct Cycles object pixels exceed XML-oracle MAD threshold 2/255")
