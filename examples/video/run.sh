#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
build=${LMBLENDER_BUILD_DIR:-$root/build}
mkdir -p "$build/video"
python3 - "$build/video" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
for level in range(1, 5):
    for path in root.glob(f"level{level}-*.png"):
        path.unlink()
    (root / f"level{level}.mp4").unlink(missing_ok=True)
PY
for level in level1-teams level2-agents level3-flow level4-pipeline; do
  PATH="$build:$PATH" \
  lmscript "examples/video/$level.lms" "$build/video/$level.dslb" >/dev/null
done
# The examples write images/video/ for users; collect the frames for QA here.
mv images/video/level*.png "$build/video/"
rmdir images/video 2>/dev/null || true
python3 tests/video_quality.py "$build/video"
if command -v ffmpeg >/dev/null 2>&1; then
  for pair in "level1-teams level1" "level2-agents level2" "level3-flow level3" "level4-pipeline level4"; do
    read -r _ output <<<"$pair"
    ffmpeg -hide_banner -loglevel error -y -framerate 24 -i "$build/video/$output-%d.png" -c:v libx264 -pix_fmt yuv420p "$build/video/$output.mp4"
  done
  echo "encoded H.264 clips at 24 fps"
else
  echo "ffmpeg unavailable; PNG sequences are ready for an external encoder"
fi
