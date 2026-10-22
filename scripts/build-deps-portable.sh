#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
#
# Build the static dependency prefix with plain CMake from the pinned
# build_environment source archives (scripts/fetch-dep-sources.py), without
# configuring Blender's build_environment. Used for Windows (MSVC, static CRT
# /MT); also runs on Linux/macOS for local verification.
#
# Environment:
#   BLENDER_SOURCE_TREE   pinned Blender checkout (versions.cmake)
#   LMBLENDER_DEPS_ROOT   work root (default: build/portable-deps)
#   LMBLENDER_PREFIX      install prefix (default: $LMBLENDER_DEPS_ROOT/prefix)
#   BUILD_JOBS            parallel jobs
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source_tree=${BLENDER_SOURCE_TREE:-$repo/build/blender-src-upstream-v5.2.2}
root=${LMBLENDER_DEPS_ROOT:-$repo/build/portable-deps}
prefix=${LMBLENDER_PREFIX:-$root/prefix}
jobs=${BUILD_JOBS:-2}
src="$root/src"
bld="$root/build"
mkdir -p "$src" "$bld" "$prefix"

# CMake on Windows needs mixed (C:/...) paths, not MSYS (/c/...) paths.
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }
P=$(native "$prefix")
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) windows=1 ;;
  *) windows=0 ;;
esac
py=python3; command -v python3 >/dev/null 2>&1 || py=python

"$py" "$repo/scripts/fetch-dep-sources.py" "$(native "$source_tree")" "$(native "$src")" \
  ZLIB PNG JPEG ZSTD FMT PUGIXML ROBINMAP TBB EMBREE IMATH OPENEXR TIFF EXPAT YAMLCPP PYSTRING MINIZIPNG \
  OPENCOLORIO OPENIMAGEIO OIDN

common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH="$P"
  -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded)
if [[ -n "${LMBLENDER_CC:-}" ]]; then common+=(-DCMAKE_C_COMPILER="$LMBLENDER_CC" -DCMAKE_CXX_COMPILER="$LMBLENDER_CXX"); fi

build_one() {
  local name=$1 source=$2; shift 2
  local stamp="$bld/$name.done"
  if [[ -f "$stamp" ]]; then echo "== $name (cached)"; return; fi
  echo "== $name"
  cmake -S "$(native "$source")" -B "$(native "$bld/$name")" "${common[@]}" "$@"
  cmake --build "$(native "$bld/$name")" --parallel "$jobs"
  cmake --install "$(native "$bld/$name")"
  touch "$stamp"
}

if (( windows )); then
  lib() { printf '%s/lib/%s.lib' "$P" "$1"; }
else
  lib() { printf '%s/lib/lib%s.a' "$P" "$1"; }
fi

build_one zlib "$src/zlib" -DZLIB_BUILD_EXAMPLES=OFF
# zlib always builds a shared library too; keep consumers on the static one.
if (( windows )); then
  cp "$prefix/lib/zlibstatic.lib" "$prefix/lib/zlib.lib"
  rm -f "$prefix/bin/zlib.dll" "$prefix/bin/zlib1.dll"
  zlib_lib=$(lib zlib)
else
  rm -f "$prefix"/lib/libz.so*
  zlib_lib=$(lib z)
fi
zlib_args=(-DZLIB_LIBRARY="$zlib_lib" -DZLIB_INCLUDE_DIR="$P/include" -DZLIB_ROOT="$P" -DZLIB_USE_STATIC_LIBS=ON)

build_one png "$src/png" "${zlib_args[@]}" -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF
# SIMD needs NASM; the scalar path is enough for texture loading.
# libjpeg-turbo resolves a relative CMAKE_INSTALL_LIBDIR against the working directory.
build_one jpeg "$src/jpeg" -DCMAKE_INSTALL_LIBDIR="$P/lib" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_CRT_DLL=OFF
build_one zstd "$src/zstd/build/cmake" -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_USE_STATIC_RUNTIME=ON
build_one fmt "$src/fmt" -DFMT_TEST=OFF -DFMT_DOC=OFF
build_one pugixml "$src/pugixml" -DPUGIXML_BUILD_TESTS=OFF
build_one robinmap "$src/robinmap"
build_one tbb "$src/tbb" -DTBB_TEST=OFF -DTBB_EXAMPLES=OFF -DTBBMALLOC_BUILD=OFF \
  -DTBBMALLOC_PROXY_BUILD=OFF -DTBB4PY_BUILD=OFF -DTBB_ENABLE_IPO=OFF -DTBB_STRICT=OFF
