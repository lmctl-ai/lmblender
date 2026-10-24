#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Replace lmscript's 60-second shell shorthand with checked, timed argv calls.

This one-time migration also serves as a regression lint with --check. Tutorial
Markdown code blocks are synchronized separately by scripts/tutorial-sync.py.
"""

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SHORT = re.compile(
    r"^(?P<indent>[ \t]*)(?P<result>r|run) = system \$\{dsl_cli\(p\)\} "
    r"--dsl \$\{(?P<file>out|file)\};$",
    re.M,
)
OLD = re.compile(r"\bsystem\s+\$\{dsl_cli\([^)]*\)\}")
TIMED = re.compile(r"^(?P<indent>[ \t]*)(?P<result>r|run) = system\(argv, 3600000\);$", re.M)


def files():
    yield ROOT / "README.md"
    for base in ("examples", "tests", "docs", "durable-memory"):
        for extension in ("*.lms", "*.md"):
            yield from (ROOT / base).rglob(extension)


def rewrite(path: pathlib.Path) -> bool:
    source = path.read_text()
    matches = list(SHORT.finditer(source))
    if not matches:
        return False
    if len(matches) != 1:
        raise ValueError(f"expected one shorthand in {path}, found {len(matches)}")
    m = matches[0]
    result, file, indent = m.group("result", "file", "indent")
    replacement = "\n".join(
        indent + line
        for line in (
            "argv = dsl_cli(p);",
            'push(argv, "--dsl");',
            f"push(argv, {file});",
            f"{result} = system(argv, 3600000);",
            f"println(trim({result}.stdout));",
        )
    )
    updated = source[:m.start()] + replacement + source[m.end():]
    # Story examples used assert() after printing errors. A timed-out/null exit
    # must be a clear process failure, and every host error should show stderr.
    if path.parts[: len(ROOT.parts) + 1] == ROOT.parts + ("examples",):
        old = (f"if ({result}.code != 0) {{ println(trim({result}.stdout)); "
               f"println(trim({result}.stderr)); }}\nassert({result}.code == 0);")
        new = (f'if ({result}.code != 0) {{ println(' 
               f'trim(f"lmblender exit {{{result}.code}}: {{{result}.stderr}}")); exit(1); }}')
        if old in updated:
            updated = updated.replace(old, new, 1)
    # Negative tests intentionally expect a nonzero host result. Treat a null
    # result (lmscript timeout) as failure rather than accepting it as nonzero.
    if path.parts[: len(ROOT.parts) + 1] == ROOT.parts + ("tests",) and f"if ({result}.code == 0)" in updated:
        marker = f"println(trim({result}.stdout));\n"
        updated = updated.replace(
            marker,
            marker + f'if ({result}.code == null) {{ println("lmblender child timed out"); exit(1); }}\n',
            1,
        )
    path.write_text(updated)
    return True


def normalize_stdout(path: pathlib.Path) -> bool:
    """Print host stdout even on success, so JSON errors never vanish."""
    source = path.read_text()
    edits = []
    for match in TIMED.finditer(source):
        result = match.group("result")
        following = source[match.end():].splitlines()[1:2]
        if following and re.match(rf"\s*println\(.*\b{result}\.stdout\b", following[0]):
            continue
        edits.append((match.end(), f'\n{match.group("indent")}println(trim({result}.stdout));'))
    if not edits:
        return False
    for offset, replacement in reversed(edits):
        source = source[:offset] + replacement + source[offset:]
    path.write_text(source)
    return True


def dedupe_stdout(path: pathlib.Path) -> bool:
    """Earlier examples printed stdout after the error check; keep the early copy."""
    source = path.read_text()
    updated = source
    for result in ("r", "run"):
        line = f"println(trim({result}.stdout));\n"
        if updated.count(line) == 2 and len(list(TIMED.finditer(updated))) == 1:
            first = updated.find(line)
            second = updated.find(line, first + len(line))
            updated = updated[:second] + updated[second + len(line):]
    if updated == source:
        return False
    path.write_text(updated)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if any shell shorthand remains")
    args = parser.parse_args()
    paths = sorted(files())
    if not args.check:
        changed = [p for p in paths if rewrite(p)]
        print(f"migrated {len(changed)} example/test/tutorial source files")
        printed = [p for p in paths if p.suffix == ".lms" and normalize_stdout(p)]
        print(f"normalized stdout for {len(printed)} more lmscript files")
        deduped = [p for p in paths if p.suffix == ".lms" and dedupe_stdout(p)]
        print(f"removed duplicate stdout prints in {len(deduped)} files")
    remaining = [str(p.relative_to(ROOT)) for p in paths if OLD.search(p.read_text())]
    if remaining:
        print("60-second DSL host shorthand remains:\n  " + "\n  ".join(remaining), file=sys.stderr)
        return 1
    missing_stdout = []
    for path in paths:
        if path.suffix != ".lms":
            continue
        source = path.read_text()
        for match in TIMED.finditer(source):
            following = source[match.end():].splitlines()[1:2]
            if not following or not re.match(rf"\s*println\(.*\b{match.group('result')}\.stdout\b", following[0]):
                missing_stdout.append(str(path.relative_to(ROOT)))
    if missing_stdout:
        print("timed DSL calls without stdout printing:\n  " + "\n  ".join(missing_stdout), file=sys.stderr)
        return 1
    print(f"DSL host timeout lint passed ({len(paths)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
