#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
src_root=${BLENDER_DEPS_SOURCE_ROOT:-${BLENDER_DEPS_ENV_BUILD:-$repo/build/blender-deps-environment}/build}
deps_install=${BLENDER_DEPS_INSTALL:-$repo/build/blender-deps-harvest}
prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix}
build_root=${LMBLENDER_DEPS_BUILD_DIR:-$repo/build/deps}
jobs=${BUILD_JOBS:-2}
cmake=${CMAKE:-cmake}
case "$(uname -s)" in
  Darwin) default_cc=clang default_cxx=clang++ ;;
  *) default_cc=/usr/bin/gcc-14 default_cxx=/usr/bin/g++-14 ;;
esac
c_compiler=${LMBLENDER_CC:-$default_cc}
cxx_compiler=${LMBLENDER_CXX:-$default_cxx}
abspath() { python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "$1"; }
prefix=$(abspath "$prefix")
build_root=$(abspath "$build_root")
compiler_id="$($cxx_compiler --version | head -n 1)"
state_file="$build_root/.compiler-id"
if [ -f "$state_file" ] && [ "$(cat "$state_file")" != "$compiler_id" ]; then
  rm -rf "$build_root" "$prefix"
fi
mkdir -p "$build_root" "$prefix"
printf '%s\n' "$compiler_id" > "$state_file"

# Blender's dependency bootstrap intentionally does not build its shared
# Embree target (which would also pull in DPC++ on x64). Extract the pinned
# package here and build our own CPU-only static variant below.
embree_source="$src_root/embree/src/external_embree"
if [[ ! -f "$embree_source/CMakeLists.txt" ]]; then
  embree_archive="${BLENDER_DEPS_ENV_BUILD:-$repo/build/blender-deps-environment}/packages/embree-v4.4.1.zip"
  if [[ ! -f "$embree_archive" ]]; then
    mkdir -p "$(dirname "$embree_archive")"
    curl --fail --location --retry 3 \
      https://projects.blender.org/blender/lib-source/media/branch/blender-v5.2-release/embree-v4.4.1.zip \
      --output "$embree_archive" ||
      curl --fail --location --retry 3 \
        https://github.com/RenderKit/embree/archive/v4.4.1.zip --output "$embree_archive"
  fi
  if command -v sha256sum >/dev/null; then
    actual_embree_hash=$(sha256sum "$embree_archive" | cut -d' ' -f1)
  else
    actual_embree_hash=$(shasum -a 256 "$embree_archive" | cut -d' ' -f1)
  fi
  [[ "$actual_embree_hash" == 643b87490b207475872b0721019ebe3947feb9104f36c25795455516e4e85b82 ]] || {
    echo "Embree archive SHA-256 mismatch: $embree_archive" >&2; exit 1;
  }
  mkdir -p "$(dirname "$embree_source")"
  python3 - "$embree_archive" "$(dirname "$embree_source")" <<'PY'
import pathlib, sys, zipfile
archive, parent = sys.argv[1:]
with zipfile.ZipFile(archive) as zipped:
    zipped.extractall(parent)
pathlib.Path(parent, 'embree-4.4.1').rename(pathlib.Path(parent, 'external_embree'))
PY
fi

for name in tbb imath openexr openimageio tiff embree; do
  test -f "$src_root/$name/src/external_$name/CMakeLists.txt" || {
    echo "missing Blender dependency source: $src_root/$name/src/external_$name" >&2; exit 1;
  }
done
mkdir -p "$prefix" "$build_root"

build_one() {
  local name=$1 source=$2 build=$3; shift 3
  "$cmake" -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$c_compiler" -DCMAKE_CXX_COMPILER="$cxx_compiler" \
    -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF "$@"
  "$cmake" --build "$build" --parallel "$jobs"
  "$cmake" --install "$build"
}

# The malloc proxy replaces global allocation operators and is not required by Cycles.
build_one tbb "$src_root/tbb/src/external_tbb" "$build_root/tbb" \
  -DTBB_TEST=OFF -DTBB_EXAMPLES=OFF -DTBBMALLOC_BUILD=OFF \
  -DTBBMALLOC_PROXY_BUILD=OFF -DTBB4PY_BUILD=OFF -DTBB_ENABLE_IPO=OFF

# Embree's traversal kernels use the native CPU ISA; ISPC support is only
# needed by its example applications. Keep all CPU geometry types for Cycles.
embree_isa_args=()
case "$(uname -s)-$(uname -m)" in
  *-aarch64|*-arm64) embree_isa_args=(-DEMBREE_MAX_ISA=NEON) ;;
  # AppleClang cannot build a static Embree with several ISAs; every Intel Mac
  # supported by macOS 13 has SSE4.2, so build exactly that one.
  Darwin-x86_64) embree_isa_args=(-DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_SSE2=OFF -DEMBREE_ISA_SSE42=ON
    -DEMBREE_ISA_AVX=OFF -DEMBREE_ISA_AVX2=OFF -DEMBREE_ISA_AVX512=OFF) ;;
  *-x86_64|*-amd64) embree_isa_args=(-DEMBREE_MAX_ISA=AVX2) ;;
  *) echo "unsupported Embree CPU architecture: $(uname -m)" >&2; exit 1 ;;
esac
build_one embree "$src_root/embree/src/external_embree" "$build_root/embree" \
  -DEMBREE_STATIC_LIB=ON -DEMBREE_TUTORIALS=OFF -DEMBREE_ISPC_SUPPORT=OFF \
  -DEMBREE_SYCL_SUPPORT=OFF -DEMBREE_TASKING_SYSTEM=TBB \
  "${embree_isa_args[@]}" -DCMAKE_PREFIX_PATH="$prefix"

