#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source_tree=${BLENDER_SOURCE_TREE:-/home/ubuntu/blender-src}
deps_install=${BLENDER_DEPS_INSTALL:-/home/ubuntu/blender-deps/install}
static_prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix-gcc14}
build=${LMBLENDER_CYCLES_BUILD_DIR:-$repo/build/cycles-static}
cc=${LMBLENDER_CC:-/usr/bin/gcc-14}
cxx=${LMBLENDER_CXX:-/usr/bin/g++-14}
jobs=${BUILD_JOBS:-2}
test -f "$source_tree/CMakeLists.txt"
test -f "$static_prefix/lib/libOpenImageIO.a"
mkdir -p "$build"
source_overlay=${LMBLENDER_SOURCE_OVERLAY:-$build/blender-source-overlay}
python3 "$repo/scripts/make-cycles-source-overlay.py" "$source_tree" "$source_overlay"
deps_build=${LMBLENDER_DEPS_BUILD_DIR:-$repo/build/deps-gcc14}
ocio_ext="$deps_build/opencolorio/ext/dist"
openjph_archive="$deps_build/openexr/external/OpenJPH/src/core/libopenjph.a"
openjph_include="$deps_build/openexr/external/OpenJPH/src/core"
test -f "$openjph_archive"
static_targets="$build/static-dependency-targets.cmake"
cat > "$static_targets" <<EOF
foreach(_spec IN ITEMS
    "expat::expat|$ocio_ext/lib/libexpat.a|$ocio_ext/include"
    "pystring::pystring|$ocio_ext/lib/libpystring.a|$ocio_ext/include/pystring"
    "yaml-cpp::yaml-cpp|$ocio_ext/lib/libyaml-cpp.a|$ocio_ext/include"
    "MINIZIP::minizip-ng|$ocio_ext/lib/libminizip-ng.a|$ocio_ext/include"
    "openjph|$openjph_archive|$openjph_include")
  string(REPLACE "|" ";" _parts "\${_spec}")
  list(GET _parts 0 _target)
  list(GET _parts 1 _library)
  list(GET _parts 2 _include)
  if(NOT TARGET "\${_target}")
    add_library("\${_target}" UNKNOWN IMPORTED GLOBAL)
    set_target_properties("\${_target}" PROPERTIES IMPORTED_LOCATION "\${_library}" INTERFACE_INCLUDE_DIRECTORIES "\${_include}")
  endif()
endforeach()
if(NOT TARGET OpenImageIO::oiiotool)
  add_executable(OpenImageIO::oiiotool IMPORTED GLOBAL)
  set_target_properties(OpenImageIO::oiiotool PROPERTIES IMPORTED_LOCATION "$deps_install/openimageio/bin/oiiotool")
endif()
EOF
prefixes="$static_prefix;$deps_install/tbb;$deps_install/zlib;$deps_install/png;$deps_install/jpeg;$deps_install/pugixml;$deps_install/openjpeg;$deps_install/openjph;$deps_install/boost;$deps_install/fmt;$deps_install/brotli"
cmake -S "$source_overlay" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
  -DCMAKE_PREFIX_PATH="$prefixes" \
  -DImath_DIR="$static_prefix/lib/cmake/Imath" \
  -DOpenEXR_DIR="$static_prefix/lib/cmake/OpenEXR" \
  -DOpenColorIO_DIR="$static_prefix/lib/cmake/OpenColorIO" \
  -DOpenImageIO_DIR="$static_prefix/lib/cmake/OpenImageIO" \
  -DTBB_DIR="$static_prefix/lib/cmake/TBB" \
  -DTIFF_LIBRARY="$static_prefix/lib/libtiff.a" \
  -DTIFF_INCLUDE_DIR="$static_prefix/include" \
  -DZLIB_LIBRARY="$deps_install/zlib/lib/libz.a" \
  -DZLIB_INCLUDE_DIR="$deps_install/zlib/include" \
  -DJPEG_LIBRARY="$deps_install/jpeg/lib/libjpeg.a" \
  -DJPEG_INCLUDE_DIR="$deps_install/jpeg/include" \
  -DPNG_LIBRARY="$deps_install/png/lib/libpng.a" \
  -DPNG_PNG_INCLUDE_DIR="$deps_install/png/include" \
  -DCMAKE_PROJECT_INCLUDE="$static_targets" \
  -DCMAKE_EXE_LINKER_FLAGS='-static -static-libstdc++ -static-libgcc' \
  -DBUILD_SHARED_LIBS=OFF \
  -DWITH_BLENDER=OFF -DWITH_CYCLES=OFF -DWITH_CYCLES_STANDALONE=ON \
  -DWITH_CYCLES_STANDALONE_GUI=OFF -DWITH_PYTHON=OFF \
  -DWITH_CYCLES_OSL=OFF -DWITH_CYCLES_TEST_OSL=OFF -DWITH_LLVM=OFF \
  -DWITH_CYCLES_EMBREE=OFF -DWITH_CYCLES_PATH_GUIDING=OFF \
  -DWITH_OPENVDB=OFF -DWITH_OPENSUBDIV=OFF \
  -DWITH_OPENIMAGEDENOISE=OFF -DWITH_TBB=ON -DWITH_TBB_MALLOC_PROXY=OFF \
  -DWITH_USD=OFF -DWITH_HYDRA=OFF -DWITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF \
  -DWITH_CYCLES_DEVICE_CUDA=OFF -DWITH_CYCLES_DEVICE_HIP=OFF \
  -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_DEVICE_ONEAPI=OFF \
  -DWITH_CYCLES_DEVICE_METAL=OFF -DWITH_CUDA_DYNLOAD=OFF \
  -DWITH_GHOST=OFF -DWITH_X11=OFF -DWITH_OPENGL=OFF \
  -DLIBDIR="${BLENDER_LIBDIR:-/home/ubuntu/lib/linux_aarch64}"
cmake --build "$build" --target cycles --parallel "$jobs"
file "$build/bin/cycles"
ldd "$build/bin/cycles" 2>&1 | tee "$build/ldd.txt"
grep -q 'not a dynamic executable' "$build/ldd.txt"
