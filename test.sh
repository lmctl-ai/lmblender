#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd)
cd "$root"
build=${LMBLENDER_BUILD_DIR:-$root/build}
# Pinned Blender checkout (scripts/fetch-cycles-source.sh) and harvested deps
# (scripts/bootstrap-static-deps.sh); override both for another layout.
blender_source=${BLENDER_SOURCE_TREE:-$build/blender-src-upstream-v5.2.2}
deps_install=${BLENDER_DEPS_INSTALL:-$build/blender-deps-harvest}
[ -d "$blender_source" ] || { echo "pinned Blender source missing at $blender_source: run scripts/fetch-cycles-source.sh or set BLENDER_SOURCE_TREE" >&2; exit 2; }
mkdir -p "$build"
find "$build" -maxdepth 1 -type f \
  \( -name '*.png' -o -name '*.exr' -o -name '*.apng' -o -name '*.dslb' \
     -o -name '*-result.json' -o -name 'version.txt' -o -name 'presets-list.txt' \
     -o -name 'install-example.log' \) -delete
rm -rf "$build/video" "$build/test-tool-cache" "$build/material-map-assets"
rm -f "$root/install-and-run.png"
python3 tests/create_material_maps.py "$build/material-map-assets"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLMBLENDER_FULL_STATIC=ON \
  -DBLENDER_SOURCE_TREE="$blender_source" \
  -DBLENDER_DEPS_INSTALL="$deps_install" \
  -DBLENDER_LIBDIR="${BLENDER_LIBDIR:-$deps_install}" \
  -DCYCLES_BUILD_DIR="${CYCLES_BUILD_DIR:-$build/cycles-static}" \
  -DLMBLENDER_STATIC_PREFIX="${LMBLENDER_STATIC_PREFIX:-$build/static-prefix-gcc14}"
cmake --build "$build" --parallel "${BUILD_JOBS:-2}"
./scripts/sync-sdk.sh --verify
# The corresponding-source manifest must resolve every dependency pin of the
# Blender tree this build uses (keeps SOURCES.json in step with the scripts).
python3 scripts/source-manifest.py --blender "$blender_source" --check
grep -q '^WITH_CYCLES_EMBREE:BOOL=ON$' "${CYCLES_BUILD_DIR:-$build/cycles-static}/CMakeCache.txt" || {
  echo "static Cycles was not configured with Embree CPU BVH" >&2; exit 1;
}
exe="$build/lmblender"
export PATH="$build:$PATH"
compile_dsl() {
  local source="$1" bytecode="$2" stem temporary log
  shift 2
  stem=$(basename "$source")
  temporary="$build/compile-only-$stem"
  log="$temporary.log"
  # lmscript normally evaluates the whole outer script after saving its
  # embedded DSL block. Stop at savedsl() here so this pass only builds the
  # bytecode; the host execution and image assertions run once below. This
  # avoids the 60 s system() timeout hiding a slow render as exit code null.
  awk '/^[[:space:]]*savedsl\(/ { print; found=1; exit } { print }
       END { if (!found) exit 1 }' "$source" >"$temporary"
  if ! "${LMSCRIPT:-lmscript}" "$temporary" "$@" "$bytecode" >"$log" 2>&1; then
    cat "$log" >&2
    echo "failed compiling DSL block from $source" >&2
    return 1
  fi
  rm -f "$temporary" "$log"
}
cmp schema/lmblender.dsl.json <("$exe" --schema | head -c -1)
cmp schema/lmblender.dsl.json <("$exe" --dsl-schema --json)
cmp schema/lmblender.dsl.json <("$exe" --schema --json)
"$exe" --dsl-schema >"$build/schema-summary.txt"
"$exe" --dsl-signatures >"$build/host-signatures.txt"
"${LMSCRIPT:-lmscript}" --dsl-signatures schema/lmblender.dsl.json >"$build/compiler-signatures.txt"
diff -u "$build/compiler-signatures.txt" "$build/host-signatures.txt"
# docs/LLM_GUIDE.md carries the same list for DSL authors; it must not drift from the binary.
python3 scripts/update-llm-guide.py --binary "$exe" --check
python3 tests/schema_host_protocol.py "$build/schema-summary.txt" "$build/host-signatures.txt"
python3 - <<'PY'
from pathlib import Path
import re
files = [Path("README.md"), *Path("docs").rglob("*.md"), *Path("examples").rglob("*.lms"), *Path("tests").rglob("*.lms")]
declarations = []
for path in files:
    text = path.read_text(errors="replace")
    declarations.extend((path, value) for value in re.findall(r'install="github:lmctl-ai/lmblender@([^" ]+)"', text))
