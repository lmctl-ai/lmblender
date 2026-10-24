#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
# Validate same-run platform artifacts before either dry-run upload or publication.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
dist=${1:?usage: assemble-release.sh DIST_DIR}
# Automatic releases require all targets; exploratory/manual assembly may omit
# optional targets. Never infer completeness from continue-on-error job results.
required=${REQUIRE_PLATFORMS:-linux}
case "$required" in
  all|linux) ;;
  *) echo "invalid REQUIRE_PLATFORMS: $required (expected all or linux)" >&2; exit 1 ;;
esac
echo "Required release platforms: $required"
version=${RELEASE_VERSION:-$(cat "$root/VERSION")}
version=${version#v}
[[ "$version" == "$(cat "$root/VERSION")" ]] || { echo 'release version differs from VERSION' >&2; exit 1; }
require() { [[ -s "$dist/$1" ]] || { echo "missing release asset: $1" >&2; exit 1; }; }
require "lmblender-source-v$version.tar.gz"
require "lmblender-source-v$version.tar.gz.sha256"
for platform in linux-arm64 linux-x64 macos-arm64 macos-x64 windows-x64; do
  base=lmblender-$platform
  binary=$base
  [[ $platform != windows-* ]] || binary=$base.exe
  if [[ $required != all && $platform != linux-* && ! -e "$dist/$binary" ]]; then
    # Optional means absent, never partially staged.
    if compgen -G "$dist/*$platform*" >/dev/null; then
      echo "incomplete optional platform: $platform" >&2; exit 1
    fi
    echo "Optional platform absent: $platform"
    continue
  fi
  require "$binary"
  require "$binary.sha256"
  require "$base-v$version.tar.gz"
  require "$base-v$version.tar.gz.sha256"
  for document in notices source-offer relinking; do
    require "lmblender-$document-$base-v$version.md"
  done
  # Bare binaries are accompanied by the full license texts in their archive.
  contents=$(tar -tzf "$dist/$base-v$version.tar.gz")
  for member in LICENSE SOURCE_OFFER.md THIRD_PARTY_NOTICES.md docs/RELINKING.md; do
    grep -Fxq "$base/$member" <<<"$contents" || { echo "$base archive lacks $member" >&2; exit 1; }
  done
  echo "Validated release platform: $platform"
done
# Also cross-check every platform/source sidecar, without relying on the path
# recorded by a different runner's checksum utility.
"$root/scripts/release-checksums.sh" "$dist"
