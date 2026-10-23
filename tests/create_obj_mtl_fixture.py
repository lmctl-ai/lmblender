#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Generate a bounded two-material OBJ with explicit corner UVs and normals."""
import math
import pathlib
import struct
import sys
import zlib

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)


def png(name, color):
    width = height = 64
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(max(0, min(255, int(round(c * 255)))) for c in color(x, y))

    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    (out / name).write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    )


png("red-checker.png", lambda x, y: ((0.95, 0.52, 0.42) if (x // 12 + y // 12) % 2 else (0.22, 0.12, 0.10)))
png("height.png", lambda x, y: ((0.5 + 0.25 * math.sin(x * 0.5) * math.cos(y * 0.4),) * 3))
png("opacity.png", lambda x, y: ((0.80,) * 3))

vertex_lines = []
face_lines = ["mtllib study.mtl"]
face_vertices = (
    (0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
    (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7),
)
normals = ((0, 0, -1), (0, 0, 1), (0, -1, 0), (1, 0, 0), (0, 1, 0), (-1, 0, 0))
for cube, center in enumerate((-1.3, 1.3)):
    face_lines.append("usemtl RedCheck" if cube == 0 else "usemtl BlueGloss")
    verts = [(center + x * 0.72, y * 0.72, 0.85 + z * 0.72)
             for z in (-1, 1) for y in (-1, 1) for x in (-1, 1)]
    # Reorder from bit order to the standard 0..7 cube convention.
    verts = [verts[i] for i in (0, 1, 3, 2, 4, 5, 7, 6)]
    vertex_lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in verts]
    for face, normal in zip(face_vertices, normals):
        vertex_lines.append("vn " + " ".join(map(str, normal)))
        uv_start = len([line for line in vertex_lines if line.startswith("vt ")]) + 1
        vertex_lines += ["vt 0 0", "vt 1 0", "vt 1 1", "vt 0 1"]
        normal_index = cube * 6 + normals.index(normal) + 1
        corners = [f"{cube * 8 + v + 1}/{uv_start + i}/{normal_index}" for i, v in enumerate(face)]
        face_lines.append("f " + " ".join(corners))

# All v/vt/vn definitions precede faces, as OBJ's positive indices require.
geometry = "\n".join(vertex_lines) + "\n" + "\n".join(face_lines[1:]) + "\n"
def text(name, contents):
    # Fixtures are compared byte-for-byte by test.sh. Keep generated text stable
    # on Windows, where pathlib.write_text otherwise translates LF to CRLF.
    (out / name).write_text(contents, newline="\n")


text("study.obj", "mtllib study.mtl\n" + geometry)
text("study-flat.obj", "mtllib study-flat.mtl\n" + geometry)
text("study-no-bump.obj", "mtllib study-no-bump.mtl\n" + geometry)
text("study-bad.obj", "mtllib study-bad.mtl\n" + geometry)
text("study-huge-coordinate.obj",
     "mtllib study.mtl\n" + geometry.replace("v -2.020000", "v 1e307", 1))

red = """newmtl RedCheck
Kd 0.96 0.35 0.28
Ks 0.12 0.12 0.12
Ns 28
d 1
"""
blue = """newmtl BlueGloss
Kd 0.08 0.24 0.92
Ks 0.75 0.75 0.90
Ns 220
Tr 0.04
Ke 0.00 0.00 0.06
"""
text("study.mtl", red + "map_Kd red-checker.png\nmap_Bump -bm 0.35 height.png\n" + blue + "map_d opacity.png\n")
text("study-flat.mtl", red + blue)
text("study-no-bump.mtl", red + "map_Kd red-checker.png\n" + blue + "map_d opacity.png\n")
text("study-bad.mtl", red + "map_Kd ../outside.png\n" + blue)
print(f"OBJ/MTL fixture: {out / 'study.obj'}; 16 vertices, 24 triangles, 2 materials")
