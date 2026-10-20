#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import sys
from png_metrics import decode

w1, h1, c1, a = decode(sys.argv[1])
w2, h2, c2, b = decode(sys.argv[2])
assert (w1, h1, c1) == (w2, h2, c2), "direct Cycles and XML oracle dimensions differ"
mask_a = {(x // c1, y) for y, row in enumerate(a) for x in range(0, len(row), c1) if max(row[x:x + 3]) > 100}
mask_b = {(x // c2, y) for y, row in enumerate(b) for x in range(0, len(row), c2) if max(row[x:x + 3]) > 100}
union = mask_a | mask_b
iou = len(mask_a & mask_b) / len(union) if union else 0.0
print(f"Direct Cycles vs XML oracle: silhouette IoU={iou:.4f} ({len(mask_a)} vs {len(mask_b)} foreground pixels)")
if iou < 0.95:
    raise SystemExit("direct Cycles and XML oracle mesh silhouettes differ (IoU < 0.95)")
