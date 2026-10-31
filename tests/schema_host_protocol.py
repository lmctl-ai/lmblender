#!/usr/bin/env python3
import sys
from pathlib import Path

summary = Path(sys.argv[1]).read_text(encoding="utf-8")
signatures = Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
missing = [signature for signature in signatures
           if (signature[7:] if signature.startswith("method ") else signature) not in summary]
if missing:
    raise SystemExit("--lmcode-schema summary omits registry signatures: " + "; ".join(missing[:8]))
print(f"schema summary contains all {len(signatures)} canonical signatures")
