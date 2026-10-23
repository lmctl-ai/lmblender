#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Keep tutorial code blocks identical to the files that were actually run.

A page marks a block with a comment line `<!-- code: PATH -->` (PATH relative to
docs/tutorial/) followed by a fenced block. Without arguments the fenced block is
rewritten from the file; with --check the script fails if any block differs.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1] / "docs" / "tutorial"
MARK = re.compile(r"<!-- code: (?P<path>[^ ]+) -->\n```(?P<lang>[a-z]*)\n(?P<body>.*?)```\n", re.S)


def main() -> int:
    check = "--check" in sys.argv[1:]
    drift = []
    for page in sorted(ROOT.rglob("*.md")):
        text = page.read_text()

        def fill(match: re.Match) -> str:
            source = (ROOT / match["path"]).read_text()
            if source != match["body"]:
                drift.append(f"{page.relative_to(ROOT)}: {match['path']}")
            return f"<!-- code: {match['path']} -->\n```{match['lang']}\n{source}```\n"

        updated = MARK.sub(fill, text)
        if not check and updated != text:
            page.write_text(updated)
    if check and drift:
        print("tutorial code blocks differ from their files:\n  " + "\n  ".join(drift))
        return 1
    print(f"tutorial code blocks {'match' if check else 'synced'} ({'none' if not drift else len(drift)} changed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