assert declarations, "expected lmblender install declarations in docs and examples"
stale = [(str(path), value) for path, value in declarations if value != "latest"]
assert not stale, f"install examples must use @latest: {stale}"
print(f"lmscript install references use @latest ({len(declarations)} declarations)")
PY
"$exe" --version >"$build/version.txt"
python3 - "$build/version.txt" VERSION <<'PY'
import pathlib, re, sys
lines = pathlib.Path(sys.argv[1]).read_text().splitlines()
version = pathlib.Path(sys.argv[2]).read_text().strip()
assert lines and lines[0].startswith("lmblender "), lines
tokens = re.findall(r"(?<![A-Za-z0-9])v?\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?", lines[0])
assert tokens == [version], f"first version line must contain exactly release version {version}: {lines[0]!r}; found {tokens}"
PY
grep -q '^Cycles [0-9]\+\.[0-9]\+\.[0-9]\+' "$build/version.txt"
grep -Eq '^OpenImageDenoise ([0-9]+\.[0-9]+\.[0-9]+|disabled)$' "$build/version.txt"
compile_dsl examples/version.lms "$build/version.dslb"
compile_dsl examples/import-mesh.lms "$build/import-mesh.dslb"
compile_dsl examples/import-ply.lms "$build/import-ply.dslb"
compile_dsl examples/gltf-import.lms "$build/gltf-import.dslb"
compile_dsl examples/glb-import.lms "$build/glb-import.dslb"
compile_dsl tests/gltf-invalid.lms "$build/gltf-invalid.dslb"
compile_dsl examples/sky-texture.lms "$build/sky-texture.dslb"
compile_dsl examples/environment-texture.lms "$build/environment-texture.dslb"
compile_dsl examples/transparent-film.lms "$build/transparent-film.dslb"
compile_dsl examples/camera-dof.lms "$build/camera-dof.dslb"
compile_dsl examples/primitives-gallery.lms "$build/primitives-gallery.dslb"
compile_dsl examples/strands-showcase.lms "$build/strands-showcase.dslb"
compile_dsl examples/spot-falloff.lms "$build/spot-falloff.dslb"
compile_dsl examples/light-falloff.lms "$build/light-falloff.dslb"
compile_dsl examples/bridge/render-export.lms "$build/bridge-render.dslb"
for effect in volume-fog volume-control subsurface subsurface-control glass-transmission glass-opaque-control; do
  compile_dsl "examples/$effect.lms" "$build/$effect.dslb"
