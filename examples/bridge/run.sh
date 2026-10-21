#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
cli_repo=${BLENDERCLI_REPO:-/home/ubuntu/blendercli}
lm_repo=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$cli_repo/examples/bridge/out" "$lm_repo/build"
cd "$cli_repo"
PATH="$cli_repo/build/bin:$PATH" lmscript "$cli_repo/examples/bridge/build-and-export.lms"
python3 "$cli_repo/tests/bridge_export_quality.py" "$cli_repo/examples/bridge/out/bridge.glb"
cp "$cli_repo/examples/bridge/out/bridge.glb" "$lm_repo/tests/fixtures/bridge/bridge.glb"
cp "$cli_repo/examples/bridge/out/blendercli-0001.png" "$lm_repo/tests/fixtures/bridge/blendercli-reference.png"
cd "$lm_repo"
PATH="$lm_repo/build:$PATH" lmscript "$lm_repo/examples/bridge/render-export.lms" "$lm_repo/build/bridge-render.dslb"
python3 "$lm_repo/tests/bridge_parity.py" \
  "$cli_repo/examples/bridge/out/blendercli-0001.png" \
  "$lm_repo/build/bridge-from-blendercli.png"
