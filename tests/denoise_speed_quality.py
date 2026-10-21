#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import math
import sys

from png_metrics import decode


def ssim_windows(a, b, width, height, channels, window=8, stride=4):
    c1 = (0.01 * 255) ** 2
    c2 = (0.03 * 255) ** 2
    scores = []
    for y0 in range(0, height - window + 1, stride):
        for x0 in range(0, width - window + 1, stride):
            for channel in range(3):
                x = [a[y][i + channel] for y in range(y0, y0 + window)
                     for i in range(x0 * channels, (x0 + window) * channels, channels)]
                y = [b[row][i + channel] for row in range(y0, y0 + window)
                     for i in range(x0 * channels, (x0 + window) * channels, channels)]
                mx, my = sum(x) / len(x), sum(y) / len(y)
                vx = sum((v - mx) ** 2 for v in x) / (len(x) - 1)
                vy = sum((v - my) ** 2 for v in y) / (len(y) - 1)
                cov = sum((u - mx) * (v - my) for u, v in zip(x, y)) / (len(x) - 1)
                scores.append(((2 * mx * my + c1) * (2 * cov + c2)) /
                               ((mx * mx + my * my + c1) * (vx + vy + c2)))
    return sum(scores) / len(scores)


def main(fast_path, reference_path):
    wf, hf, cf, fast = decode(fast_path)
    wr, hr, cr, reference = decode(reference_path)
    assert (wf, hf) == (wr, hr) == (640, 360) and cf >= 3 and cr >= 3, "denoiser comparison dimensions/channels differ"
    fast_rgb = [row[i] for row in fast for x in range(wf) for i in range(x * cf, x * cf + 3)]
    ref_rgb = [row[i] for row in reference for x in range(wr) for i in range(x * cr, x * cr + 3)]
    mse = sum((x - y) ** 2 for x, y in zip(fast_rgb, ref_rgb)) / len(fast_rgb)
    psnr = 10 * math.log10(255 ** 2 / mse) if mse else math.inf
    ssim = ssim_windows(fast, reference, wf, hf, cf)
    print(f"fast 16 spp vs high 256 spp OIDN reference: PSNR={psnr:.2f} dB, SSIM={ssim:.5f}")
    assert psnr >= 30.0, f"fast OIDN PSNR {psnr:.2f} dB is below 30 dB"
    assert ssim >= 0.95, f"fast OIDN SSIM {ssim:.5f} is below 0.95"


if __name__ == "__main__":
    main(*sys.argv[1:])
