#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source=${OIDN_SOURCE:-${BLENDER_DEPS_ENV_BUILD:-$repo/build/blender-deps-environment}/build/openimagedenoise/src/external_openimagedenoise}
ispc=${ISPC_EXECUTABLE:-$(cat "$repo/build/ispc-executable.txt" 2>/dev/null || echo ispc)}
static_prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix-gcc14}
build=${OIDN_BUILD_DIR:-$repo/build/oidn-static-build}
prefix=${OIDN_INSTALL_PREFIX:-$repo/build/oidn-static-install}
smoke_build=${OIDN_SMOKE_BUILD_DIR:-$repo/build/oidn-smoke-build}
jobs=${BUILD_JOBS:-2}
test -f "$source/CMakeLists.txt"
test -x "$ispc"
test -f "$static_prefix/lib/libtbb.a"
cmake -S "$source" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DOIDN_STATIC_LIB=ON -DOIDN_DEVICE_CPU=ON -DOIDN_DEVICE_CUDA=OFF \
  -DOIDN_DEVICE_HIP=OFF -DOIDN_DEVICE_SYCL=OFF -DOIDN_APPS=OFF -DOIDN_FILTER_RT=ON \
  -DISPC_EXECUTABLE="$ispc" -DTBB_ROOT="$static_prefix" -DCMAKE_PREFIX_PATH="$static_prefix"
cmake --build "$build" --parallel "$jobs"
cmake --install "$build"
# Older experiments used a different source directory for this build tree.
# CMake refuses to reuse that cache, so reset only this dedicated smoke dir.
if [[ -f "$smoke_build/CMakeCache.txt" ]] && \
   ! grep -Fq "CMAKE_HOME_DIRECTORY:INTERNAL=$repo/tests/oidn-static-smoke" "$smoke_build/CMakeCache.txt"; then
  rm -rf -- "$smoke_build"
fi
cmake -S "$repo/tests/oidn-static-smoke" -B "$smoke_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix;$static_prefix"
cmake --build "$smoke_build" --parallel "$jobs"
"$smoke_build/oidn_static_smoke"
"$repo/scripts/check-static.sh" "$smoke_build/oidn_static_smoke"
echo "OIDN static smoke binary: $(wc -c < "$smoke_build/oidn_static_smoke" | tr -d ' ') bytes"
