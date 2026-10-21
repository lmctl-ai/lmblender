#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source_tree=${BLENDER_SOURCE_TREE:-$repo/build/blender-src-upstream-v5.2.2}
env_build=${BLENDER_DEPS_ENV_BUILD:-$repo/build/blender-deps-environment}
deps_install=${BLENDER_DEPS_INSTALL:-$repo/build/blender-deps-harvest}
libdir=${BLENDER_LIBDIR:-$deps_install}
ispc_root=${LMBLENDER_ISPC_ROOT:-$repo/build/ispc-toolchain}
jobs=${BUILD_JOBS:-2}
cc=${LMBLENDER_CC:-/usr/bin/gcc-14}
cxx=${LMBLENDER_CXX:-/usr/bin/g++-14}

test -f "$source_tree/build_files/build_environment/CMakeLists.txt" || {
  echo "pinned Blender build_environment is missing; run scripts/fetch-cycles-source.sh" >&2; exit 2;
}
mkdir -p "$deps_install" "$libdir" "$ispc_root"
# Configure downloads every pinned package source. Blender's lib-source
# mirror (hash-checked) is used by default because some upstream hosts, such
# as gmplib.org, are unreachable from CI runners.
cmake -S "$source_tree/build_files/build_environment" -B "$env_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
  -DHARVEST_TARGET="$deps_install" -DLIBDIR="$libdir" \
  -DPACKAGE_USE_UPSTREAM_SOURCES="${BLENDER_PACKAGE_USE_UPSTREAM_SOURCES:-OFF}"

# Building these pinned Blender ExternalProject targets populates the exact
# source layout consumed by scripts/build-deps.sh and harvests the small set
# of non-OpenEXR static dependencies. Their build products are cached in CI.
targets=(external_zlib external_zstd external_png external_jpeg
  external_openjpeg external_openjph external_brotli external_pugixml external_fmt
  external_tbb external_imath external_openexr external_tiff external_opencolorio
  external_expat external_pystring external_yamlcpp external_minizipng external_openimageio
  external_epoxy)
# sse2neon only exists (and is only needed) on arm64 hosts.
case "$(uname -m)" in aarch64|arm64) targets=(external_sse2neon "${targets[@]}") ;; esac
cmake --build "$env_build" --parallel "$jobs" --target "${targets[@]}"

# build_environment always installs into its own <build>/Release LIBDIR and
# only fills HARVEST_TARGET from the full `harvest` target. Copy just the
# prefixes consumed by build-deps.sh, configure-cycles-static.sh, and the host.
release_libdir="$env_build/Release"
for name in zlib zstd png jpeg openjpeg openjph brotli pugixml fmt tbb imath expat epoxy sse2neon; do
  if [[ -d "$release_libdir/$name" ]]; then
    rm -rf "${deps_install:?}/$name"
    cp -a "$release_libdir/$name" "$deps_install/$name"
  fi
done
# Match Blender's Linux harvest: png and brotli ship only static archives so
# their shared objects can never win a link. openjph, tbb, and imath keep their
# shared objects because their exported CMake targets reference them; the host
# links static archives from the static prefix (checked by ldd in CI).
for name in png brotli; do
  find "$deps_install/$name" \( -name '*.so' -o -name '*.so.*' \) -delete
done

source_root="$env_build/build"
mkdir -p "$source_root/openimagedenoise/src/external_openimagedenoise"
oidn_archive="$repo/build/oidn-2.5.0.src.tar.gz"
curl --fail --location --retry 3 \
  https://github.com/RenderKit/oidn/releases/download/v2.5.0/oidn-2.5.0.src.tar.gz \
  --output "$oidn_archive"
echo 'ae984ce4cc4c81ec152ab81b241a8738  '"$oidn_archive" | md5sum --check
tar -xzf "$oidn_archive" --strip-components=1 \
  -C "$source_root/openimagedenoise/src/external_openimagedenoise"

case "$(uname -m)" in
  aarch64|arm64)
    ispc_archive=ispc-v1.30.0-linux.aarch64.tar.gz
    ispc_sha=509399c399ec162d746889458a10cc13797a1aed1c0164b2bd3faddf7d023f13
    ;;
  x86_64|amd64)
    ispc_archive=ispc-v1.30.0-linux.tar.gz
    ispc_sha=63e7d61037849fa1ed644f0398d21740ee9f880b9bf81f017c65eebe1d42c02b
    ;;
  *) echo "unsupported ISPC host architecture: $(uname -m)" >&2; exit 2 ;;
esac
ispc_tar="$repo/build/$ispc_archive"
curl --fail --location --retry 3 \
  "https://github.com/ispc/ispc/releases/download/v1.30.0/$ispc_archive" \
  --output "$ispc_tar"
echo "$ispc_sha  $ispc_tar" | sha256sum --check
tar -xzf "$ispc_tar" -C "$ispc_root"
ispc=$(find "$ispc_root" -type f -name ispc -perm -u+x -print -quit)
test -n "$ispc" || { echo 'ISPC executable not found in pinned archive' >&2; exit 2; }
printf '%s\n' "$ispc" >"$repo/build/ispc-executable.txt"
printf 'Blender dependency sources: %s\nHarvested support libs: %s\nISPC: %s\n' \
  "$source_root" "$deps_install" "$ispc"
