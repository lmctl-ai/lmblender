# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:4] == bytes((0x76, 0x2f, 0x31, 0x01)), "output is not an OpenEXR file"
pos = 8
window = None
while pos < len(data):
    end = data.index(0, pos)
    if end == pos:
        break
    name = data[pos:end].decode("ascii")
    pos = end + 1
    end = data.index(0, pos)
    kind = data[pos:end].decode("ascii")
    pos = end + 1
    size = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    value = data[pos:pos + size]
    pos += size
    if name == "dataWindow" and kind == "box2i":
        window = struct.unpack("<4i", value)
assert window is not None, "EXR header has no dataWindow"
width, height = window[2] - window[0] + 1, window[3] - window[1] + 1
assert (width, height) == (128, 96), f"EXR dimensions were {(width, height)}, expected (128,96)"
assert len(data) > 1024, "EXR has no meaningful pixel payload"
print(f"OpenEXR valid: {width}x{height}, {len(data)} bytes")