build_one imath "$src_root/imath/src/external_imath" "$build_root/imath" \
  -DBUILD_TESTING=OFF -DPYTHON=OFF -DPYBIND11=OFF

build_one openexr "$src_root/openexr/src/external_openexr" "$build_root/openexr" \
  -DBUILD_TESTING=OFF -DOPENEXR_BUILD_TESTS=OFF -DOPENEXR_BUILD_EXAMPLES=OFF \
  -DOPENEXR_BUILD_TOOLS=OFF -DOPENEXR_BUILD_UTILS=OFF -DOPENEXR_BUILD_PYTHON=OFF \
  -DOPENEXR_USE_TBB=OFF -DCMAKE_PREFIX_PATH="$prefix;$deps_install/imath;$deps_install/zlib"

# OpenEXR 3.4's generated config leaves optional dependency expressions empty
# when those optional packages are disabled. CMake 3.28 rejects `if(0 AND )`.
openexr_config="$prefix/lib/cmake/OpenEXR/OpenEXRConfig.cmake"
if [ -f "$openexr_config" ]; then
  sed -i -e 's/^if (0 AND )$/if (FALSE)/' -e 's/^if ()$/if (FALSE)/' "$openexr_config"
fi

build_one tiff "$src_root/tiff/src/external_tiff" "$build_root/tiff" \
  -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF -Dtiff-docs=OFF \
  -Dtiff-static=ON -Dtiff-deprecated=OFF -Dtiff-install=ON \
  -Djbig=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF -Dlerc=OFF \
  -DCMAKE_PREFIX_PATH="$deps_install/jpeg;$deps_install/zlib"

oiio_prefixes="$prefix;$deps_install/zlib;$deps_install/png;$deps_install/jpeg;$deps_install/openjpeg;$deps_install/openjph;$deps_install/fmt;$deps_install/brotli"
build_one opencolorio "$src_root/opencolorio/src/external_opencolorio" "$build_root/opencolorio" \
  -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF \
  -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_OPENFX=OFF -DOCIO_BUILD_NUKE=OFF \
  -DOCIO_BUILD_DOCS=OFF -DOCIO_INSTALL_EXT_PACKAGES=ALL \
  -DCMAKE_PREFIX_PATH="$prefix;$deps_install/expat;$deps_install/zlib"

# OCIO's static package config requires its private static dependency targets
# to exist in the consuming project. OIIO consumes the build-tree dependency
# bundle directly, so define those imported targets before its project config.
ocio_ext="$build_root/opencolorio/ext/dist"
ocio_targets="$build_root/oiio-static-targets.cmake"
cat > "$ocio_targets" <<EOF
foreach(_spec IN ITEMS
    "expat::expat|$ocio_ext/lib/libexpat.a|$ocio_ext/include"
    "pystring::pystring|$ocio_ext/lib/libpystring.a|$ocio_ext/include/pystring"
    "yaml-cpp::yaml-cpp|$ocio_ext/lib/libyaml-cpp.a|$ocio_ext/include"
    "MINIZIP::minizip-ng|$ocio_ext/lib/libminizip-ng.a|$ocio_ext/include")
  string(REPLACE "|" ";" _parts "\${_spec}")
  list(GET _parts 0 _target)
  list(GET _parts 1 _library)
  list(GET _parts 2 _include)
  if(NOT TARGET "\${_target}")
    add_library("\${_target}" UNKNOWN IMPORTED GLOBAL)
    set_target_properties("\${_target}" PROPERTIES IMPORTED_LOCATION "\${_library}" INTERFACE_INCLUDE_DIRECTORIES "\${_include}")
  endif()
endforeach()
EOF

build_one openimageio "$src_root/openimageio/src/external_openimageio" "$build_root/openimageio" \
  -DOIIO_BUILD_TESTS=OFF -DOIIO_BUILD_TOOLS=OFF -DOIIO_BUILD_DOCS=OFF \
  -DUSE_PYTHON=OFF -DUSE_TBB=ON -DUSE_OPENVDB=OFF -DUSE_FFMPEG=OFF \
  -DUSE_FREETYPE=OFF -DUSE_GIF=OFF -DUSE_LIBRAW=OFF -DUSE_WEBP=OFF \
  -DUSE_OPENJPEG=OFF -DUSE_OPENCV=OFF -DCMAKE_PREFIX_PATH="$oiio_prefixes" \
  -Dexpat_ROOT="$build_root/opencolorio/ext/dist" \
  -Dpystring_ROOT="$build_root/opencolorio/ext/dist" \
  -Dyaml-cpp_ROOT="$build_root/opencolorio/ext/dist" \
  -Dminizip-ng_ROOT="$build_root/opencolorio/ext/dist" \
  -Dexpat_STATIC_LIBRARY=ON \
  -Dexpat_LIBRARY="$build_root/opencolorio/ext/dist/lib/libexpat.a" \
  -Dexpat_INCLUDE_DIR="$build_root/opencolorio/ext/dist/include" \
  -Dpystring_LIBRARY="$build_root/opencolorio/ext/dist/lib/libpystring.a" \
  -Dpystring_INCLUDE_DIR="$build_root/opencolorio/ext/dist/include/pystring" \
  -Dyaml-cpp_DIR="$build_root/opencolorio/ext/dist/lib/cmake/yaml-cpp" \
  -Dminizip-ng_DIR="$build_root/opencolorio/ext/dist/lib/cmake/minizip-ng" \
  -DCMAKE_PROJECT_INCLUDE="$ocio_targets"

echo "Static dependency prefix: $prefix"
find "$prefix" -type f -name '*.a' -print | sort
