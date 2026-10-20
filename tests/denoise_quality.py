#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

import math
import sys

from png_metrics import decode


def read(path):
    width, height, channels, rows = decode(path)
    pixels = []
    for row in rows:
        pixels.append([tuple(row[x:x + 3]) for x in range(0, width * channels, channels)])
    return width, height, pixels


def luminance(pixel):
    return (pixel[0] * 2126 + pixel[1] * 7152 + pixel[2] * 722) / 10000.0


def measures(path):
    width, height, image = read(path)
    residuals = []
    mask = [[False] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            r, g, b = image[y][x]
            mask[y][x] = b > r * 1.35 and b > g * 1.15
    for y in range(1, height - 1):
        for x in range(1, width - 1):
            values = [luminance(image[yy][xx]) for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2)]
            if max(values) - min(values) < 50:
                residuals.append(values[4] - sum(values) / 9)
    noise = math.sqrt(sum(v * v for v in residuals) / max(1, len(residuals)))
    boundary = []
    for y in range(1, height - 1):
        for x in range(1, width - 1):
            if mask[y][x] != mask[y][x + 1] or mask[y][x] != mask[y + 1][x]:
                boundary.append((x, y))
    return width, height, image, mask, noise, boundary


def edge_gradient(image, boundary):
    gradients = []
    for x, y in boundary:
        gx = luminance(image[y][x + 1]) - luminance(image[y][x - 1])
        gy = luminance(image[y + 1][x]) - luminance(image[y - 1][x])
        gradients.append(math.hypot(gx, gy))
    return sorted(gradients)[len(gradients) // 2] if gradients else 0.0


def main(noisy_path, denoised_path, reference_path):
    w0, h0, noisy_image, noisy, n0, _ = measures(noisy_path)
    w1, h1, denoised_image, denoised, n1, _ = measures(denoised_path)
    wr, hr, reference_image, reference, nr, reference_boundary = measures(reference_path)
    assert (w0, h0) == (w1, h1) == (wr, hr) == (192, 144), "render dimensions changed"
    intersection = union = 0
    for ya, yb in zip(reference, denoised):
        for a, b in zip(ya, yb):
            intersection += a and b
            union += a or b
    iou = intersection / max(1, union)
    er = edge_gradient(reference_image, reference_boundary)
    e1 = edge_gradient(denoised_image, reference_boundary)
    print(f"OIDN quality: high-frequency residual {n0:.3f}->{n1:.3f}; silhouette IoU vs 64-sample reference={iou:.4f}; edge-gradient median {er:.2f}->{e1:.2f}")
    assert n0 > 0.25, f"noisy reference has insufficient measured residual ({n0:.3f})"
    assert n1 < n0 * 0.8, f"OIDN did not reduce high-frequency residual by 20%: {n0:.3f}->{n1:.3f}"
    assert iou > 0.95, f"denoising shifted silhouette (IoU {iou:.4f})"
    assert er > 2.0, f"reference silhouette edge is not measurable (median gradient {er:.2f})"
    assert e1 >= er * 0.65, f"denoising over-blurred edges (gradient median {er:.2f}->{e1:.2f})"


if __name__ == "__main__":
    main(*sys.argv[1:])
