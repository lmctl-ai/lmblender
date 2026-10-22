#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Write a textured glTF quad whose authored UVs disagree with any projection.

    create_gltf_uv_fixture.py OUT_DIR   -> OUT_DIR/quad.gltf, quad.bin, halves.png

The 64x64 texture is red on the left half and blue on the right. Every corner's
TEXCOORD_0 is (0.125, 0.5), deep inside the red half, so a render that honours
the authored UVs shows a uniformly red quad; a cube/planar projection would show
both halves. Z-up conversion: the quad stands upright facing -Y in lmblender.
"""
import json
import struct
import sys
import zlib
from pathlib import Path


def png(width, height, rgb_at):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + b"".join(bytes(rgb_at(x, y)) for x in range(width)) for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    (out / "halves.png").write_bytes(png(64, 64, lambda x, y: (230, 20, 20) if x < 32 else (20, 20, 230)))
    # glTF is Y-up: a 2x2 quad in the XY plane at z=0 becomes upright (XZ) in lmblender's Z-up frame.
    positions = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    uvs = [(0.125, 0.5)] * 4
    indices = [0, 1, 2, 0, 2, 3]
    pos_bytes = b"".join(struct.pack("<fff", *p) for p in positions)
    uv_bytes = b"".join(struct.pack("<ff", *t) for t in uvs)
    idx_bytes = b"".join(struct.pack("<H", i) for i in indices)
    buffer = pos_bytes + uv_bytes + idx_bytes
    (out / "quad.bin").write_bytes(buffer)
    gltf = {
        "asset": {"version": "2.0", "generator": "lmblender tests/create_gltf_uv_fixture.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": 0}]}],
        "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 0.9,
                                                "baseColorTexture": {"index": 0}}}],
        "textures": [{"source": 0}],
        "images": [{"uri": "halves.png"}],
        "buffers": [{"uri": "quad.bin", "byteLength": len(buffer)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bytes)},
            {"buffer": 0, "byteOffset": len(pos_bytes), "byteLength": len(uv_bytes)},
            {"buffer": 0, "byteOffset": len(pos_bytes) + len(uv_bytes), "byteLength": len(idx_bytes)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-1, -1, 0], "max": [1, 1, 0]},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
    }
    (out / "quad.gltf").write_text(json.dumps(gltf, indent=1) + "\n")
    print(f"{out}: quad.gltf, quad.bin ({len(buffer)} bytes), halves.png")


if __name__ == "__main__":
    main()
