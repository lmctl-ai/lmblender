#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
# Fail if an aarch64 executable contains SVE instructions outside glibc's runtime-dispatched
# A64FX/SVE string routines. Usage: scripts/check-no-sve.sh build/lmblender
set -euo pipefail
exe=${1:?usage: check-no-sve.sh EXECUTABLE}
objdump=${OBJDUMP:-objdump}
report=$("$objdump" -d --no-show-raw-insn "$exe" | awk '
  /^[0-9a-f]+ <.*>:$/ { fn = $2; next }
  fn ~ /a64fx|_sve/ { next }
  / z[0-9]+\.[bhsdq]|[ ,]p[0-9]+\.[bhsd]|\tptrue|\twhilel[ot]/ { count[fn]++ }
  END { for (f in count) print count[f], f }' | sort -rn)
if [[ -n "$report" ]]; then
  echo "SVE instructions found in $exe (would SIGILL on CPUs without SVE):" >&2
  echo "$report" | head -20 >&2
  exit 1
fi
echo "no SVE instructions outside glibc dispatch routines: $exe"
