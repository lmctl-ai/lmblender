#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
#
# Build the corresponding-source release asset: dist/lmblender-source-v<version>.tar.gz
#
#   lmblender-source-v<version>/
#     lmblender/              repository tree at the release commit (git archive)
#     upstream/               pinned Blender subset, patched Cycles overlay, and every
#                             third-party source archive from SOURCES.json (hash-verified)
#     SOURCES.json, SOURCES.md, README.md
#
# Environment: RELEASE_VERSION (default VERSION), BLENDER_SOURCE_TREE (pinned checkout
# from scripts/fetch-cycles-source.sh), BLENDER_PACKAGES_DIR (optional cache of the
# Blender build_environment downloads), LMBLENDER_SOURCE_FETCH=0 to skip downloading
# archives that are not cached locally (the manifest then lists them as download-only).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
version=${RELEASE_VERSION:-$(cat VERSION)}
version=${version#v}
source_tree=${BLENDER_SOURCE_TREE:-$root/build/blender-src-upstream-v5.2.2}
packages_dir=${BLENDER_PACKAGES_DIR:-${BLENDER_DEPS_ENV_BUILD:-$root/build/blender-deps-environment}/packages}
fetch=${LMBLENDER_SOURCE_FETCH:-1}
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }

commit=$(sed -n 's/^commit=//p' scripts/fetch-cycles-source.sh)
[[ -d "$source_tree/.git" ]] || { echo "pinned Blender checkout missing at $source_tree; run scripts/fetch-cycles-source.sh" >&2; exit 2; }
actual=$(git -C "$source_tree" rev-parse HEAD)
[[ "$actual" == "$commit" ]] || { echo "Blender source pin mismatch: $actual != $commit" >&2; exit 2; }
blender_version=$(sed -n "s/.*Blender \(v[0-9.]*\) source pinned.*/\1/p" scripts/fetch-cycles-source.sh | head -1)
lm_commit=$(git -C "$root" rev-parse HEAD)
if [[ -n "$(git -C "$root" status --porcelain --untracked-files=no)" ]]; then
  echo "warning: working tree has uncommitted changes; the source asset records commit $lm_commit" >&2
fi

name=lmblender-source-v$version
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
pkg=$stage/$name
mkdir -p "$pkg/lmblender" "$pkg/upstream"

# 1. lmblender at the release commit.
git -C "$root" archive --format=tar HEAD | tar -x -C "$pkg/lmblender"
printf '%s\n' "$lm_commit" >"$pkg/lmblender/GIT_COMMIT"