done
compile_dsl examples/render-mesh.lms "$build/render-mesh.dslb" "$build/imported-mesh.png"
compile_dsl examples/render-mesh-xml-oracle.lms "$build/render-mesh-xml.dslb"
compile_dsl examples/material-principled.lms "$build/material-principled.dslb"
compile_dsl examples/camera-look-at.lms "$build/camera-look-at.dslb"
compile_dsl examples/render-timeout.lms "$build/render-timeout.dslb"
compile_dsl tests/geometry-budget.lms "$build/geometry-budget.dslb"
compile_dsl tests/image-limit.lms "$build/image-limit.dslb"
compile_dsl tests/image-limit-maps.lms "$build/image-limit-maps.dslb"
compile_dsl tests/run-budget.lms "$build/run-budget.dslb"
compile_dsl tests/output-inside.lms "$build/output-inside.dslb"
compile_dsl tests/output-escape.lms "$build/output-escape.dslb"
compile_dsl tests/output-symlink.lms "$build/output-symlink.dslb"
compile_dsl tests/no-default-light.lms "$build/no-default-light.dslb"
compile_dsl tests/preset-no-autoframe.lms "$build/preset-no-autoframe.dslb"
compile_dsl tests/preset-no-autoframe-position.lms "$build/preset-no-autoframe-position.dslb"
compile_dsl examples/render-exr.lms "$build/render-exr.dslb"
compile_dsl examples/mesh-instances.lms "$build/mesh-instances.dslb"
compile_dsl examples/embree-heavy.lms "$build/embree-heavy.dslb"
compile_dsl examples/render-settings.lms "$build/render-settings.dslb"
compile_dsl examples/procedural-materials.lms "$build/procedural-materials.dslb"
compile_dsl examples/orthographic-camera.lms "$build/orthographic-camera.dslb"
compile_dsl tests/camera_aspect.lms "$build/camera-aspect.dslb"
compile_dsl tests/camera_aspect_cube.lms "$build/camera-aspect-cube.dslb"
compile_dsl examples/image-texture.lms "$build/image-texture.dslb"
compile_dsl examples/display-transforms.lms "$build/display-transforms.dslb"
compile_dsl examples/render-passes.lms "$build/render-passes.dslb"
compile_dsl examples/denoise-quality.lms "$build/denoise-quality.dslb"
compile_dsl examples/animation-tween.lms "$build/animation-tween.dslb"
compile_dsl examples/animation-visibility-optics.lms "$build/animation-visibility-optics.dslb"
compile_dsl examples/motion-blur.lms "$build/motion-blur.dslb"
compile_dsl examples/material-nodes.lms "$build/material-nodes.dslb"
compile_dsl examples/material-maps.lms "$build/material-maps.dslb"
compile_dsl examples/backface-culling.lms "$build/backface-culling.dslb"
compile_dsl tests/quality-preset.lms "$build/quality-preset.dslb"
compile_dsl tests/preset-explicit-overrides.lms "$build/preset-explicit-overrides.dslb"
compile_dsl tests/denoise-speed.lms "$build/denoise-speed.dslb"
compile_dsl tests/sequence-budget.lms "$build/sequence-budget.dslb"
mkdir -p "$build/video"
for video in level1-teams level2-agents level3-flow level4-pipeline; do
  compile_dsl "examples/video/$video.lms" "$build/video/$video.dslb"
done
compile_dsl examples/material-nodes-control.lms "$build/material-nodes-control.dslb"
for preset in diagram-dark-16x9-draft diagram-dark-16x9-final product-studio turntable-neutral glass-showcase technical-clean; do
  compile_dsl "examples/presets/$preset.lms" "$build/preset-$preset.dslb"
done
PATH="$build:$PATH" LMSCRIPT_TOOL_CACHE="$build/test-tool-cache" "${LMSCRIPT:-lmscript}" examples/dsl/install-and-run.lms >"$build/install-example.log"
test -s install-and-run.png
python3 tests/png_metrics.py install-and-run.png
rm -f install-and-run.png
compile_dsl examples/presets/external-load.lms "$build/preset-external.dslb"
compile_dsl examples/presets/switch.lms "$build/preset-switch.dslb"
"$exe" --dsl "$build/version.dslb" >"$build/version-result.json"
"$exe" --dsl "$build/import-mesh.dslb" >"$build/import-mesh-result.json"
"$exe" --dsl "$build/import-ply.dslb" >"$build/import-ply-result.json"
"$exe" --dsl "$build/gltf-import.dslb" >"$build/gltf-import-result.json"
python3 tests/gltf_emission_quality.py "$build/import-gltf-gltf.png"
"$exe" --dsl "$build/glb-import.dslb" >"$build/glb-import-result.json"
if "$exe" --dsl "$build/gltf-invalid.dslb" >"$build/gltf-invalid-result.json"; then
  echo "malformed GLB unexpectedly imported" >&2; exit 1
