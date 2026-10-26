#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/lmblender-native-inputs-XXXXXX")
trap 'rm -rf "$work"' EXIT
source="$work/source"; cycles="$work/cycles"; prefix="$work/prefix"
mkdir -p "$source/intern/atomic" "$cycles/lib" "$prefix/include/OpenImageIO"
touch "$source/intern/atomic/atomic_ops.h" "$prefix/include/OpenImageIO/paramlist.h"
if "$root/scripts/check-native-inputs.sh" "$source" "$cycles" "$prefix" 2>"$work/missing.log"; then
  echo 'negative preflight unexpectedly passed' >&2; exit 1
fi
grep -q 'cycles_session.lib' "$work/missing.log"
touch "$cycles/lib/cycles_session.lib"
"$root/scripts/check-native-inputs.sh" "$source" "$cycles" "$prefix"
echo 'native input preflight: negative and portable Windows archive checks passed'
