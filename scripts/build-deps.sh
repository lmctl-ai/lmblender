#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
src_root=${BLENDER_DEPS_SOURCE_ROOT:-/home/ubuntu/blender-deps/build/build}
deps_install=${BLENDER_DEPS_INSTALL:-/home/ubuntu/blender-deps/install}
prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix}
build_root=${LMBLENDER_DEPS_BUILD_DIR:-$repo/build/deps}
jobs=${BUILD_JOBS:-2}
cmake=${CMAKE:-cmake}

for name in tbb imath openexr openimageio tiff; do
  test -f "$src_root/$name/src/external_$name/CMakeLists.txt" || {
    echo "missing Blender dependency source: $src_root/$name/src/external_$name" >&2; exit 1;
  }
done
mkdir -p "$prefix" "$build_root"

build_one() {
  local name=$1 source=$2 build=$3; shift 3
  "$cmake" -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF "$@"
  "$cmake" --build "$build" --parallel "$jobs"
  "$cmake" --install "$build"
}

# The malloc proxy replaces global allocation operators and is not required by Cycles.
build_one tbb "$src_root/tbb/src/external_tbb" "$build_root/tbb" \
  -DTBB_TEST=OFF -DTBB_EXAMPLES=OFF -DTBBMALLOC_BUILD=OFF \
  -DTBBMALLOC_PROXY_BUILD=OFF -DTBB4PY_BUILD=OFF -DTBB_ENABLE_IPO=OFF

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

build_one openimageio "$src_root/openimageio/src/external_openimageio" "$build_root/openimageio" \
  -DOIIO_BUILD_TESTS=OFF -DOIIO_BUILD_TOOLS=OFF -DOIIO_BUILD_DOCS=OFF \
  -DUSE_PYTHON=OFF -DUSE_TBB=ON -DUSE_OPENVDB=OFF -DUSE_FFMPEG=OFF \
  -DUSE_FREETYPE=OFF -DUSE_GIF=OFF -DUSE_LIBRAW=OFF -DUSE_WEBP=OFF \
  -DUSE_OPENJPEG=OFF -DUSE_OPENCV=OFF -DCMAKE_PREFIX_PATH="$oiio_prefixes"

echo "Static dependency prefix: $prefix"
find "$prefix" -type f -name '*.a' -print | sort
