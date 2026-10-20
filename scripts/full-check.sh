#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
build=${LMBLENDER_BUILD_DIR:-$root/build}
log=${LMBLENDER_FULL_CHECK_LOG:-$build/full-check.log}
mkdir -p "$build"
printf 'Running full test suite; log: %s\n' "$log"
if ! BUILD_JOBS=2 LMBLENDER_BUILD_DIR="$build" ./test.sh >"$log" 2>&1; then
  cat "$log" >&2
  exit 1
fi
LMBLENDER_BUILD_DIR="$build" ./examples/video/run.sh >>"$log" 2>&1 || {
  cat "$log" >&2
  exit 1
}
exe="$build/lmblender"
cycle="$build/cycles-static/bin/cycles"
for binary in "$exe" "$cycle"; do
  ldd "$binary" >"$build/ldd-$(basename "$binary").txt" 2>&1 || true
  grep -q 'not a dynamic executable' "$build/ldd-$(basename "$binary").txt" || {
    cat "$build/ldd-$(basename "$binary").txt" >&2
    exit 1
  }
done
python3 - "$build" <<'PY'
import struct,sys
from pathlib import Path
b=Path(sys.argv[1])
images=['import-gltf-gltf.png','import-gltf-glb.png','sky-texture.png','environment-texture.png',
        'transparent-film.png','camera-dof.png','primitives-gallery.png','volume-fog.png','material-maps.png',
        'subsurface.png','glass-transmission.png','procedural-materials.png','image-texture.png','backface-culling.png']
for name in images:
    data=(b/name).read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n', f'{name}: not PNG'
    w,h=struct.unpack('>II',data[16:24])
    assert w>=640 and h>=480, f'{name}: expected at least 640x480, got {w}x{h}'
print(f'Gallery image dimensions: {len(images)} images >=640x480')
PY

