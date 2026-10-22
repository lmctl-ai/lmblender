#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
# Dry run of the release assembly: two Linux artifacts, a macOS and a Windows artifact,
# and the source asset are merged into one dist/ (each arriving with its own stale
# SHA256SUMS, exactly as the per-job uploads do), then scripts/release-checksums.sh must
# produce one SHA256SUMS naming every asset, and must reject a lying *.sha256 sidecar.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
dist="$work/dist"
mkdir -p "$dist"
artifact() { # artifact NAME FILE...  — simulate one job's upload dir, then merge it into dist/
  local name=$1; shift
  local dir="$work/$name"; mkdir -p "$dir"
  for file in "$@"; do printf '%s payload for %s\n' "$file" "$name" >"$dir/$file"; (cd "$dir" && sha256 "$file" >"$file.sha256"); done
  (cd "$dir" && sha256 "$@" >SHA256SUMS)   # the stale per-job file
  cp "$dir"/* "$dist"/                     # merge-multiple: later copies overwrite SHA256SUMS
}
artifact lmblender-linux-arm64 lmblender-linux-arm64 lmblender-linux-arm64-v9.9.9.tar.gz lmblender-linux-arm64.ldd
artifact lmblender-linux-x64 lmblender-linux-x64 lmblender-linux-x64-v9.9.9.tar.gz lmblender-linux-x64.ldd
artifact lmblender-macos-arm64 lmblender-macos-arm64 lmblender-macos-arm64-v9.9.9.tar.gz
artifact lmblender-windows-x64 lmblender-windows-x64.exe lmblender-windows-x64-v9.9.9.zip
artifact lmblender-source lmblender-source-v9.9.9.tar.gz
printf 'notices\n' >"$dist/lmblender-notices-lmblender-linux-x64-v9.9.9.md"

"$root/scripts/release-checksums.sh" "$dist" >/dev/null
expected=$(find "$dist" -maxdepth 1 -type f ! -name SHA256SUMS | wc -l | tr -d ' ')
listed=$(grep -c '' "$dist/SHA256SUMS")
[[ "$listed" == "$expected" ]] || { echo "SHA256SUMS lists $listed of $expected assets" >&2; cat "$dist/SHA256SUMS" >&2; exit 1; }
for asset in lmblender-linux-arm64 lmblender-linux-x64 lmblender-macos-arm64-v9.9.9.tar.gz lmblender-windows-x64.exe lmblender-source-v9.9.9.tar.gz lmblender-linux-arm64.ldd; do
  grep -q " [ *]$asset\$" "$dist/SHA256SUMS" || { echo "SHA256SUMS lacks $asset" >&2; exit 1; }
done
(cd "$dist" && if command -v sha256sum >/dev/null; then sha256sum --check --strict --quiet SHA256SUMS; else shasum -a 256 --check --strict --quiet SHA256SUMS; fi)

# A corrupted sidecar (asset replaced after its .sha256 was written) must be refused.
printf 'tampered\n' >"$dist/lmblender-linux-x64"
if "$root/scripts/release-checksums.sh" "$dist" >/dev/null 2>"$work/err"; then
  echo "release-checksums accepted a sidecar that disagrees with its asset" >&2; exit 1
fi
grep -q "disagrees" "$work/err"
echo "release checksum assembly: one SHA256SUMS over $expected merged assets; tampered sidecar refused"
