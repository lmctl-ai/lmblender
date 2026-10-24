#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${LMCODE_CPP:-$HERE/../lmcode-cpp}"
LOCK="$HERE/sdk.lock"
HEADER="$HERE/cpp/dslvm.hpp"
hash() { sha256sum "$1" | cut -c1-64; }
if [ "${1:-}" = "--verify" ]; then
  want="$(sed -n 's/^sha256=//p' "$LOCK")"
  have="$(hash "$HEADER")"
  [ "$want" = "$have" ] || { echo "header hash $have differs from sdk.lock $want" >&2; exit 1; }
  echo "cpp/dslvm.hpp matches sdk.lock ($(sed -n 's/^commit=//p' "$LOCK"))"
  exit 0
fi
[ -f "$SDK/include/dslvm.hpp" ] || { echo "SDK checkout missing at $SDK (set LMCODE_CPP)" >&2; exit 1; }
ref="${LMCODE_CPP_REF:-origin/main}"
commit="$(git -C "$SDK" rev-parse "$ref")"
git -C "$SDK" show "$commit:include/dslvm.hpp" > "$HEADER"
printf 'repo=https://github.com/lmctl-ai/lmcode-cpp\ncommit=%s\nsha256=%s\n' "$commit" "$(hash "$HEADER")" > "$LOCK"
echo "synced cpp/dslvm.hpp from lmcode-cpp $commit"
