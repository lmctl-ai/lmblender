#!/usr/bin/env python3
import sys
from pathlib import Path

summary = Path(sys.argv[1]).read_text()
signatures = Path(sys.argv[2]).read_text().splitlines()
missing = [signature for signature in signatures
           if (signature[7:] if signature.startswith("method ") else signature) not in summary]
if missing:
    raise SystemExit("--dsl-schema summary omits registry signatures: " + "; ".join(missing[:8]))
print(f"schema summary contains all {len(signatures)} canonical signatures")
