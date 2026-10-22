#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
version=${RELEASE_VERSION:-$(cat "$root/VERSION")}
version=${version#v}
case "$(uname -s)" in
  Linux) os=linux exe= ;;
  Darwin) os=macos exe= ;;
  MINGW*|MSYS*|CYGWIN*) os=windows exe=.exe ;;
  *) echo "unsupported release OS: $(uname -s)" >&2; exit 2 ;;
esac
binary=${LMBLENDER_BINARY:-$root/build/lmblender$exe}
machine=${LMBLENDER_ARCH:-$(uname -m)}
case "$machine" in
  aarch64|arm64) arch=arm64 ;;
  x86_64|amd64|x64) arch=x64 ;;
  *) echo "unsupported release architecture: $machine" >&2; exit 2 ;;
esac
# lmscript `_dsl_ install=` convention: PROGRAM-OS-ARCH[.exe]
artifact=lmblender-$os-$arch
filesize() { wc -c <"$1" | tr -d ' '; }
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
if [[ ! -x "$binary" ]]; then echo "release binary missing: $binary" >&2; exit 2; fi
"$root/scripts/check-static.sh" "$binary" >/dev/null || { echo 'release binary must only depend on OS libraries (check-static.sh failed)' >&2; exit 2; }
binary_version=$("$binary" --version | sed -n '1s/^lmblender \([0-9][^ ]*\) .*/\1/p')
if [[ "$binary_version" != "$version" ]]; then echo "binary version $binary_version does not match release $version" >&2; exit 2; fi
size=$(filesize "$binary")
mkdir -p "$root/dist"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
pkg="$stage/$artifact"
mkdir -p "$pkg/bin" "$pkg/schema" "$pkg/docs" "$pkg/licenses" "$pkg/examples"
install -m 0755 "$binary" "$pkg/bin/lmblender$exe"
install -m 0644 schema/lmblender.dsl.json "$pkg/schema/lmblender.dsl.json"
install -m 0644 README.md THIRD_PARTY_NOTICES.md LICENSE SOURCE_OFFER.md BUILD.md "$pkg/"
install -m 0644 docs/LLM_GUIDE.md docs/ANIMATION.md docs/THROUGHPUT.md docs/OIDN_AUDIT.md docs/PRESETS.md docs/RELINKING.md docs/LICENSE_AUDIT.md docs/CONTRIBUTING.md docs/VIDEO-QA.md docs/USING_FROM_POC.md "$pkg/docs/"
cp -a licenses/. "$pkg/licenses/"
cp -a examples/. "$pkg/examples/"
printf 'lmblender %s (%s)\n' "$version" "$artifact" >"$pkg/VERSION"
printf 'binary_bytes=%s\n' "$size" >>"$pkg/VERSION"
printf '\nRun: bin/lmblender%s --version\nSchema: schema/lmblender.dsl.json\n' "$exe" >>"$pkg/VERSION"
archive="$root/dist/${artifact}-v${version}.tar.gz"
tar -C "$stage" -czf "$archive" "$artifact"
(cd "$root/dist" && sha256 "$(basename "$archive")" >"$(basename "$archive").sha256")
install -m 0644 THIRD_PARTY_NOTICES.md "$root/dist/lmblender-notices-${artifact}-v${version}.md"
install -m 0644 SOURCE_OFFER.md "$root/dist/lmblender-source-offer-${artifact}-v${version}.md"
notices="$root/dist/lmblender-notices-${artifact}-v${version}.md"
source_offer="$root/dist/lmblender-source-offer-${artifact}-v${version}.md"
relinking="$root/dist/lmblender-relinking-${artifact}-v${version}.md"
install -m 0644 docs/RELINKING.md "$relinking"
bare="$root/dist/${artifact}${exe}"
install -m 0755 "$binary" "$bare"
(cd "$root/dist" && sha256 "$(basename "$bare")" >"$(basename "$bare").sha256")
host_root=${LMBLENDER_HOST_RELEASE:-$root/build/host-release}
mkdir -p "$host_root/versions/v${version}"
cp -a "$pkg/." "$host_root/versions/v${version}/"
ln -sfn "versions/v${version}" "$host_root/latest"
ln -sfn latest "$host_root/current"
for path in bin schema docs examples; do ln -sfn "current/$path" "$host_root/$path"; done
echo "Installed stable host layout at $host_root (current -> v${version})"
echo "Created $archive ($(filesize "$archive") bytes compressed); binary=$size bytes; bare=$bare"
if [[ ${1:-} == --publish ]]; then
  # GPL-3.0: binaries are never published without the corresponding-source asset
  # produced by scripts/release-source.sh (CI builds it in the `source` job).
  source_asset="$root/dist/lmblender-source-v${version}.tar.gz"
  if [[ ! -f "$source_asset" || ! -f "$source_asset.sha256" ]]; then
    echo "corresponding-source asset missing: $source_asset (run scripts/release-source.sh first)" >&2; exit 2
  fi
  assets=("$archive" "$archive.sha256" "$bare" "$bare.sha256" "$notices" "$source_offer" "$relinking" "$source_asset" "$source_asset.sha256")
  if gh release view "v${version}" --repo lmctl-ai/lmblender >/dev/null 2>&1; then
    gh release upload "v${version}" "${assets[@]}" --repo lmctl-ai/lmblender --clobber
    gh release edit "v${version}" --repo lmctl-ai/lmblender --draft=false --notes-file docs/RELEASE_NOTES.md
  else
    gh release create "v${version}" "${assets[@]}" --repo lmctl-ai/lmblender \
      --title "lmblender ${version} (${artifact})" --notes-file docs/RELEASE_NOTES.md
  fi
fi
