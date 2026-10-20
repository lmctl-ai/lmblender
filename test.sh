#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd)
cd "$root"
build=${LMBLENDER_BUILD_DIR:-$root/build}
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLMBLENDER_FULL_STATIC=ON
cmake --build "$build" --parallel "${BUILD_JOBS:-2}"
./scripts/sync-sdk.sh --verify
exe="$build/lmblender"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cmp schema/lmblender.dsl.json <("$exe" --schema | head -c -1)
"${LMSCRIPT:-lmscript}" examples/version.lms "$tmp/version.dslb" >/dev/null
"$exe" --dsl "$tmp/version.dslb" >"$tmp/result.json"
"${LMSCRIPT:-lmscript}" --dsl-signatures schema/lmblender.dsl.json >"$tmp/compiler-signatures"
"$exe" --dsl-signatures >"$tmp/host-signatures"
diff -u "$tmp/compiler-signatures" "$tmp/host-signatures"
python3 - "$tmp/result.json" <<'PY'
import json,sys
r=json.load(open(sys.argv[1])); assert r.get('success') is True, r
print('DSL v5 runner and embedded schema pass')
PY
if ldd "$exe" >"$tmp/ldd" 2>&1; then
  if grep -q 'not a dynamic executable' "$tmp/ldd"; then :; else echo 'expected ldd to report static binary' >&2; cat "$tmp/ldd" >&2; exit 1; fi
else
  grep -q 'not a dynamic executable' "$tmp/ldd" || { cat "$tmp/ldd" >&2; exit 1; }
fi
