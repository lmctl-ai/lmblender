#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
#
# Assemble ONE SHA256SUMS over every release asset in a directory.
#
#   scripts/release-checksums.sh DIST_DIR
#
# Per-platform build jobs each used to write their own SHA256SUMS; merging their
# artifacts into one directory kept only one of them. This replaces any stale
# SHA256SUMS* with a single sorted list covering all files (binaries, archives,
# notices, source asset, .ldd/.otool reports, per-file *.sha256 sidecars), then
# verifies the list and cross-checks every *.sha256 sidecar against it.
set -euo pipefail
dist=${1:?usage: release-checksums.sh DIST_DIR}
cd "$dist"
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
rm -f SHA256SUMS SHA256SUMS-* SHA256SUMS.txt
assets=()
while IFS= read -r name; do assets+=("$name"); done < <(find . -maxdepth 1 -type f ! -name '.*' -exec basename {} \; | LC_ALL=C sort)
[[ ${#assets[@]} -gt 0 ]] || { echo "release-checksums: no assets in $dist" >&2; exit 2; }
sha256 "${assets[@]}" > SHA256SUMS
# Independent verification of what was just written.
if command -v sha256sum >/dev/null; then sha256sum --check --strict SHA256SUMS >/dev/null; else shasum -a 256 --check --strict SHA256SUMS >/dev/null; fi
# Every per-file sidecar (release.sh / release-source.sh write "<hash>  <path>") must agree.
for sidecar in *.sha256; do
  [[ -f "$sidecar" ]] || continue
  asset=${sidecar%.sha256}
  [[ -f "$asset" ]] || { echo "release-checksums: $sidecar has no matching asset $asset" >&2; exit 3; }
  expected=$(awk '{print $1}' "$sidecar")
  actual=$(awk -v a="$asset" '$2 == a {print $1}' SHA256SUMS)
  [[ "$expected" == "$actual" ]] || { echo "release-checksums: $sidecar ($expected) disagrees with $asset ($actual)" >&2; exit 3; }
done
echo "SHA256SUMS covers ${#assets[@]} assets in $dist"
