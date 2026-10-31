#!/usr/bin/env python3
"""Reject retired host option spellings in tracked files; preserve exact evidence.

A .lmcode-flag-history.json exception records hashes of individual historical
lines, not directory exclusions. New literals in a historical file still fail.
"""
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys

OLD = b'--' + b'dsl'

def scan(root):
    root = Path(root)
    manifest = root / '.lmcode-flag-history.json'
    history = json.loads(manifest.read_text()).get('lines', {}) if manifest.exists() else {}
    files = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
    failures, kept = [], 0
    for name in files:
        path = root / name
        if not name or not path.is_file() or path.is_symlink():
            continue
        allowed = collections.Counter(history.get(name, []))
        for number, line in enumerate(path.read_bytes().splitlines(), 1):
            if OLD not in line:
                continue
            digest = hashlib.sha256(line).hexdigest()
            if allowed[digest]:
                allowed[digest] -= 1
                kept += 1
            else:
                failures.append(f'{name}:{number}')
    return failures, kept

def main():
    failures, kept = scan(Path(sys.argv[1] if len(sys.argv) > 1 else '.').resolve())
    for failure in failures:
        print(failure)
    print(f'{len(failures)} non-historical old host-flag lines; {kept} exact historical lines retained')
    return int(bool(failures))

if __name__ == '__main__':
    sys.exit(main())
