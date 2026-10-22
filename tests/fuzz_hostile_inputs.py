#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Deterministic hostile-input corpus for the host's import and bytecode boundaries.

Run with LMBLENDER_FUZZ_BINARY=build-asan/lmblender to repeat under ASan/UBSan.
No render is needed: parsers and the DSL VM are the attack surface here.
"""
import json
import os
import random
import shutil
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
os.chdir(root)
binary = Path(os.environ.get("LMBLENDER_FUZZ_BINARY", "build/lmblender")).resolve()
work = Path("build/hostile-corpus")
work.mkdir(parents=True, exist_ok=True)
rng = random.Random(48291)
environment = os.environ.copy()
environment["PATH"] = str(binary.parent) + os.pathsep + environment.get("PATH", "")
environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
# Windows CreateProcess does not search PATHEXT for npm's .cmd launcher.
lmscript = shutil.which(os.environ.get("LMSCRIPT", "lmscript"))
if lmscript is None:
    raise FileNotFoundError("lmscript is not on PATH; set LMSCRIPT to its executable")


def compile_case(name, method, file):
    source = work / f"{name}.lms"
    bytecode = work / f"{name}.dslb"
    source.write_text(
        f'p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";\n'
        f'let item = scene.{method}("{file.as_posix()}"' + (",1.0" if method == "image_material" else "") + ');\n'
        'end _dsl_\n'
        f'savedsl(p,"{bytecode.as_posix()}");\n'
    )
    result = subprocess.run([lmscript, str(source)],
                            env=environment, text=True, capture_output=True, timeout=10)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return bytecode


def variants(data):
    cuts = {0, 1, 2, 4, 8, 16, 32, 64, len(data)//2, max(0, len(data)-1)}
    for cut in sorted(c for c in cuts if c < len(data)):
        yield f"cut-{cut}", data[:cut]
    for i in range(16):
        mutant = bytearray(data)
        if mutant:
            offset = rng.randrange(len(mutant))
            mutant[offset] ^= 1 << rng.randrange(8)
        yield f"flip-{i}", bytes(mutant)
    yield "nul-block", b"\x00" * 4096
    yield "digits", b"999999999999999999999999999999999999\n" * 64


def run_case(label, bytecode):
    result = subprocess.run([str(binary), "--dsl", str(bytecode)], env=environment,
                            text=True, capture_output=True, timeout=8)
    if result.returncode < 0 or result.returncode in (134, 139) or \
       "AddressSanitizer" in result.stderr or "runtime error:" in result.stderr:
        raise AssertionError(f"{label}: crash/sanitizer finding rc={result.returncode}: {result.stderr[-1600:]}")
    try:
        response = json.loads(result.stdout)
    except Exception as error:
        raise AssertionError(f"{label}: no structured JSON response rc={result.returncode}: {result.stdout[-500:]} {result.stderr[-500:]}") from error
    assert isinstance(response, dict) and "ok" in response, (label, response)
    if not response["ok"]:
        assert isinstance(response.get("code"), str) and response.get("message"), (label, response)


fixtures = Path("tests/fixtures")
gltf = fixtures / "gltf"
shutil.copy2(gltf / "scene.bin", work / "scene.bin")
shutil.copy2(gltf / "checker.png", work / "checker.png")
cases = [
    ("obj", "import_obj", fixtures / "tetra.obj", ".obj"),
    ("ply", "import_ply", fixtures / "tetra.ply", ".ply"),
    ("stl", "import_stl", fixtures / "one-triangle-binary.stl", ".stl"),
    ("gltf", "import_gltf", gltf / "scene.gltf", ".gltf"),
    ("glb", "import_gltf", gltf / "scene.glb", ".glb"),
    ("png", "image_material", fixtures / "checker-texture.png", ".png"),
    ("jpeg", "image_material", fixtures / "checker.jpg", ".jpg"),
]
total = 0
for name, method, fixture, suffix in cases:
    probe = work / ("probe-" + name + suffix)
    base = fixture.read_bytes()
    bytecode = compile_case(name, method, probe)
    for variant, payload in [("valid", base), *variants(base)]:
        probe.write_bytes(payload)
        run_case(name + "/" + variant, bytecode)
        total += 1

# MTL is parsed through an OBJ reference, including traversal and huge-count cases.
obj = work / "probe-mtl.obj"
obj.write_text("mtllib probe.mtl\nusemtl paint\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")
mtl = work / "probe.mtl"
mtl_program = compile_case("mtl", "import_obj", obj)
mtl_base = b"newmtl paint\nKd 0.8 0.2 0.1\nNs 48\nd 0.9\n"
for variant, payload in [("valid", mtl_base), *variants(mtl_base),
                         ("escape", b"newmtl paint\nmap_Kd ../../etc/passwd\n"),
                         ("huge", b"newmtl paint\nNs 999999999999999999999999\n")]:
    mtl.write_bytes(payload)
    run_case("mtl/" + variant, mtl_program)
    total += 1

# Corrupt compiled DSL v5 bytes, not just the source text. The host must reject
# bad opcodes and truncation with structured errors before dispatching a method.
base_bytecode = mtl_program.read_bytes()
for variant, payload in variants(base_bytecode):
    target = work / "mutant.dslb"
    target.write_bytes(payload)
    run_case("bytecode/" + variant, target)
    total += 1

# Source .lms text is compiled by the external lmscript frontend, not by the
# host. Exercise its error boundary too, then keep the ASan host target on the
# actual bytecode it receives. This check cannot claim sanitizer coverage of
# lmscript's own implementation.
source_base = (b'p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";\n'
               b'let box = scene.cube(1.0);\nend _dsl_\n'
               b'savedsl(p,"build/hostile-corpus/source.dslb");\n')
for variant, payload in variants(source_base):
    source = work / "mutant.lms"
    source.write_bytes(payload)
    result = subprocess.run([lmscript, "--check", str(source)],
                            env=environment, text=True, capture_output=True, timeout=8)
    assert result.returncode >= 0 and result.returncode not in (134,139), \
        ("lms/" + variant, result.returncode, result.stderr[-500:])
    total += 1

print(f"hostile corpus: {total} import/texture/bytecode/source cases, no crash or sanitizer report")
