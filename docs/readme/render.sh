#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
# Re-render the README images in docs/readme: hero-light.png, hero-dark.png,
# gallery-fog.png, gallery-strands.png. Uses the lmblender lmscript resolves
# (a binary on PATH first, otherwise the latest release). The scenes use the
# OIDN denoiser, so on Linux arm64 without SVE use a release newer than v0.3.4
# or a build from source.
set -euo pipefail
cd "$(dirname "$0")"
lmscript hero.lms
lmscript gallery.lms
for image in hero-light.png hero-dark.png gallery-fog.png gallery-strands.png; do
  echo "$image $(wc -c < "$image" | tr -d ' ') bytes"
done
