#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cad_source=${LMCAD_SOURCE:-$root/../lmcad}
cad_bin=${LMCAD_BIN:-$cad_source/build/lmcad}
host_bin=${LMBLENDER_BIN:-$root/build/lmblender}
host_dir=$(dirname "$host_bin")
export PATH="$host_dir:$PATH"
out="$root/build/compose-input"
mkdir -p "$out/build"

for cad in mug plate-with-holes; do
  ( cd "$cad_source" && "${LMSCRIPT:-lmscript}" "examples/$cad.lms" "$out/$cad.dslb" >/dev/null )
  ( cd "$out" && "$cad_bin" --lmcode "$out/$cad.dslb" )
done

sed "s|schema=\"../lmcad/schema/lmcad.dsl.json\"|schema=\"$cad_source/schema/lmcad.dsl.json\"|" "$root/examples/compose/bracket-cad.lms" >"$out/bracket-cad.lms"
"${LMSCRIPT:-lmscript}" "$out/bracket-cad.lms" "$out/bracket-cad.dslb" >/dev/null
( cd "$out" && "$cad_bin" --lmcode "$out/bracket-cad.dslb" )

for item in mug plate; do
  "${LMSCRIPT:-lmscript}" "$root/examples/compose/$item-render.lms" "$out/$item-render.dslb" >/dev/null
  python3 "$root/tests/png_metrics.py" "$root/build/compose-$item.png"
  if [[ "$item" == mug ]]; then
    python3 "$root/tests/compose_mug_quality.py" "$root/build/compose-mug.png" "$out/build/mug.stl"
  else
    python3 "$root/tests/compose_plate_quality.py" "$root/build/compose-plate.png"
  fi
done

"${LMSCRIPT:-lmscript}" "$root/examples/compose/bracket-render.lms" "$out/bracket-render.dslb" >/dev/null
for view in front side rear; do
  python3 "$root/tests/png_metrics.py" "$root/build/compose-bracket-$view.png"
  python3 "$root/tests/compose_bracket_quality.py" "$root/build/compose-bracket-$view.png" "$view"
done
for item in turntable-mug turntable-bracket; do
  "${LMSCRIPT:-lmscript}" "$root/examples/compose/$item.lms" "$out/$item.dslb" >/dev/null
done
python3 "$root/tests/turntable_quality.py" "$root/build"
for item in studio-mug studio-bracket studio-plate; do
  "${LMSCRIPT:-lmscript}" "$root/examples/compose/$item.lms" "$out/$item.dslb" >/dev/null
  python3 "$root/tests/studio_quality.py" "$root/build/$item.png"
done
echo "Wrote mug/plate/bracket compositions and eight-angle mug/bracket turntable image sets under $root/build/"
