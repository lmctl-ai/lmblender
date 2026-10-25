#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Keep the host API block in docs/LLM_GUIDE.md identical to `lmblender --dsl-signatures`.

    scripts/update-llm-guide.py --binary build/lmblender          # rewrite the block
    scripts/update-llm-guide.py --binary build/lmblender --check  # fail if it drifted

The block is the fenced ```text section that follows the "## Host API signatures"
heading; everything else in the guide is left untouched.
"""
import argparse
import json
import runpy
import re
import subprocess
import sys
from pathlib import Path

GUIDE = Path(__file__).resolve().parent.parent / "docs/LLM_GUIDE.md"


def locate_block(text):
    """Return (start, end) of the signature lines inside the fenced block, or None."""
    heading = text.find("## Host API signatures\n")
    if heading < 0:
        return None
    opening = text.find("```text\n", heading)
    if opening < 0:
        return None
    start = opening + len("```text\n")
    end = text.find("\n```", start)
    return None if end < 0 else (start, end)


def undocumented_methods(text, signatures):
    """Host methods whose name never appears in the '## Parameter guide' table.

    Blind LLM evaluations (docs/LLM_EVAL.md) showed that a bare typed signature
    such as `checker_ground(float, float)` is not enough to call a method.
    """
    begin = text.find("## Parameter guide")
    end = text.find("\n## ", begin + 1)
    table = text[begin:end] if begin >= 0 else ""
    names = re.findall(r"^method \w+\.(\w+)\(", signatures, re.M)
    return sorted({n for n in names if not re.search(r"\b" + re.escape(n) + r"\b", table)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, help="lmblender executable to query")
    parser.add_argument("--check", action="store_true", help="exit 1 instead of rewriting when the block is stale")
    args = parser.parse_args()
    effective = subprocess.run([args.binary, "--dsl-signatures"], check=True, capture_output=True, text=True).stdout.splitlines()
    root = GUIDE.parent.parent
    bridge = runpy.run_path(str(root / "scripts/merge-std.py"))
    standard = bridge["canonical_signatures"](json.loads((root / "cpp/std/lmcode-std.schema.json").read_text()))
    signatures = "\n".join(line for line in effective if line not in standard)
    text = GUIDE.read_text(encoding="utf-8")
    span = locate_block(text)
    if not span:
        print("docs/LLM_GUIDE.md has no '## Host API signatures' ```text block", file=sys.stderr)
        return 2
    start, end = span
    current = text[start:end].strip()
    undocumented = undocumented_methods(text, signatures)
    if undocumented:
        print("docs/LLM_GUIDE.md '## Parameter guide' has no row naming these host methods:", file=sys.stderr)
        for name in undocumented:
            print(f"  undocumented: {name}", file=sys.stderr)
        return 1
    if current == signatures:
        print("docs/LLM_GUIDE.md host API block matches --dsl-signatures")
        return 0
    if args.check:
        missing = sorted(set(signatures.splitlines()) - set(current.splitlines()))
        stale = sorted(set(current.splitlines()) - set(signatures.splitlines()))
        print("docs/LLM_GUIDE.md host API block is stale; run scripts/update-llm-guide.py --binary build/lmblender", file=sys.stderr)
        for line in missing:
            print(f"  missing: {line}", file=sys.stderr)
        for line in stale:
            print(f"  stale:   {line}", file=sys.stderr)
        return 1
    GUIDE.write_text(text[:start] + signatures + text[end:], encoding="utf-8")
    print(f"docs/LLM_GUIDE.md host API block rewritten ({len(signatures.splitlines())} signatures)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
