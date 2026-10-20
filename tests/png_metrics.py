#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import struct
import sys
import zlib
from pathlib import Path


def decode(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"not a PNG: {path}")
    offset, chunks, header = 8, [], None
    while offset < len(data):
        size = struct.unpack(">I", data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + size]
        offset += size + 12
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            chunks.append(chunk)
        elif kind == b"IEND":
            break
    width, height, depth, color, compression, filtering, interlace = header
    channels = {2: 3, 6: 4}.get(color)
    if depth != 8 or channels is None or interlace:
        raise SystemExit("expected a non-interlaced 8-bit RGB/RGBA PNG")
    stride = width * channels
    raw = zlib.decompress(b"".join(chunks))
    rows, previous, pos = [], bytearray(stride), 0
    for _ in range(height):
        kind = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + stride])
        pos += stride
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if kind == 1:
                row[x] = (row[x] + left) & 255
            elif kind == 2:
                row[x] = (row[x] + up) & 255
            elif kind == 3:
                row[x] = (row[x] + (left + up) // 2) & 255
            elif kind == 4:
                estimate = left + up - upper_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upper_left))
                predictor = left if distances[0] <= min(distances[1:]) else up if distances[1] <= distances[2] else upper_left
                row[x] = (row[x] + predictor) & 255
            elif kind != 0:
                raise SystemExit(f"unsupported PNG filter {kind}")
        rows.append(row)
        previous = row
    return width, height, channels, rows


def main(path):
    w, h, channels, rows = decode(path)
    luma = []
    for row in rows:
        for x in range(0, w * channels, channels):
            r, g, b = row[x:x + 3]
            luma.append((r * 2126 + g * 7152 + b * 722) / 10000)
    mean = sum(luma) / len(luma)
    variance = sum((value - mean) ** 2 for value in luma) / len(luma)
    stddev = variance ** 0.5
    distinct = len(set(int(value) for value in luma))
    print(f"PNG {w}x{h}: luminance stddev={stddev:.2f}/255, distinct levels={distinct}")
    if w < 16 or h < 16 or stddev < 5 or distinct < 16:
        raise SystemExit("rendered PNG is blank or has insufficient visible pixel variation")


if __name__ == "__main__":
    main(sys.argv[1])