qa_dir="$build/full-check-qa"
mkdir -p "$qa_dir"
: > "$qa_dir/results.txt"
qa_image() {
  local name=$1 image=$2 checks=$3
  local path="$build/$image"
  local response
  response=$(lmctl prompt /home/ubuntu/lmblender.lmctl QA \
    "Inspect the local image $path directly. Checklist for $name: $checks Reply PASS only if every numbered item is visibly true; otherwise reply FAIL followed by failed item numbers and a short reason. Give a quality score from 1 to 5.")
  printf '%s\n' "$response" | tee "$qa_dir/$name.txt" | tail -n 2
  printf '%s\n' "$response" | grep -q '^PASS ' || {
    printf '%s: QA did not pass\n' "$name" | tee -a "$qa_dir/results.txt" >&2
    return 1
  }
  printf '%s: %s\n' "$name" "$(printf '%s' "$response" | head -n 1)" >> "$qa_dir/results.txt"
}
qa_image gltf-json import-gltf-gltf.png '1) exactly two separate upright checker-textured panels, left and right; 2) both fully inside the image with a gap; 3) red/yellow checker squares are visible on both.'
qa_image glb import-gltf-glb.png '1) exactly two separate upright checker-textured panels, left and right; 2) both fully inside the image with a gap; 3) red/yellow checker squares are visible on both.'
qa_image primitives primitives-gallery.png '1) five distinct forms are present (cube, sphere, cylinder, cone, torus); 2) they are separated, fully framed, and not overlapping; 3) their red, blue, green, yellow, and magenta colors are distinguishable; 4) the sphere, cylinder, and torus look smoothly shaded.'
qa_image environment environment-texture.png '1) a reflective metallic sphere is clearly visible; 2) the sphere reflects a colored horizon and distinct warm/cool sky regions; 3) the checker floor is visible below it.'
qa_image sky sky-texture.png '1) a ground plane and sky horizon are both visible; 2) sky brightness changes from horizon toward the upper sky; 3) the scene subject is visible and not mostly clipped white.'
qa_image transparent transparent-film.png '1) one colored sphere is visible; 2) the background around it is transparent/checkerboard-like in the alpha-aware preview or visibly distinct from the sphere; 3) the sphere is fully inside frame.'
qa_image dof camera-dof.png '1) three colored spheres (red, green, blue) appear in a row; 2) the middle green sphere is visibly sharper than the blue far sphere; 3) no sphere is clipped or overlaps another.'
qa_image fog volume-fog.png '1) a red subject is visible inside a blue-tinted haze; 2) haze reduces checker/floor contrast with distance; 3) the image contains a visible lit subject and floor rather than an isolated blob.'
qa_image subsurface subsurface.png '1) a warm orange wax-like sphere is clearly visible; 2) the sphere has a smooth lit-to-shadow gradient; 3) it is not a flat silhouette and is fully framed.'
qa_image glass glass-transmission.png '1) a glass sphere is clearly visible over a checker floor; 2) the checker pattern is visibly refracted through its center; 3) highlights are localized and there are no conspicuous firefly specks.'
qa_image procedural procedural-materials.png '1) two distinct objects are visible; 2) the left object has a red/yellow checker material and the right has blue noise variation; 3) both objects and their shadows are inside the frame.'
qa_image image-texture image-texture.png '1) the textured cube is fully visible; 2) red and yellow regions from the texture appear on multiple faces; 3) the image is not a plain gray cube.'
qa_image material-maps material-maps.png '1) exactly two smooth spheres sit apart on the ground and both are fully framed; 2) the left orange sphere has visibly irregular normal-map relief and varying metallic/roughness highlights; 3) the right sphere visibly alternates gold and blue from a texture-driven shader mix; 4) neither sphere overlaps or is cropped.'
qa_image backface-culling backface-culling.png '1) one bright green square panel is clearly visible on the left; 2) the corresponding right panel area is empty gray because the red material is viewed from its culled backface; 3) both left/right regions are unclipped and the distinction is clear.'
qa_image video-level1 'video/level1-8.png' '1) Coder and Research orb teams both have readable labels; 2) every visible ring and satellite orb is fully inside frame; 3) this peek is closer than video/level1-0.png; 4) the groups remain visibly separate.'
qa_image video-level2 'video/level2-12.png' '1) one yellow lead and the cyan, green, pink, and purple agents are visible; 2) the CODER, LEAD, and BROWSER labels are readable; 3) message arrows and a red-orange token are visible; 4) the scene is fully framed.'
qa_image video-level3 'video/level3-12.png' '1) HUMAN, LEAD, TEAM, RESULT cards are all visible in left-to-right order; 2) the text on all cards is readable; 3) arrows connect consecutive cards; 4) a red-orange message token is visible without hiding card labels.'
qa_image video-level4 'video/level4-23.png' '1) INPUT, BUILD, REVIEW, SHIP labels are visible in order; 2) the green check is large and fully inside frame; 3) the green check belongs at the final SHIP stage; 4) the moving red-orange token is visible near the end.'

size_host=$(stat -c %s "$exe")
size_cycles=$(stat -c %s "$cycle")
commit=$(git rev-parse --short HEAD)
if git diff --quiet && git diff --cached --quiet; then
  tested_revision="$commit (clean worktree)"
else
  tested_revision="$commit plus tested worktree changes"
fi
{
  printf '# Full-check status\n\n'
printf 'Generated: %s\n\n' "$(date -u '+%Y-%m-%d %H:%M UTC')"
  printf 'Source revision tested: `%s`\n\n' "$tested_revision"
  printf '## Results\n\n- `BUILD_JOBS=2 ./test.sh`: PASS.\n- Video sequences: four 24-frame 640×360 sequences passed all per-frame numeric gates; ffmpeg encoding was attempted when available.\n- Strict pixel and file checks: PASS; 14 gallery images are at least 640×480.\n- Static linkage: PASS (`ldd` reports “not a dynamic executable” for both binaries).\n- Binary sizes: `lmblender` %s bytes; standalone Cycles %s bytes.\n- QA provider `agy/gemini-4-argon-medium`: PASS on all 18 named images; exact responses follow.\n\n## QA responses\n\n' "$size_host" "$size_cycles"
  while IFS= read -r line; do printf -- '- %s\n' "$line"; done < "$qa_dir/results.txt"
} > docs/STATUS.md
cat docs/STATUS.md
