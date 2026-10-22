#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
#
# Build static Cycles libraries without Blender's top-level CMake, whose Windows
# and macOS platform files require Blender's precompiled lib/<platform> trees.
# Uses the standalone Cycles build system (github.com/blender/cycles, pinned
# commit below) with the Cycles *sources* from our pinned Blender checkout plus
# patches/cycles-*.patch, and the static prefix from build-deps-portable.sh.
#
# Environment:
#   BLENDER_SOURCE_TREE         pinned Blender checkout
#   LMBLENDER_PREFIX            static dependency prefix (build-deps-portable.sh)
#   LMBLENDER_CYCLES_BUILD_DIR  output build dir (default: build/cycles-static)
#   BUILD_JOBS
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source_tree=${BLENDER_SOURCE_TREE:-$repo/build/blender-src-upstream-v5.2.2}
prefix=${LMBLENDER_PREFIX:-$repo/build/portable-deps/prefix}
build=${LMBLENDER_CYCLES_BUILD_DIR:-$repo/build/cycles-static}
jobs=${BUILD_JOBS:-2}
cycles_repo=https://github.com/blender/cycles.git
cycles_commit=3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba  # tag v5.2.0 (build system only)
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }

work="$build-source"
standalone="$work/cycles-buildsystem"
overlay="$work/overlay"
if [[ ! -d "$standalone/.git" ]]; then
  rm -rf "$standalone"; mkdir -p "$standalone"
  git -C "$standalone" init -q
  git -C "$standalone" remote add origin "$cycles_repo"
fi
git -C "$standalone" fetch -q --depth 1 origin "$cycles_commit"
git -C "$standalone" checkout -q --detach FETCH_HEAD
[[ "$(git -C "$standalone" rev-parse HEAD)" == "$cycles_commit" ]]