fi
python3 - "$build/gltf-invalid-result.json" <<'PYERR'
import json,sys
r=json.load(open(sys.argv[1]))
assert r.get("ok") is False and r.get("code")=="GLTF_INVALID", r
assert "GLB JSON chunk is malformed" in r.get("message", ""), r
print("glTF malformed-GLB structured error passed")
PYERR
# A bytecode that must stop with a structured error: expect_dsl_error <stem> <code> [VAR=value ...]
expect_dsl_error() {
  local stem=$1 code=$2
  shift 2
  if env "$@" "$exe" --dsl "$build/$stem.dslb" >"$build/$stem-result.json"; then
    echo "$stem unexpectedly succeeded (expected $code)" >&2
    return 1
  fi
  python3 - "$build/$stem-result.json" "$code" <<'PY'
import json, sys
r = json.load(open(sys.argv[1]))
assert r.get("ok") is False and r.get("code") == sys.argv[2], r
print(f"{sys.argv[1].rsplit('/', 1)[-1]}: refused with {sys.argv[2]}: {r.get('message', '')[:110]}")
PY
}
# Scene-wide geometry budget stops accumulation at creation time, before any render.
expect_dsl_error geometry-budget GEOMETRY_BUDGET
test ! -e "$build/geometry-budget.png"
# Decoded-dimension guards: a 48 KB PNG that decodes to 400 megapixels is refused at
# declaration time by every image input, never handed to Cycles.
python3 tests/create_image_bomb.py "$build/image-bomb.png"
expect_dsl_error image-limit IMAGE_LIMIT
expect_dsl_error image-limit-maps IMAGE_LIMIT
# Whole-program budget: instruction ceiling, then the wall-clock deadline alone (ceiling
# disabled so a fast runner cannot trip it first); the latter hard-exits five seconds
# after the deadline while the loop is still running.
expect_dsl_error run-budget RUN_BUDGET LMBLENDER_MAX_INSTRUCTIONS=1000000
expect_dsl_error run-budget RUN_TIMEOUT LMBLENDER_RUN_TIMEOUT_SECONDS=1 LMBLENDER_MAX_INSTRUCTIONS=0
# Output path policy: a confinement root keeps renders inside it, and a symlinked parent
# directory is refused even without a root.
LMBLENDER_OUTPUT_ROOT="$build" "$exe" --dsl "$build/output-inside.dslb" >"$build/output-inside-result.json"
test -s "$build/output-inside.png"
rm -f /tmp/lmblender-output-escape.png
expect_dsl_error output-escape OUTPUT_PATH LMBLENDER_OUTPUT_ROOT="$build"
test ! -e /tmp/lmblender-output-escape.png
ln -sfn "$build" "$build/linked-out"
expect_dsl_error output-symlink OUTPUT_PATH
rm -f "$build/linked-out"
# The fallback key light exists only for scenes without any light: a black world with a
# zero-power area light must render black.
"$exe" --dsl "$build/no-default-light.dslb" >"$build/no-default-light-result.json"
python3 tests/pixel_bounds_quality.py "$build/no-default-light.png" 0 2
# Presets that switch auto-framing off must place the camera explicitly.
expect_dsl_error preset-no-autoframe PRESET_INVALID
"$exe" --dsl "$build/preset-no-autoframe-position.dslb" >"$build/preset-no-autoframe-position-result.json"
python3 tests/pixel_bounds_quality.py "$build/preset-no-autoframe-position.png" 20 255
"$exe" --dsl "$build/sky-texture.dslb" >"$build/sky-texture-result.json"
"$exe" --dsl "$build/environment-texture.dslb" >"$build/environment-texture-result.json"
"$exe" --dsl "$build/transparent-film.dslb" >"$build/transparent-film-result.json"
"$exe" --dsl "$build/camera-dof.dslb" >"$build/camera-dof-result.json"
"$exe" --dsl "$build/primitives-gallery.dslb" >"$build/primitives-gallery-result.json"
"$exe" --dsl "$build/strands-showcase.dslb" >"$build/strands-showcase-result.json"
python3 tests/strands_quality.py "$build/strands-showcase.png"
"$exe" --dsl "$build/spot-falloff.dslb" >"$build/spot-falloff-result.json"
python3 tests/spot_falloff_quality.py "$build/spot-falloff.png"
"$exe" --dsl "$build/light-falloff.dslb" >"$build/light-falloff-result.json"
python3 tests/light_falloff_quality.py "$build/light-falloff.png"
"$exe" --dsl "$build/bridge-render.dslb" >"$build/bridge-render-result.json"
python3 tests/bridge_parity.py tests/fixtures/bridge/blendercli-reference.png "$build/bridge-from-blendercli.png"
for effect in volume-fog volume-control subsurface subsurface-control glass-transmission glass-opaque-control; do
  "$exe" --dsl "$build/$effect.dslb" >"$build/$effect-result.json"
