#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source=${BLENDER_SOURCE_TREE:-$repo/build/blender-src-upstream-v5.2.2}
url=https://github.com/blender/blender.git
commit=d13f752e3b9c4f8c261cda552b1021f8bcc0382c

if [[ -e "$source" && ! -d "$source/.git" ]]; then
  echo "source path exists but is not a git checkout: $source" >&2
  exit 2
fi
if [[ ! -d "$source/.git" ]]; then
  mkdir -p "$(dirname "$source")"
  git clone --filter=blob:none --no-checkout "$url" "$source"
fi
origin=$(git -C "$source" remote get-url origin)
[[ "$origin" == "$url" || "$origin" == "git@github.com:blender/blender.git" ]] || {
  echo "unexpected Blender source remote: $origin" >&2; exit 2;
}
git -C "$source" sparse-checkout init --cone
git -C "$source" sparse-checkout set \
  CMakeLists.txt build_files/cmake build_files/build_environment intern/cycles intern/atomic \
  intern/guardedalloc intern/libc_compat intern/sky
git -C "$source" fetch --filter=blob:none --depth=1 origin "$commit"
git -C "$source" checkout --detach "$commit"
actual=$(git -C "$source" rev-parse HEAD)
[[ "$actual" == "$commit" ]] || { echo "Blender source pin mismatch: $actual" >&2; exit 2; }
printf 'Blender v5.2.2 source pinned at %s (%s)\n' "$actual" "$source"
