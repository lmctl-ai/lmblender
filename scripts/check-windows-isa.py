#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Audit host/Cycles Ninja compile commands; AVX2 is allowed only in its dispatched TU.

This is a compile-flag guard, not a whole-binary disassembly claim: OIDN contains
runtime-dispatched AVX2/AVX-512 kernels and oneDNN JIT code. See docs/PLATFORMS.md.
"""
import json
from pathlib import Path
import re
import subprocess
import sys


def check(entries):
    count = 0
    dispatched = 0
    for entry in entries:
        source = entry["file"].replace("\\", "/")
        if Path(source).suffix.lower() not in (".c", ".cc", ".cpp", ".cxx"):
            continue
        count += 1
        command = entry["command"]
        flags = re.findall(r"(?i)(?:/arch:[\w.-]+|-m(?:arch|cpu)=[\w.-]+|-mavx[\w.-]*|/Qx[\w.-]+)", command)
        is_dispatch = source.endswith("/kernel/device/cpu/kernel_avx2.cpp")
        for flag in flags:
            if flag.lower() in ("/arch:sse2", "-march=x86-64", "-march=x86-64-v2"):
                continue
            if is_dispatch and flag.lower() == "/arch:avx2":
                dispatched += 1
                continue
            raise ValueError(f"non-baseline ISA flag {flag} in {source}")
        if "WITH_KERNEL_NATIVE" in command:
            raise ValueError(f"native-only Cycles kernel in {source}")
    if not count:
        raise ValueError("no C/C++ compile commands found")
    return count, dispatched


def main():
    for directory in sys.argv[1:]:
        result = subprocess.run(["ninja", "-C", directory, "-t", "compdb"],
                                capture_output=True, check=True, encoding="utf-8")
        count, dispatched = check(json.loads(result.stdout))
        print(f"Windows ISA guard: {directory}: {count} C/C++ commands; {dispatched} dispatched AVX2 flags; no general AVX/native flags")
    if len(sys.argv) < 2:
        raise SystemExit("usage: check-windows-isa.py BUILD_DIR [...]")


if __name__ == "__main__":
    main()
