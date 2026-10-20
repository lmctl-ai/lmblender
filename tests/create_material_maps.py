#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import math
import pathlib
import struct
import sys
import zlib

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
w = h = 64

def png(name, fn):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(max(0, min(255, round(c * 255))) for c in fn(x, y))
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    (out / name).write_bytes(data)

png('normal.png', lambda x,y: (
    0.5 + 0.25 * math.sin(x * 0.31) * math.cos(y * 0.23),
    0.5 + 0.24 * math.cos(x * 0.21) * math.sin(y * 0.29),
    0.90))
png('roughness.png', lambda x,y: ((0.18 + 0.72 * (x / (w - 1)),) * 3))
png('metallic.png', lambda x,y: ((0.08 if x < w // 2 else 0.82,) * 3))
png('ao.png', lambda x,y: ((0.35 + 0.65 * min(1.0, math.hypot(x-31.5,y-31.5)/45),) * 3))
png('mix.png', lambda x,y: ((0.08 if ((x//8 + y//8) & 1) else 0.92,) * 3))
