#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""An over-lit frame warns on stderr with a usable exposure, and that exposure
removes the blowout (tests/exposure.lms)."""
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from png_metrics import decode


def clipped(path):
    w, h, c, rows = decode(path)
    hit = sum(1 for row in rows for x in range(w) if max(row[x * c:x * c + 3]) >= 254)
    return hit / (w * h)


hot, fixed, stderr = sys.argv[1], sys.argv[2], Path(sys.argv[3]).read_text(encoding="utf-8")
warn = re.search(r"exposure-hot\.png: (\d+)% of pixels are clipped.*scene\.exposure\((-?[\d.]+)\)", stderr)
assert warn, f"no blowout warning for the over-lit frame:\n{stderr}"
assert -5.5 <= float(warn.group(2)) <= -3.5, f"suggested exposure drifted from the -4.5 used by tests/exposure.lms: {warn.group(2)}"
dark = re.search(r"exposure-dark\.png: image is nearly black.*scene\.exposure\((-?[\d.]+)\)", stderr)
assert dark and -5.5 <= float(dark.group(1)) <= -3.5, f"no usable black-frame warning at exposure(-10):\n{stderr}"
assert "exposure-fixed.png" not in stderr, f"corrected frame still warns:\n{stderr}"
before, after = clipped(hot), clipped(fixed)
print(f"exposure: clipped {before:.0%} -> {after:.0%} with scene.exposure({warn.group(2)})")
assert before > 0.5 and after < 0.02, (before, after)
