#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Write a valid PNG that is tiny on disk but decodes to 20000x20000 pixels.

The host must refuse it from the header alone (IMAGE_LIMIT) rather than let
OpenImageIO or Cycles allocate the 400-megapixel image at render time.
"""
import struct
import sys
import zlib
from pathlib import Path

WIDTH = HEIGHT = 20000


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def main():
    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)
    # 1-bit greyscale: each row is one filter byte plus WIDTH/8 bytes of zeros.
    row = b"\x00" + bytes(WIDTH // 8)
    compressor = zlib.compressobj(9)
    body = b"".join(compressor.compress(row) for _ in range(HEIGHT)) + compressor.flush()
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 1, 0, 0, 0, 0)
    out.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", body) + chunk(b"IEND", b""))
    print(f"{out}: {WIDTH}x{HEIGHT} PNG, {out.stat().st_size} bytes")


if __name__ == "__main__":
    main()
