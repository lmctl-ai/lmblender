#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/lmblender-native-inputs-XXXXXX")
trap 'rm -rf "$work"' EXIT
source="$work/source"; cycles="$work/cycles"; prefix="$work/prefix"
mkdir -p "$source/intern/atomic" "$cycles/lib" "$prefix/include/OpenImageIO"
touch "$source/intern/atomic/atomic_ops.h" "$prefix/include/OpenImageIO/paramlist.h"
code=0
"$root/scripts/check-native-inputs.sh" "$source" "$cycles" "$prefix" 2>"$work/missing.log" || code=$?
[ "$code" = 2 ] || { echo "negative preflight exit $code, expected 2" >&2; exit 1; }
grep -q 'cycles_session.lib' "$work/missing.log"
for archive in libcycles_session.a cycles_session.lib libcycles_session.lib; do
  touch "$cycles/lib/$archive"
  "$root/scripts/check-native-inputs.sh" "$source" "$cycles" "$prefix"
  rm "$cycles/lib/$archive"
done
echo 'native input preflight: exit-2 refusal, Unix and portable Windows archive checks passed'