# Match the CPU BVH used by the shared suite. One x64 ISA keeps hosted MSVC
# cold builds practical; Cycles already requires the SSE4.2 baseline.
embree_isa=(-DEMBREE_MAX_ISA=AVX2)
case "$(uname -s)-$(uname -m)" in
  MINGW*|MSYS*|CYGWIN*|Darwin-x86_64)
    embree_isa=(-DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_SSE2=OFF -DEMBREE_ISA_SSE42=ON
      -DEMBREE_ISA_AVX=OFF -DEMBREE_ISA_AVX2=OFF -DEMBREE_ISA_AVX512=OFF) ;;
  *-arm64|*-aarch64) embree_isa=(-DEMBREE_MAX_ISA=NEON) ;;
esac
build_one embree "$src/embree" -DEMBREE_STATIC_LIB=ON -DEMBREE_TUTORIALS=OFF \
  -DEMBREE_ISPC_SUPPORT=OFF -DEMBREE_SYCL_SUPPORT=OFF -DEMBREE_TASKING_SYSTEM=TBB \
  "${embree_isa[@]}" -DEMBREE_STATIC_RUNTIME=ON
build_one imath "$src/imath" -DBUILD_TESTING=OFF -DPYTHON=OFF -DPYBIND11=OFF
build_one openexr "$src/openexr" "${zlib_args[@]}" -DBUILD_TESTING=OFF -DOPENEXR_BUILD_TOOLS=OFF \
  -DOPENEXR_BUILD_EXAMPLES=OFF -DOPENEXR_BUILD_UTILS=OFF -DOPENEXR_BUILD_PYTHON=OFF \
  -DOPENEXR_INSTALL_EXAMPLES=OFF -DOPENEXR_USE_TBB=OFF
build_one tiff "$src/tiff" "${zlib_args[@]}" -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF \
  -Dtiff-docs=OFF -Dtiff-deprecated=OFF -Djbig=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF -Dlerc=OFF \
  -Dlibdeflate=OFF -Djpeg=ON
# OpenColorIO's private dependencies, built here so every archive uses the same
# (static on MSVC) runtime; OCIO's own ext/ builds default to /MD.
build_one expat "$src/expat/expat" -DEXPAT_BUILD_DOCS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
  -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_SHARED_LIBS=OFF -DEXPAT_MSVC_STATIC_CRT=ON
build_one yamlcpp "$src/yamlcpp" -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF \
  -DYAML_CPP_BUILD_CONTRIB=OFF -DYAML_MSVC_SHARED_RT=OFF -DYAML_BUILD_SHARED_LIBS=OFF
cp "$source_tree/build_files/build_environment/patches/cmakelists_pystring.txt" "$src/pystring/CMakeLists.txt"
# The pinned wrapper puts project() before cmake_minimum_required(), which
# initializes MSVC's runtime before CMP0091 can select the static runtime.
"$py" - "$(native "$src/pystring/CMakeLists.txt")" <<'PYPOLICY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text().replace("project(pystring)", "").replace("cmake_minimum_required(VERSION 3.10)",
    "cmake_minimum_required(VERSION 3.15)\nproject(pystring)")
path.write_text(text)
PYPOLICY
build_one pystring "$src/pystring"
build_one minizipng "$src/minizipng" "${zlib_args[@]}" -DMZ_FETCH_LIBS=OFF -DMZ_LIBCOMP=OFF \
  -DMZ_PKCRYPT=OFF -DMZ_WZAES=OFF -DMZ_OPENSSL=OFF -DMZ_SIGNING=OFF -DMZ_LZMA=OFF -DMZ_ZSTD=OFF \
  -DMZ_BZIP2=OFF -DMZ_ICONV=OFF -DMZ_ZLIB=ON -DMZ_COMPAT=OFF -DMZ_BUILD_TESTS=OFF
