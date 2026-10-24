#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Run every tutorial page's script from an empty directory and check it against the page.

For each docs/tutorial page with a `<!-- code: code/NAME.lms -->` block, the script is copied
into a fresh temporary directory and run with `lmscript NAME.lms`. The console output must equal
the page's first "Expected output" block exactly. A block starting with {"ok": false must come
with a non-zero exit status. Every image the page shows that the run wrote is compared with the
committed image: mean absolute RGB difference (0-255) must stay below --max-mad.

Usage: python3 scripts/tutorial-run.py [--max-mad 3.0] [--report report.md]
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
TUTORIAL = ROOT / "docs" / "tutorial"
sys.path.insert(0, str(ROOT / "tests"))
from png_metrics import decode  # noqa: E402


def mad(a_path, b_path):
    a, b = decode(a_path), decode(b_path)
    if a[:2] != b[:2]:
        return None
    (w, h, ca, ra), (_, _, cb, rb) = a, b
    total = 0
    for y in range(h):
        row_a, row_b = ra[y], rb[y]
        for x in range(w):
            for k in range(3):
                total += abs(row_a[x * ca + k] - row_b[x * cb + k])
    return total / (w * h * 3)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-mad", type=float, default=3.0)
    parser.add_argument("--report")
    args = parser.parse_args()
    lmscript = os.environ.get("LMSCRIPT") or shutil.which("lmscript")
    if not lmscript:
        print("lmscript not found on PATH")
        return 2
    rows, failures = [], 0
    for page in sorted(TUTORIAL.glob("[0-9][0-9]-*.md")):
        text = page.read_text(encoding="utf-8")
        code = re.search(r"<!-- code: (code/[^ ]+\.lms) -->", text)
        expected = re.search(r"Expected output[^\n]*\n\n```text\n(.*?)```", text, re.S)
        if not code or not expected:
            continue
        source = TUTORIAL / code[1]
        want = expected[1]
        work = pathlib.Path(tempfile.mkdtemp(prefix="lmblender-tutorial-"))
        shutil.copy(source, work / source.name)
        start = time.monotonic()
        run = subprocess.run([lmscript, source.name], cwd=work, capture_output=True, text=True)
        seconds = time.monotonic() - start
        got = run.stdout.replace("\r\n", "\n")
        expect_failure = want.startswith('{"ok": false')
        status_ok = (run.returncode != 0) if expect_failure else (run.returncode == 0)
        output_ok = got == want
        image_notes = []
        image_ok = True
        for name in re.findall(r"\]\(images/([^)]+\.png)\)", text):
            produced = work / name
            if not produced.exists():
                if not expect_failure:
                    image_ok = False
                    image_notes.append(f"{name} missing")
                continue
            value = mad(produced, TUTORIAL / "images" / name)
            if value is None or value >= args.max_mad:
                image_ok = False
            image_notes.append(f"{name} {'size differs' if value is None else f'{value:.2f}'}")
        ok = status_ok and output_ok and image_ok
        failures += not ok
        rows.append((page.name, run.returncode, output_ok, image_ok, seconds, "; ".join(image_notes)))
        if not output_ok:
            print(f"--- {page.name}: expected output\n{want}--- actual stdout\n{got}--- stderr\n{run.stderr}")
        shutil.rmtree(work, ignore_errors=True)
    table = ["| Page | Exit | Output matches | Images match | Seconds | Image difference /255 |",
             "|---|---|---|---|---|---|"]
    table += [f"| {p} | {c} | {'yes' if o else 'no'} | {'yes' if i else 'no'} | {s:.1f} | {n or '-'} |"
              for p, c, o, i, s, n in rows]
    print("\n".join(table))
    if args.report:
        pathlib.Path(args.report).write_text("\n".join(table) + "\n", encoding="utf-8")
    print(f"{len(rows) - failures} of {len(rows)} pages pass")
    return 1 if failures or not rows else 0


if __name__ == "__main__":
    sys.exit(main())