done
"$exe" --dsl "$build/render-mesh.dslb" >"$build/render-mesh-result.json"
LMBLENDER_TEST_XML_ORACLE=1 "$exe" --dsl "$build/render-mesh-xml.dslb" >"$build/render-mesh-xml-result.json"
"$exe" --dsl "$build/material-principled.dslb" >"$build/material-principled-result.json"
"$exe" --dsl "$build/camera-look-at.dslb" >"$build/camera-look-at-result.json"
"$exe" --dsl "$build/render-exr.dslb" >"$build/render-exr-result.json"
"$exe" --dsl "$build/mesh-instances.dslb" >"$build/mesh-instances-result.json"
cp "$build/mesh-instances.png" "$build/mesh-instances-first.png"
"$exe" --dsl "$build/mesh-instances.dslb" >"$build/mesh-instances-repeat-result.json"
cmp "$build/mesh-instances-first.png" "$build/mesh-instances.png"
"$exe" --dsl "$build/embree-heavy.dslb" >"$build/embree-heavy-result.json"
"$exe" --dsl "$build/render-settings.dslb" >"$build/render-settings-result.json"
"$exe" --dsl "$build/procedural-materials.dslb" >"$build/procedural-materials-result.json"
"$exe" --dsl "$build/orthographic-camera.dslb" >"$build/orthographic-camera-result.json"
"$exe" --dsl "$build/camera-aspect.dslb" >"$build/camera-aspect-result.json"
"$exe" --dsl "$build/camera-aspect-cube.dslb" >"$build/camera-aspect-cube-result.json"
"$exe" --dsl "$build/image-texture.dslb" >"$build/image-texture-result.json"
"$exe" --dsl "$build/display-transforms.dslb" >"$build/display-transforms-result.json"
"$exe" --dsl "$build/render-passes.dslb" >"$build/render-passes-result.json"
"$exe" --dsl "$build/denoise-quality.dslb" >"$build/denoise-quality-result.json" 2>"$build/denoise-quality.log"
"$exe" --dsl "$build/animation-tween.dslb" >"$build/animation-tween-result.json"
"$exe" --dsl "$build/animation-visibility-optics.dslb" >"$build/animation-visibility-optics-result.json"
"$exe" --dsl "$build/motion-blur.dslb" >"$build/motion-blur-result.json"
"$exe" --dsl "$build/material-nodes.dslb" >"$build/material-nodes-result.json"
"$exe" --dsl "$build/material-maps.dslb" >"$build/material-maps-result.json"
"$exe" --dsl "$build/backface-culling.dslb" >"$build/backface-culling-result.json"
"$exe" --dsl "$build/quality-preset.dslb" >"$build/quality-preset-result.json"
"$exe" --dsl "$build/denoise-speed.dslb" >"$build/denoise-speed-result.json"
if "$exe" --dsl "$build/sequence-budget.dslb" >"$build/sequence-budget-result.json"; then
  echo "sequence budget test unexpectedly rendered" >&2
  exit 1
fi
test ! -e "$build/should-not-render-0.png"
python3 - "$build/sequence-budget-result.json" <<'PY'
import json,sys
r=json.load(open(sys.argv[1]))
assert r.get('ok') is False and r.get('code') == 'RUN_HOST', r
m=r.get('message','')
assert '64,000,000' in m and '240 frames' in m and '960x540' in m and '<= 123 frames' in m, r
PY
"$exe" --dsl "$build/material-nodes-control.dslb" >"$build/material-nodes-control-result.json"
for preset in diagram-dark-16x9-draft diagram-dark-16x9-final product-studio turntable-neutral glass-showcase technical-clean; do
  "$exe" --dsl "$build/preset-$preset.dslb" >"$build/preset-$preset-result.json"