# Overlay: standalone CMakeLists.txt, src/CMakeLists.txt, src/cmake, third_party;
# every other src/<dir> comes from Blender's intern/cycles (patched).
rm -rf "$overlay"; mkdir -p "$overlay/src" "$work/patched/intern"
cp -R "$standalone/CMakeLists.txt" "$standalone/third_party" "$overlay/"
cp -R "$standalone/src/CMakeLists.txt" "$standalone/src/cmake" "$overlay/src/"
# Reuse the Linux overlay script so both build paths get identical patched
# Cycles sources (patches/cycles-*.patch plus its scripted source edits). Give
# it a minimal source root holding only intern/cycles.
rm -rf "$work/patch-root" "$work/patched"; mkdir -p "$work/patch-root/intern"
cp -R "$source_tree/intern/cycles" "$work/patch-root/intern/cycles"
py=python3; command -v python3 >/dev/null 2>&1 || py=python
"$py" "$repo/scripts/make-cycles-source-overlay.py" "$(native "$work/patch-root")" "$(native "$work/patched")"
# Backport from the standalone Cycles repository: MSVC Release builds have no
# std::_Container_proxy. Harmless elsewhere (the block is MSVC-only).
patch --batch -p1 -d "$work/patched" -i "$repo/patches/cycles-msvc-guarded-allocator.patch"
for d in "$work/patched/intern/cycles"/*; do
  name=$(basename "$d")
  case "$name" in cmake|CMakeLists.txt) continue ;; esac
  cp -R "$d" "$overlay/src/$name"
done
cat >>"$overlay/src/cmake/dependency_targets.cmake" <<'EOF'

# lmblender: Blender v5.2.2's intern/cycles links bf::dependencies::fmt.
if(NOT TARGET bf::dependencies::fmt)
  find_package(fmt CONFIG REQUIRED)
  add_library(bf::dependencies::fmt ALIAS fmt::fmt)
endif()
EOF

# The standalone build system hard-codes /MD in configuration flags. Remove
# those flags so CMP0091 and CMAKE_MSVC_RUNTIME_LIBRARY control the CRT.
"$py" - "$(native "$overlay/src/cmake/configure_build.cmake")" "$(native "$overlay/src/cmake/external_libs.cmake")" <<'PYCRT'
from pathlib import Path
import re, sys
path = Path(sys.argv[1])
text = re.sub(r" /MDd?\b", "", path.read_text())
text += "\nif(MSVC)\n  add_compile_definitions(__TBB_NO_IMPLICIT_LINKAGE=1)\nendif()\n"
path.write_text(text)
# The legacy finder assumes all x64 ISA archives exist. Embree's own exported
# target records exactly the built static components and their TBB dependency.
path = Path(sys.argv[2])
text = path.read_text()
needle = "find_package(Embree 3.8.0 REQUIRED)"
assert text.count(needle) == 1
text = text.replace(needle, """find_package(TBB CONFIG REQUIRED)
    find_package(embree 4 CONFIG REQUIRED)
    set(EMBREE_MAJOR_VERSION ${EMBREE_VERSION_MAJOR})
    set(EMBREE_LIBRARIES embree)""")
path.write_text(text)
PYCRT

P=$(native "$prefix")
extra=()
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    # FindZstd only searches zstd/libzstd, but MSVC installs zstd_static.lib.
    extra+=(-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
      -DZSTD_LIBRARY="$P/lib/zstd_static.lib")
    ;;
esac
case "$(uname -m)" in
  aarch64|arm64) [[ -n "${SSE2NEON_ROOT_DIR:-}" ]] && extra+=(-DSSE2NEON_ROOT_DIR="$(native "$SSE2NEON_ROOT_DIR")") ;;
esac
if [[ -n "${LMBLENDER_CC:-}" ]]; then extra+=(-DCMAKE_C_COMPILER="$LMBLENDER_CC" -DCMAKE_CXX_COMPILER="$LMBLENDER_CXX"); fi
cmake -S "$(native "$overlay")" -B "$(native "$build")" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$P" -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DWITH_LIBS_PRECOMPILED=OFF -DWITH_CYCLES_OSL=OFF -DWITH_CYCLES_EMBREE=ON \
  -DWITH_CYCLES_PATH_GUIDING=OFF -DWITH_CYCLES_OPENVDB=OFF -DWITH_CYCLES_NANOVDB=OFF \
  -DWITH_CYCLES_OPENSUBDIV=OFF -DWITH_CYCLES_ALEMBIC=OFF -DWITH_CYCLES_USD=OFF \
  -DWITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF -DWITH_CYCLES_STANDALONE_GUI=OFF \
  -DWITH_CYCLES_DEVICE_CUDA=OFF -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_DEVICE_HIP=OFF \
  -DWITH_CYCLES_DEVICE_METAL=OFF -DWITH_CYCLES_DEVICE_ONEAPI=OFF \
  -DWITH_CYCLES_OPENIMAGEDENOISE=ON -DWITH_CYCLES_PUGIXML=ON \
  -DOPENIMAGEDENOISE_ROOT_DIR="$P" -DOPENIMAGEIO_ROOT_DIR="$P" -DOPENCOLORIO_ROOT_DIR="$P" \
  -DOPENEXR_ROOT_DIR="$P" -DIMATH_ROOT_DIR="$P" -DTBB_ROOT_DIR="$P" -DPUGIXML_ROOT_DIR="$P" \
  -DEMBREE_ROOT_DIR="$P" -DJPEG_ROOT="$P" -DZLIB_ROOT="$P" -Dfmt_ROOT="$P" \
  -DCMAKE_PROJECT_INCLUDE="$P/lib/cmake/lmblender-static-targets.cmake" \
  "${extra[@]}"
cmake --build "$(native "$build")" --parallel "$jobs"
printf "standalone Cycles build system %s\n" "$cycles_commit" >"$build/lmblender-portable-cycles.txt"
echo "Static Cycles libraries:"
find "$build" -type f \( -name 'libcycles_*.a' -o -name 'cycles_*.lib' \) | sort
