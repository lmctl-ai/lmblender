#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
static_cycles=${LMBLENDER_STATIC_CYCLES:-$repo/build/cycles-static/bin/cycles}
reference_cycles=${LMBLENDER_REFERENCE_CYCLES:-}
deps_install=${BLENDER_DEPS_INSTALL:-$repo/build/blender-deps-harvest}
prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix-gcc14}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

test -x "$static_cycles"
[ -n "$reference_cycles" ] && [ -x "$reference_cycles" ] || { echo "set LMBLENDER_REFERENCE_CYCLES to a dynamically linked Cycles executable for the parity smoke test" >&2; exit 2; }
ldd "$static_cycles" >"$tmp/ldd.txt" 2>&1 || true
grep -q 'not a dynamic executable' "$tmp/ldd.txt"
args=(--background --device CPU --samples 16 --threads 2 --width 128 --height 128)
env -u LD_LIBRARY_PATH -u OCIO "$static_cycles" "${args[@]}" \
  --output "$tmp/static.png" "$repo/tests/fixtures/cornell-box.xml" >"$tmp/static.log" 2>&1
mapfile -t dep_lib_dirs < <(find "$deps_install" -mindepth 2 -maxdepth 2 -type d -name lib -print | sort)
ld_path=$(IFS=:; echo "${dep_lib_dirs[*]}:$prefix/lib")
LD_LIBRARY_PATH="$ld_path" "$reference_cycles" "${args[@]}" \
  --output "$tmp/reference.png" "$repo/tests/fixtures/cornell-box.xml" >"$tmp/reference.log" 2>&1
python3 - "$tmp/static.png" "$tmp/reference.png" <<'PY'
import struct
import sys
import zlib
from pathlib import Path

def read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"not a PNG: {path}")
    pos, compressed, header = 8, [], None
    while pos < len(data):
        size = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + size]
        pos += size + 12
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            compressed.append(chunk)
        elif kind == b"IEND":
            break
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color != 6 or interlace:
        raise SystemExit("expected non-interlaced 8-bit RGBA PNG")
    stride = width * 4
    raw = zlib.decompress(b"".join(compressed))
    rows, prior, offset = [], bytearray(stride), 0
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        row = bytearray(raw[offset:offset + stride])
        offset += stride
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            up = prior[x]
            upper_left = prior[x - 4] if x >= 4 else 0
            if filter_type == 1:
                row[x] = (row[x] + left) & 255
            elif filter_type == 2:
                row[x] = (row[x] + up) & 255
            elif filter_type == 3:
                row[x] = (row[x] + (left + up) // 2) & 255
            elif filter_type == 4:
                estimate = left + up - upper_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upper_left))
                predictor = left if distances[0] <= min(distances[1:]) else (
                    up if distances[1] <= distances[2] else upper_left)
                row[x] = (row[x] + predictor) & 255
            elif filter_type != 0:
                raise SystemExit(f"unsupported PNG filter {filter_type}")
        rows.append(row)
        prior = row
    return width, height, rows

a, b = read_png(sys.argv[1]), read_png(sys.argv[2])
if a[:2] != b[:2]:
    raise SystemExit(f"image dimensions differ: {a[:2]} != {b[:2]}")
diffs = [abs(a[2][y][x] - b[2][y][x])
         for y in range(a[1]) for x in range(a[0] * 4) if x % 4 != 3]
mad = sum(diffs) / len(diffs)
maximum = max(diffs)
print(f"{a[0]}x{a[1]} RGB MAD={mad:.4f}/255 max={maximum}/255")
if mad > 1.0:
    raise SystemExit("static Cycles output differs from dynamic reference by more than 1/255 MAD")
PY