done
"$exe" --dsl "$build/preset-explicit-overrides.dslb" >"$build/preset-explicit-overrides-result.json"
python3 tests/preset_explicit_overrides_quality.py "$build"
"$exe" --dsl "$build/preset-external.dslb" >"$build/preset-external-result.json"
"$exe" --dsl "$build/preset-switch.dslb" >"$build/preset-switch-result.json"
"$exe" --presets >"$build/presets-list.txt"
"$exe" --preset-show product-studio >"$build/preset-product-studio.json"
rm -f "$build/render-timeout.png"
if "$exe" --dsl "$build/render-timeout.dslb" >"$build/render-timeout-result.json"; then
  echo "render-timeout test unexpectedly completed" >&2
  exit 1
fi
test ! -e "$build/render-timeout.png"
python3 - "$build/render-timeout-result.json" <<'PY'
import json,sys
r=json.load(open(sys.argv[1]))
assert r.get('ok') is False and r.get('code') == 'RENDER_TIMEOUT', r
assert 'timed out' in r.get('message',''), r
PY
test -s "$build/imported-mesh.png"
test -s "$build/import-ply.png"
test -s "$build/import-gltf-gltf.png"
test -s "$build/import-gltf-glb.png"
test -s "$build/sky-texture.png"
test -s "$build/environment-texture.png"
test -s "$build/transparent-film.png"
test -s "$build/camera-dof.png"
test -s "$build/primitives-gallery.png"
for image in volume-fog volume-control subsurface subsurface-control glass-transmission glass-opaque-control; do test -s "$build/$image.png"; done
test -s "$build/imported-mesh-xml.png"
test -s "$build/material-principled.png"
test -s "$build/camera-look-at.png"
test -s "$build/imported-mesh.exr"
test -s "$build/mesh-instances.png"
test -s "$build/denoise-noisy.png"
test -s "$build/denoise-filtered.png"
test -s "$build/denoise-reference.png"
test -s "$build/quality-draft.png"
test -s "$build/quality-final.png"
test -s "$build/render-settings.png"
test -s "$build/procedural-materials.png"
test -s "$build/orthographic-camera.png"
test -s "$build/image-texture.png"
test -s "$build/display-standard.png"
test -s "$build/display-filmic.png"
test -s "$build/display-agx.png"
for pass in combined depth normal albedo; do
  test -s "$build/passes-ramp-$pass.exr"
  test -s "$build/passes-ramp-$pass.png"
