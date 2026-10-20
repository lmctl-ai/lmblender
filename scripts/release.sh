#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
version=${RELEASE_VERSION:-0.2.2}
version=${version#v}
binary=${LMBLENDER_BINARY:-$root/build/lmblender}
machine=${LMBLENDER_ARCH:-$(uname -m)}
case "$machine" in
  aarch64|arm64) artifact=lmblender-linux-arm64 ;;
  x86_64|amd64|x64) artifact=lmblender-linux-x64 ;;
  *) echo "unsupported release architecture: $machine" >&2; exit 2 ;;
esac
if [[ ! -x "$binary" ]]; then echo "release binary missing: $binary" >&2; exit 2; fi
ldd_output=$(ldd "$binary" 2>&1 || true)
if ! grep -q 'not a dynamic executable' <<<"$ldd_output"; then echo 'release binary must be fully static (ldd check failed)' >&2; exit 2; fi
if ! "$binary" --version | grep -q "lmblender $version"; then echo "binary version does not match release $version" >&2; exit 2; fi
size=$(stat -c '%s' "$binary")
mkdir -p "$root/dist"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
pkg="$stage/$artifact"
mkdir -p "$pkg/bin" "$pkg/schema" "$pkg/docs" "$pkg/licenses" "$pkg/examples"
install -m 0755 "$binary" "$pkg/bin/lmblender"
install -m 0644 schema/lmblender.dsl.json "$pkg/schema/lmblender.dsl.json"
install -m 0644 README.md THIRD_PARTY_NOTICES.md LICENSE SOURCE_OFFER.md BUILD.md "$pkg/"
install -m 0644 docs/LLM_GUIDE.md docs/ANIMATION.md docs/RELINKING.md docs/LICENSE_AUDIT.md docs/CONTRIBUTING.md docs/VIDEO-QA.md "$pkg/docs/"
cp -a licenses/. "$pkg/licenses/"
cp -a examples/. "$pkg/examples/"
printf 'lmblender %s (%s)\n' "$version" "$artifact" >"$pkg/VERSION"
printf 'binary_bytes=%s\n' "$size" >>"$pkg/VERSION"
printf '\nRun: bin/lmblender --version\nSchema: schema/lmblender.dsl.json\n' >>"$pkg/VERSION"
archive="$root/dist/${artifact}-v${version}.tar.gz"
tar -C "$stage" -czf "$archive" "$artifact"
sha256sum "$archive" >"$archive.sha256"
bare="$root/dist/${artifact}-v${version}"
install -m 0755 "$binary" "$bare"
sha256sum "$bare" >"$bare.sha256"
host_root=${LMBLENDER_HOST_RELEASE:-/home/ubuntu/lmblender-release}
mkdir -p "$host_root/versions/v${version}"
cp -a "$pkg/." "$host_root/versions/v${version}/"
ln -sfn "versions/v${version}" "$host_root/latest"
ln -sfn latest "$host_root/current"
for path in bin schema docs examples; do ln -sfn "current/$path" "$host_root/$path"; done
echo "Installed stable host layout at $host_root (current -> v${version})"
echo "Created $archive ($(stat -c '%s' "$archive") bytes compressed); binary=$size bytes; bare=$bare"
if [[ ${1:-} == --draft ]]; then
  if gh release view "v${version}" --repo lmctlhq/lmblender >/dev/null 2>&1; then
    gh release upload "v${version}" "$archive" "$archive.sha256" "$bare" "$bare.sha256" --repo lmctlhq/lmblender --clobber
    gh release edit "v${version}" --repo lmctlhq/lmblender --draft --notes-file docs/RELEASE_NOTES.md
  else
    gh release create "v${version}" "$archive" "$archive.sha256" "$bare" "$bare.sha256" --repo lmctlhq/lmblender --draft \
      --title "lmblender ${version} (${artifact})" --notes-file docs/RELEASE_NOTES.md
  fi
fi
