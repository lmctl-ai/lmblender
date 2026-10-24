#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""depth_range(near, far) spreads subject depths across the depth PNG; automatic stays the default.

usage: depth_range_quality.py AUTO-depth.png RANGED-depth.png
"""
import struct
import sys
import zlib


def gray(path):
    data = open(path, "rb").read()
    pos, idat, width, height, colour, depth = 8, b"", 0, 0, 0, 0
    while pos < len(data):
        n, = struct.unpack(">I", data[pos:pos + 4]); kind = data[pos + 4:pos + 8]; body = data[pos + 8:pos + 8 + n]; pos += 12 + n
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
    assert depth == 8, "expected an 8-bit depth PNG"
    raw, stride, rows, prev, i = zlib.decompress(idat), width * channels, [], bytearray(width * channels), 0
    for _ in range(height):
        f, line = raw[i], bytearray(raw[i + 1:i + 1 + stride]); i += 1 + stride
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0; b = prev[x]; c = prev[x - channels] if x >= channels else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line[0::channels])); prev = line
    return rows


def subjects(rows):
    # tests/depth-range.lms at 320x240: cube, sphere and cone centres.
    return rows[150][65], rows[115][160], rows[108][198]


auto, ranged = subjects(gray(sys.argv[1])), subjects(gray(sys.argv[2]))
auto_spread, ranged_spread = auto[0] - auto[2], ranged[0] - ranged[2]
print(f"depth PNG cube/sphere/cone: automatic {auto} spread {auto_spread}; depth_range {ranged} spread {ranged_spread}")
assert auto[0] > auto[1] > auto[2] and ranged[0] > ranged[1] > ranged[2], "depth must decrease with distance"
assert ranged_spread >= 120, f"depth_range did not widen subject contrast ({ranged_spread})"
assert ranged_spread > 2 * auto_spread, "depth_range should clearly beat automatic normalization here"
