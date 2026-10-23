#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Ensure generated text fixtures use one byte representation on every OS."""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
paths = sorted(root.glob("*.obj")) + sorted(root.glob("*.mtl"))
assert paths, f"no generated text fixtures under {root}"
for path in paths:
    data = path.read_bytes()
    assert b"\r" not in data, f"{path} contains CR bytes; fixture generation must use LF"
print(f"fixture text EOL stable: {len(paths)} OBJ/MTL files use LF")
