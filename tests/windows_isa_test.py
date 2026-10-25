#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("isa", Path(__file__).resolve().parents[1] / "scripts/check-windows-isa.py")
isa = importlib.util.module_from_spec(spec)
spec.loader.exec_module(isa)
entry = lambda source, flags: {"file": source, "command": "cl /c " + flags}
assert isa.check([entry("cpp/main.cpp", "/O2")]) == (1, 0)
assert isa.check([entry("overlay/src/kernel/device/cpu/kernel_avx2.cpp", "/arch:AVX2")]) == (1, 1)
for entries in ([], [entry("cpp/main.cpp", "/arch:AVX2")],
                [entry("src/kernel/device/cpu/kernel.cpp", "/arch:AVX512")],
                [entry("cpp/main.cpp", "-march=native")],
                [entry("cpp/main.cpp", "-DWITH_KERNEL_NATIVE")]):
    try:
        isa.check(entries)
    except ValueError:
        continue
    raise AssertionError(f"unsafe compilation accepted: {entries}")
print("Windows ISA guard fixtures passed")
