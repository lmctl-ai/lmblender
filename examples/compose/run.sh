#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cad_source=${LMCAD_SOURCE:-/home/ubuntu/repos/lmcad}
cad_bin=${LMCAD_BIN:-/home/ubuntu/lmcad-poc-build/lmcad-build/lmcad}
host_bin=${LMBLENDER_BIN:-$root/build/lmblender}
out="$root/build/compose-input"
mkdir -p "$out/build"

for cad in mug plate-with-holes; do
  ( cd "$cad_source" && "${LMSCRIPT:-lmscript}" "examples/$cad.lms" "$out/$cad.dslb" >/dev/null )
  ( cd "$out" && "$cad_bin" --dsl "$out/$cad.dslb" )
done

for item in mug plate; do
  "${LMSCRIPT:-lmscript}" "$root/examples/compose/$item-render.lms" "$out/$item-render.dslb" >/dev/null
  "$host_bin" --dsl "$out/$item-render.dslb"
  python3 "$root/tests/png_metrics.py" "$root/build/compose-$item.png"
done
echo "Wrote $root/build/compose-mug.png and compose-plate.png"
