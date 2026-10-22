# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Compare the Embree render with the fixed-seed BVH2 reference."""

import math
import sys

from png_metrics import decode

ref_width, ref_height, ref_channels, reference = decode(sys.argv[1])
width, height, channels, rendered = decode(sys.argv[2])
assert (width, height, channels) == (ref_width, ref_height, ref_channels), (
    "Embree render dimensions/channels differ from the BVH2 reference"
)
error = 0
count = 0
for expected, actual in zip(reference, rendered):
    for offset in range(0, len(expected), channels):
        for component in range(3):
            error += (expected[offset + component] - actual[offset + component]) ** 2
            count += 1
psnr = math.inf if error == 0 else 10 * math.log10(255 * 255 * count / error)
print(f"Embree vs BVH2 fixed-seed PSNR: {psnr:.2f} dB")
assert psnr >= 35.0, f"Embree changed the dense-mesh image: PSNR {psnr:.2f} dB < 35 dB"
