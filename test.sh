#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd)
cd "$root"
build=${LMBLENDER_BUILD_DIR:-$root/build}
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLMBLENDER_FULL_STATIC=ON
cmake --build "$build" --parallel "${BUILD_JOBS:-2}"
./scripts/sync-sdk.sh --verify
exe="$build/lmblender"
cmp schema/lmblender.dsl.json <("$exe" --schema | head -c -1)
"${LMSCRIPT:-lmscript}" examples/version.lms "$build/version.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/import-mesh.lms "$build/import-mesh.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/render-mesh.lms "$build/imported-mesh.png" "$build/render-mesh.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/render-mesh-xml-oracle.lms "$build/render-mesh-xml.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/material-principled.lms "$build/material-principled.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/camera-look-at.lms "$build/camera-look-at.dslb" >/dev/null
"${LMSCRIPT:-lmscript}" examples/render-timeout.lms "$build/render-timeout.dslb" >/dev/null
"$exe" --dsl "$build/version.dslb" >"$build/version-result.json"
"$exe" --dsl "$build/import-mesh.dslb" >"$build/import-mesh-result.json"
"$exe" --dsl "$build/render-mesh.dslb" >"$build/render-mesh-result.json"
LMBLENDER_TEST_XML_ORACLE=1 "$exe" --dsl "$build/render-mesh-xml.dslb" >"$build/render-mesh-xml-result.json"
"$exe" --dsl "$build/material-principled.dslb" >"$build/material-principled-result.json"
"$exe" --dsl "$build/camera-look-at.dslb" >"$build/camera-look-at-result.json"
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
test -s "$build/imported-mesh-xml.png"
test -s "$build/material-principled.png"
test -s "$build/camera-look-at.png"
file "$build/imported-mesh.png" | grep -q 'PNG image data'
file "$build/imported-mesh-xml.png" | grep -q 'PNG image data'
python3 tests/png_metrics.py "$build/imported-mesh.png"
python3 tests/png_metrics.py "$build/imported-mesh-xml.png"
python3 tests/png_metrics.py "$build/material-principled.png"
python3 tests/png_blue_material.py "$build/material-principled.png"
python3 tests/png_metrics.py "$build/camera-look-at.png"
python3 tests/png_centered_object.py "$build/camera-look-at.png"
python3 tests/png_compare.py "$build/imported-mesh.png" "$build/imported-mesh-xml.png"
diff -u <("${LMSCRIPT:-lmscript}" --dsl-signatures schema/lmblender.dsl.json) <("$exe" --dsl-signatures)
python3 - "$build/version-result.json" "$build/import-mesh-result.json" "$build/render-mesh-result.json" "$build/render-mesh-xml-result.json" "$build/material-principled-result.json" "$build/camera-look-at-result.json" <<'PY'
import json,sys
for path in sys.argv[1:]:
    result=json.load(open(path)); assert result.get('ok') is True, result
print('DSL v5 runner, OBJ/ASCII/binary STL import, direct Cycles API PNG render, XML oracle comparison, and embedded schema pass')
PY
for binary in "$exe" "$build/cycles-static/bin/cycles"; do
  [ ! -x "$binary" ] && continue
  ldd "$binary" >"$build/ldd-check.txt" 2>&1 || true
  grep -q 'not a dynamic executable' "$build/ldd-check.txt" || { cat "$build/ldd-check.txt" >&2; exit 1; }
done
