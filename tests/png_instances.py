# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import sys
from collections import deque
from png_metrics import decode

w, h, channels, rows = decode(sys.argv[1])
bg = rows[0][:3]
mask = [[False] * w for _ in range(h)]
for y, row in enumerate(rows):
    for x in range(w):
        rgb = row[x * channels:x * channels + 3]
        mask[y][x] = max(abs(rgb[c] - bg[c]) for c in range(3)) > 30
seen = set()
sizes = []
for y in range(h):
    for x in range(w):
        if not mask[y][x] or (x, y) in seen:
            continue
        q = deque([(x, y)])
        seen.add((x, y))
        size = 0
        while q:
            px, py = q.popleft()
            size += 1
            for ny in range(max(0, py - 1), min(h, py + 2)):
                for nx in range(max(0, px - 1), min(w, px + 2)):
                    if mask[ny][nx] and (nx, ny) not in seen:
                        seen.add((nx, ny))
                        q.append((nx, ny))
        sizes.append(size)
large = sorted((s for s in sizes if s >= 10), reverse=True)
print(f"Visible instance components >=10 pixels: {large}")
assert len(large) == 2, f"expected two separated rendered mesh instances, found {len(large)}"