done
test -s "$build/perspective-reference.png"
test -s "$build/material-maps.png"
test -s "$build/material-maps-control.png"
test -s "$build/material-mix-factor.png"
test -s "$build/backface-culling.png"
test -s "$build/motion-blur-2.png"
for frame in 0 1 2; do test -s "$build/animation-visibility-$frame.png"; done
test -s "$build/motion-control.png"
file "$build/imported-mesh.png" | grep -q 'PNG image data'
file "$build/imported-mesh-xml.png" | grep -q 'PNG image data'
python3 tests/png_metrics.py "$build/imported-mesh.png"
python3 tests/png_metrics.py "$build/import-ply.png"
python3 tests/animation_visibility_quality.py "$build/animation-visibility-0.png" "$build/animation-visibility-1.png" "$build/animation-visibility-2.png"
python3 tests/gltf_quality.py "$build/import-gltf-gltf.png"
python3 tests/gltf_quality.py "$build/import-gltf-glb.png"
python3 tests/sky_quality.py "$build/sky-texture.png"
python3 tests/environment_quality.py "$build/environment-texture.png"
python3 tests/transparent_alpha_quality.py "$build/transparent-film.png"
python3 tests/dof_quality.py "$build/camera-dof.png"
python3 tests/primitives_quality.py "$build/primitives-gallery.png"
python3 tests/material_effect_quality.py "$build/volume-control.png" "$build/volume-fog.png"
python3 tests/material_effect_quality.py "$build/subsurface-control.png" "$build/subsurface.png"
python3 tests/material_effect_quality.py "$build/glass-opaque-control.png" "$build/glass-transmission.png"
python3 tests/png_metrics.py "$build/imported-mesh-xml.png"
python3 tests/png_metrics.py "$build/material-principled.png"
python3 tests/png_blue_material.py "$build/material-principled.png"
python3 tests/png_metrics.py "$build/camera-look-at.png"
python3 tests/png_centered_object.py "$build/camera-look-at.png"
python3 tests/exr_header.py "$build/imported-mesh.exr"
python3 tests/png_metrics.py "$build/mesh-instances.png"
python3 tests/png_instances.py "$build/mesh-instances.png"
python3 tests/embree_quality.py tests/fixtures/embree-heavy-bvh2.png "$build/embree-heavy.png"
python3 tests/png_metrics.py "$build/render-settings.png"
python3 tests/procedural_materials_quality.py "$build/procedural-materials.png"
python3 tests/orthographic_camera_quality.py "$build/perspective-reference.png" "$build/orthographic-camera.png"
python3 tests/camera_aspect_quality.py "$build"
python3 tests/image_texture_quality.py "$build/image-texture.png"
python3 tests/denoise_quality.py "$build/denoise-noisy.png" "$build/denoise-filtered.png" "$build/denoise-reference.png"
python3 tests/animation_quality.py "$build"
python3 tests/motion_blur_quality.py "$build"
python3 tests/material_nodes_quality.py "$build/material-nodes.png" "$build/material-nodes-control.png"
python3 tests/material_maps_quality.py "$build"
python3 tests/backface_culling_quality.py "$build/backface-culling.png"
python3 tests/preset_quality.py "$build"
python3 tests/denoise_speed_quality.py "$build/denoise-speed-fast.png" "$build/denoise-speed-reference.png"
# Only an explicitly chosen Blender (5.2 with OIDN) can produce the goldens; a random
# `blender` on PATH is not used.
blender=${BLENDERCLI_BLENDER:-}
if [ -n "$blender" ]; then
  "$blender" --background --factory-startup --python tests/make_display_goldens.py -- "$build" >"$build/display-goldens.log" 2>&1
  python3 tests/display_transform_quality.py \
    "$build/display-standard.png" "$build/display-filmic.png" "$build/display-agx.png" \
    "$build/blender-standard.png" "$build/blender-filmic.png" "$build/blender-agx.png"
  "$blender" --background --factory-startup --python tests/render_passes_quality.py -- "$build/passes-ramp" >"$build/render-passes-quality.log" 2>&1
  tail -n 1 "$build/render-passes-quality.log"
else
  echo "SKIP: set BLENDERCLI_BLENDER to run Blender-based display-transform and pass numeric tests"
fi
python3 tests/png_compare.py "$build/imported-mesh.png" "$build/imported-mesh-xml.png"
diff -u <("${LMSCRIPT:-lmscript}" --dsl-signatures schema/lmblender.dsl.json) <("$exe" --dsl-signatures)
python3 - "$build/version-result.json" "$build/import-mesh-result.json" "$build/render-mesh-result.json" "$build/render-mesh-xml-result.json" "$build/material-principled-result.json" "$build/camera-look-at-result.json" "$build/render-exr-result.json" "$build/mesh-instances-result.json" "$build/render-settings-result.json" "$build/procedural-materials-result.json" "$build/orthographic-camera-result.json" "$build/image-texture-result.json" "$build/display-transforms-result.json" "$build/render-passes-result.json" "$build/animation-tween-result.json" "$build/motion-blur-result.json" "$build/material-nodes-result.json" "$build/material-maps-result.json" "$build/backface-culling-result.json" "$build/material-nodes-control-result.json" "$build/preset-external-result.json" "$build/preset-switch-result.json" <<'PY'
import json,sys
for path in sys.argv[1:]:
    result=json.load(open(path)); assert result.get('ok') is True, result
print('DSL v5 runner, mesh import, direct Cycles rendering, motion blur, display transforms, render passes, and embedded schema pass')
PY
for binary in "$exe" "$build/cycles-static/bin/cycles"; do
  [ ! -x "$binary" ] && continue
  scripts/check-static.sh "$binary" >"$build/ldd-check.txt" 2>&1 || { cat "$build/ldd-check.txt" >&2; exit 1; }
done
