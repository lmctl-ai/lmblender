#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Create an OBJ whose vertices and explicit normals are pre-rotated equally."""
import math
import pathlib
import shutil
import sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
source = pathlib.Path("tests/fixtures/obj-mtl")
shutil.copy2(source / "study-flat.mtl", out / "study-flat.mtl")
for name in ("red-checker.png", "height.png", "opacity.png"):
    if (source / name).exists(): shutil.copy2(source / name, out / name)
c, s = math.cos(math.radians(35.0)), math.sin(math.radians(35.0))
lines = []
for line in (source / "study-flat.obj").read_text(encoding="utf-8").splitlines():
    fields = line.split()
    if fields and fields[0] in ("v", "vn"):
        x, y, z = map(float, fields[1:4])
        fields[1:4] = (f"{c*x+s*z:.9f}", fields[2], f"{-s*x+c*z:.9f}")
        line = " ".join(fields)
    lines.append(line)
(out / "study-flat-rotated-y.obj").write_text("\n".join(lines) + "\n", newline="\n")
