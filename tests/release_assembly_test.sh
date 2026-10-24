#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
version=$(cat "$root/VERSION")
dist=$work/dist
mkdir -p "$dist"
asset() { printf 'fixture\n' >"$dist/$1"; (cd "$dist" && sha256sum "$1" >"$1.sha256"); }
platform() {
  local base=lmblender-$1 binary=lmblender-$1
  [[ $1 != windows-* ]] || binary=$binary.exe
  asset "$binary"
  mkdir -p "$work/$base/docs"
  for member in LICENSE SOURCE_OFFER.md THIRD_PARTY_NOTICES.md docs/RELINKING.md; do
    printf 'fixture\n' >"$work/$base/$member"
  done
  tar -C "$work" -czf "$dist/$base-v$version.tar.gz" "$base"
  (cd "$dist" && sha256sum "$base-v$version.tar.gz" >"$base-v$version.tar.gz.sha256")
  for doc in notices source-offer relinking; do printf 'fixture\n' >"$dist/lmblender-$doc-$base-v$version.md"; done
}
policy=linux
assemble() { REQUIRE_PLATFORMS="$policy" "$root/scripts/assemble-release.sh" "$dist" >"$work/log" 2>&1; }
reject() { if assemble; then echo "unexpected assembly success: $1" >&2; exit 1; fi; }
asset "lmblender-source-v$version.tar.gz"
platform linux-arm64
platform linux-x64
assemble # Optional platform failures/absence must not block Linux.
platform windows-x64
platform macos-arm64
platform macos-x64
assemble
policy=all
assemble # Complete automatic release must pass the same packaging/checksum gates.
# Reproduce v0.3.127: an entire platform job is absent, with no partial files.
# Check every target, not just the Windows case that exposed the regression.
for missing in linux-arm64 linux-x64 macos-arm64 macos-x64 windows-x64; do
  mkdir "$work/missing"
  mv "$dist/"*"$missing"* "$work/missing/"
  reject "strict release with only 4/5 platforms (missing $missing)"
  grep -q "missing release asset: lmblender-$missing" "$work/log"
  if [[ $missing == windows-x64 ]]; then
    policy=linux
    assemble # Exploratory manual dry runs remain tolerant.
    policy=all
  fi
  mv "$work/missing/"* "$dist/"
  rmdir "$work/missing"
done
for companion in lmblender-windows-x64.exe.sha256 "lmblender-notices-lmblender-windows-x64-v$version.md"; do
  mv "$dist/$companion" "$work/companion"
  reject "strict release missing $companion"
  mv "$work/companion" "$dist/$companion"
done
assemble
(cd "$dist" && sha256sum --check --strict --quiet SHA256SUMS)
mv "$dist/lmblender-source-v$version.tar.gz" "$work/source"
reject 'missing source'
mv "$work/source" "$dist/lmblender-source-v$version.tar.gz"
mv "$dist/lmblender-windows-x64.exe" "$work/exe"
reject 'partial optional platform'
mv "$work/exe" "$dist/lmblender-windows-x64.exe"
rm "$dist/lmblender-relinking-lmblender-windows-x64-v$version.md"
reject 'missing Windows relinking information'
printf 'fixture\n' >"$dist/lmblender-relinking-lmblender-windows-x64-v$version.md"
printf 'tampered\n' >"$dist/lmblender-windows-x64.exe"
reject 'corrupt executable'
echo 'release assembly: tolerant Linux-only and strict all-platform bundles pass; every 4/5 bundle, missing checksum/notices/source/relinking, partial platform and corruption rejected'