build_one opencolorio "$src/opencolorio" "${zlib_args[@]}" -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF \
  -DOCIO_BUILD_GPU_TESTS=OFF -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_OPENFX=OFF -DOCIO_BUILD_DOCS=OFF \
  -DOCIO_INSTALL_EXT_PACKAGES=NONE -DImath_ROOT="$P" -DImath_DIR="$P/lib/cmake/Imath"  \
  -Dpystring_ROOT="$P" -Dpystring_INCLUDE_DIR="$P/include" \
  -Dpystring_LIBRARY="$(lib pystring)" -DOCIO_USE_SIMD=ON
# OCIO's static package config needs its private dependencies (expat, pystring,
# yaml-cpp, minizip-ng) as targets in every consuming project. Make the prefix
# self-contained: load the installed CMake packages (including static compile
# definitions), plus the two dependencies without complete package configs.
find_archive() {
  local pattern=$1 f
  f=$(find "$prefix/lib" -maxdepth 1 -type f \( -name "*${pattern}*.a" -o -name "*${pattern}*.lib" \) 2>/dev/null | head -1)
  [[ -n "$f" ]] || { echo "missing static archive for $pattern" >&2; exit 1; }
  native "$f"
}
# OpenEXR vendors OpenJPH and references an `openjph` target it doesn't export.
ojph_lib=$(find "$bld/openexr/external/OpenJPH" -type f \( -name 'libopenjph.a' -o -name 'openjph*.lib' \) | head -1)
[[ -n "$ojph_lib" ]] || { echo "OpenEXR's vendored OpenJPH archive not found" >&2; exit 1; }
cp "$ojph_lib" "$prefix/lib/"
rm -rf "$prefix/include/openjph"
cp -R "$src/openexr/external/OpenJPH/src/core/openjph" "$prefix/include/openjph"
ojph_installed=$(native "$prefix/lib/$(basename "$ojph_lib")")
static_targets="$prefix/lib/cmake/lmblender-static-targets.cmake"
mkdir -p "$(dirname "$static_targets")"
cat > "$static_targets" <<EOF2
find_package(expat CONFIG REQUIRED)
if(NOT TARGET expat::expat AND TARGET expat::libexpat)
  add_library(expat::expat ALIAS expat::libexpat)
endif()
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)
foreach(_spec IN ITEMS
    "pystring::pystring|$(find_archive pystring)|$P/include"
    "openjph|$ojph_installed|$P/include")
  string(REPLACE "|" ";" _parts "\${_spec}")
  list(GET _parts 0 _target)
  list(GET _parts 1 _library)
  list(GET _parts 2 _include)
  if(NOT TARGET "\${_target}")
    add_library("\${_target}" UNKNOWN IMPORTED GLOBAL)
    set_target_properties("\${_target}" PROPERTIES IMPORTED_LOCATION "\${_library}" INTERFACE_INCLUDE_DIRECTORIES "\${_include}")
  endif()
