#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
# Regenerate every README gallery image in docs/images/ by running each story
# example exactly as `lmctl autorun` would: in an empty scratch directory, writing
# images/<name>.png (and images/kepler-orbit.gif). lmblender (and lmcad for the
# gear story) come from PATH or are installed by lmscript; ffmpeg makes the GIF.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
LMSCRIPT=${LMSCRIPT:-lmscript}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$root/docs/images"
# Pass example names to re-render only those.
stories="${*:-caustic-ball-lens sunset-long-shadows god-rays-window cornell-box backlit-skin
  caffeine-molecule dna-double-helix salt-crystal saturn-rings kepler-orbit
  bouncing-ball-strobe gear-pair-render
  ball-stick buckyball truss-bridge pbr-spheres material-sandbox model-viewer text
  shadows multidraw cubemap bullet fluidsimulation3d fluidsimulation2d
  level-1-sessions level-2-orchestration level-3-hands-off-lead level-4-automation}"
for name in $stories; do
  start=$(date +%s)
  (cd "$work" && "$LMSCRIPT" "$root/examples/$name.lms" >"$work/$name.log" 2>&1) || {
    cat "$work/$name.log" >&2; echo "render-all: $name failed" >&2; exit 1; }
  [ "$name" = kepler-orbit ] || cp "$work/images/$name.png" "$root/docs/images/$name.png"
  case "$name" in level-*) cp "$work/images/$name.gif" "$root/docs/images/$name.gif"; mkdir -p "$root/docs/videos"; cp "$work/images/$name.mp4" "$root/docs/videos/$name.mp4";; esac
  echo "$name: $(( $(date +%s) - start )) s"
done
if [ -f "$work/images/kepler-orbit.gif" ]; then
  cp "$work/images/kepler-orbit.gif" "$root/docs/images/kepler-orbit.gif"
else
  echo "note: ffmpeg not found, docs/images/kepler-orbit.gif not refreshed"
fi
# The host writes opaque renders as RGBA; dropping the constant alpha channel is
# lossless (same pixels) and keeps the README images under 300 KB.
if python3 -c "import PIL" 2>/dev/null; then
  python3 - "$root/docs/images" <<'PY'
import pathlib, sys
from PIL import Image
for path in sorted(pathlib.Path(sys.argv[1]).glob("*.png")):
    image = Image.open(path)
    if image.mode == "RGBA" and image.getextrema()[3] == (255, 255):
        image.convert("RGB").save(path, optimize=True)
PY
else
  echo "note: Pillow not found; docs/images PNGs keep their alpha channel and may exceed 300 KB"
fi
ls -l "$root/docs/images"