# 2. The Blender source subset exactly as checked out (sparse cone list), GPL intern code included.
mapfile -t sparse < <(git -C "$source_tree" sparse-checkout list 2>/dev/null || true)
if [[ ${#sparse[@]} -eq 0 ]]; then
  # A full checkout: take the path list scripts/fetch-cycles-source.sh pins.
  mapfile -t sparse < <(awk '/sparse-checkout set/ { f=1; sub(/.*sparse-checkout set/, "") }
    f { line=$0; sub(/\\$/, "", line); n=split(line, a, /[ \t]+/); for (i=1;i<=n;i++) if (a[i]!="") print a[i]; if ($0 !~ /\\$/) f=0 }' scripts/fetch-cycles-source.sh)
fi
[[ ${#sparse[@]} -gt 0 ]] || { echo "could not determine the pinned Blender source paths" >&2; exit 2; }
subset="$pkg/upstream/blender-${blender_version}-${commit:0:12}-subset.tar.gz"
git -C "$source_tree" archive --format=tar.gz --prefix="blender-${commit:0:12}/" -o "$subset" "$commit" -- "${sparse[@]}"

# 3. The patched Cycles tree the static archives are compiled from.
python3 scripts/make-cycles-source-overlay.py "$source_tree" "$stage/overlay" >/dev/null
tar -C "$stage/overlay/intern" -czf "$pkg/upstream/cycles-overlay-patched-${commit:0:12}.tar.gz" cycles

# Windows uses the standalone Cycles build system and its third_party sources.
# Include the exact pinned tree; the repository archive carries the overlay
# script and MSVC patch needed to reproduce the Windows source tree.
cycles_commit=$(sed -n 's/^cycles_commit=\([0-9a-f]*\).*/\1/p' scripts/configure-cycles-portable.sh)
cycles_repo=$(sed -n 's/^cycles_repo=//p' scripts/configure-cycles-portable.sh)
git init -q "$stage/standalone"
git -C "$stage/standalone" fetch -q --depth 1 "$cycles_repo" "$cycles_commit"
[[ "$(git -C "$stage/standalone" rev-parse FETCH_HEAD)" == "$cycles_commit" ]]
git -C "$stage/standalone" archive --format=tar.gz --prefix="cycles-standalone/" \
  -o "$pkg/upstream/cycles-standalone-${cycles_commit:0:12}.tar.gz" "$cycles_commit"

# 4. Third-party source archives: reuse a local build_environment download cache when present.
if [[ -d "$packages_dir" ]]; then
  find "$packages_dir" -maxdepth 1 -type f \( -name '*.tar.*' -o -name '*.zip' -o -name '*.tgz' \) -print0 |
    while IFS= read -r -d '' file; do [[ -e "$pkg/upstream/$(basename "$file")" ]] || cp "$file" "$pkg/upstream/"; done
fi
for extra in ${LMBLENDER_SOURCE_EXTRA_ARCHIVES:-}; do [[ -e "$pkg/upstream/$(basename "$extra")" ]] || cp "$extra" "$pkg/upstream/"; done
fetch_flag=(); [[ "$fetch" == 1 ]] && fetch_flag=(--fetch)
python3 scripts/source-manifest.py --blender "$source_tree" --version "$version" --commit "$lm_commit" \
  --upstream-dir "$pkg/upstream" "${fetch_flag[@]}" --json "$pkg/SOURCES.json" --markdown "$pkg/SOURCES.md"
# Drop cached archives that are not part of the manifest (keeps the asset to the audited closure).
python3 - "$pkg" <<'EOF'
import json, os, sys
pkg = sys.argv[1]
keep = {c["file"] for c in json.load(open(os.path.join(pkg, "SOURCES.json")))["components"] if c.get("file")}
for entry in os.listdir(os.path.join(pkg, "upstream")):
    if entry not in keep:
        os.remove(os.path.join(pkg, "upstream", entry))
EOF

cat >"$pkg/README.md" <<EOF
# lmblender ${version} corresponding source

This archive is the corresponding source of the \`lmblender\` ${version} binaries
(combined work licensed GPL-3.0-or-later; see lmblender/LICENSE and
lmblender/docs/LICENSE_AUDIT.md).

- \`lmblender/\`: repository tree at commit ${lm_commit} (host source, schema, presets,
  vendored DSL VM and JSON library, Cycles patches, build scripts, license texts).
- \`upstream/\`: pinned Blender ${blender_version} source subset (commit ${commit}), the
  patched Cycles overlay that was compiled, and the third-party source archives listed
  in SOURCES.md, each verified against its pinned hash.
- \`SOURCES.json\` / \`SOURCES.md\`: component manifest with versions, URLs, hashes,
  licenses, and the toolchain description.

Rebuild: follow lmblender/BUILD.md. The scripts download nothing that is not listed in
SOURCES.json; point them at the archives in upstream/ with
\`BLENDER_PACKAGES_DIR=\$PWD/upstream\` or let Blender's build_environment re-download the
same hash-verified files. Relinking the static runtime: lmblender/docs/RELINKING.md.
EOF

mkdir -p "$root/dist"
archive="$root/dist/$name.tar.gz"
tar_opts=()
if tar --version 2>/dev/null | grep -q GNU; then tar_opts=(--sort=name --owner=0 --group=0 --numeric-owner --mtime="@${SOURCE_DATE_EPOCH:-0}"); fi
tar -C "$stage" "${tar_opts[@]}" -czf "$archive" "$name"
(cd "$root/dist" && sha256 "$(basename "$archive")" >"$(basename "$archive").sha256")
echo "Created $archive ($(wc -c <"$archive" | tr -d ' ') bytes); $(ls "$pkg/upstream" | wc -l | tr -d ' ') upstream archives"
