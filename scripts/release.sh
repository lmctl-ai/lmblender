#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
version=${RELEASE_VERSION:-0.2.0}
binary=${LMBLENDER_BINARY:-$root/build/lmblender}
if [[ ! -x "$binary" ]]; then echo "release binary missing: $binary" >&2; exit 2; fi
ldd_output=$(ldd "$binary" 2>&1 || true)
if ! grep -q 'not a dynamic executable' <<<"$ldd_output"; then echo 'release binary must be fully static (ldd check failed)' >&2; exit 2; fi
if ! "$binary" --version | grep -q "lmblender $version"; then echo "binary version does not match release $version" >&2; exit 2; fi
size=$(stat -c '%s' "$binary")
mkdir -p "$root/dist"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
pkg="$stage/lmblender-linux-arm64"
mkdir -p "$pkg/bin" "$pkg/schema" "$pkg/docs" "$pkg/licenses"
install -m 0755 "$binary" "$pkg/bin/lmblender"
install -m 0644 schema/lmblender.dsl.json "$pkg/schema/lmblender.dsl.json"
install -m 0644 README.md THIRD_PARTY_NOTICES.md "$pkg/"
install -m 0644 docs/LLM_GUIDE.md docs/ANIMATION.md docs/RELINKING.md "$pkg/docs/"
cp -a licenses/. "$pkg/licenses/"
printf 'lmblender %s\n' "$version" >"$pkg/VERSION"
printf 'binary_bytes=%s\n' "$size" >>"$pkg/VERSION"
printf '\nRun: bin/lmblender --version\nSchema: schema/lmblender.dsl.json\n' >>"$pkg/VERSION"
archive="$root/dist/lmblender-linux-arm64-v${version}.tar.gz"
tar -C "$stage" -czf "$archive" lmblender-linux-arm64
sha256sum "$archive" >"$archive.sha256"
echo "Created $archive ($(stat -c '%s' "$archive") bytes compressed); binary=$size bytes"
if [[ ${1:-} == --draft ]]; then
  if gh release view "v${version}" --repo lmctlhq/lmblender >/dev/null 2>&1; then echo "release v${version} already exists" >&2; exit 2; fi
  gh release create "v${version}" "$archive" "$archive.sha256" --repo lmctlhq/lmblender --draft \
    --title "lmblender ${version} (Linux arm64)" --notes-file docs/RELEASE_NOTES.md
fi