endforeach()
set(pystring_ROOT "$P")
set(pystring_INCLUDE_DIR "$P/include")
set(pystring_LIBRARY "$(find_archive pystring)")
set(expat_STATIC_LIBRARY ON)
set(expat_INCLUDE_DIR "$P/include")
set(expat_LIBRARY "$(find_archive expat)")
EOF2
E=$P
build_one openimageio "$src/openimageio" "${zlib_args[@]}" -DOIIO_BUILD_TESTS=OFF -DOIIO_BUILD_TOOLS=OFF \
  -DOIIO_BUILD_DOCS=OFF -DUSE_PYTHON=OFF -DUSE_TBB=ON -DUSE_OPENVDB=OFF -DUSE_FFMPEG=OFF \
  -DUSE_FREETYPE=OFF -DUSE_GIF=OFF -DUSE_LIBRAW=OFF -DUSE_WEBP=OFF -DUSE_OPENJPEG=OFF -DUSE_OPENCV=OFF \
  -DUSE_PTEX=OFF -DUSE_DCMTK=OFF -DUSE_LIBHEIF=OFF -DUSE_JXL=OFF -DUSE_LIBUHDR=OFF -DUSE_QT=OFF \
  -DOpenImageIO_BUILD_MISSING_DEPS=none -DLINKSTATIC=ON -DOIIO_INSTALL_FONTS=OFF \
  -DOpenColorIO_ROOT="$P" -DTBB_ROOT="$P" -Dfmt_ROOT="$P" -DRobinmap_ROOT="$P" \
  -Dpystring_ROOT="$E" \
  -Dexpat_STATIC_LIBRARY=ON -Dexpat_LIBRARY="$(find_archive expat)" -Dexpat_INCLUDE_DIR="$E/include" \
  -Dpystring_LIBRARY="$(find_archive pystring)" -Dpystring_INCLUDE_DIR="$E/include" \
  -DCMAKE_PROJECT_INCLUDE="$(native "$static_targets")"

ispc_dir="$root/ispc"
if [[ ! -x "$ispc_dir/bin/ispc" && ! -x "$ispc_dir/bin/ispc.exe" ]]; then
  case "$(uname -s)-$(uname -m)" in
    MINGW*|MSYS*|CYGWIN*) a=ispc-v1.30.0-windows.zip ;;
    Darwin-*) a=ispc-v1.30.0-macOS.universal.tar.gz ;;
    *-aarch64|*-arm64) a=ispc-v1.30.0-linux.aarch64.tar.gz ;;
    *) a=ispc-v1.30.0-linux.tar.gz ;;
  esac
  curl --fail --location --retry 3 -o "$root/$a" "https://github.com/ispc/ispc/releases/download/v1.30.0/$a"
  rm -rf "$ispc_dir" "$root/ispc-unpack" && mkdir -p "$root/ispc-unpack"
  if [[ $a == *.zip ]]; then unzip -q "$root/$a" -d "$root/ispc-unpack"; else tar -xzf "$root/$a" -C "$root/ispc-unpack"; fi
  mv "$root/ispc-unpack"/* "$ispc_dir"
fi
ispc="$ispc_dir/bin/ispc"; [[ -x "$ispc" ]] || ispc="$ispc_dir/bin/ispc.exe"
[[ -x "$ispc" ]] || { echo "ISPC executable not found under $ispc_dir" >&2; exit 1; }
build_one oidn "$src/oidn" -DOIDN_STATIC_LIB=ON -DOIDN_DEVICE_CPU=ON -DOIDN_DEVICE_CUDA=OFF \
  -DOIDN_DEVICE_HIP=OFF -DOIDN_DEVICE_SYCL=OFF -DOIDN_DEVICE_METAL=OFF -DOIDN_APPS=OFF \
  -DOIDN_FILTER_RT=ON -DOIDN_FILTER_RTLIGHTMAP=OFF -DISPC_EXECUTABLE="$(native "$ispc")" -DTBB_ROOT="$P"

# Cycles' FindOpenImageDenoise searches the legacy OIDN 1.x component names;
# OIDN 2.x ships them as OpenImageDenoise_core / _device_cpu.
if (( windows )); then a=; e=.lib; else a=lib; e=.a; fi
cp "$prefix/lib/${a}OpenImageDenoise_core$e" "$prefix/lib/${a}common$e"
cp "$prefix/lib/${a}OpenImageDenoise_core$e" "$prefix/lib/${a}dnnl_common$e"
cp "$prefix/lib/${a}OpenImageDenoise_device_cpu$e" "$prefix/lib/${a}dnnl_cpu$e"

echo "Portable static dependency prefix: $prefix"
find "$prefix/lib" -maxdepth 1 -type f \( -name '*.a' -o -name '*.lib' \) | sort
