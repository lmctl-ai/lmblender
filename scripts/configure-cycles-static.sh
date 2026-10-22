#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
source_tree=${BLENDER_SOURCE_TREE:-/home/ubuntu/blender-src}
deps_install=${BLENDER_DEPS_INSTALL:-/home/ubuntu/blender-deps/install}
static_prefix=${LMBLENDER_STATIC_PREFIX:-$repo/build/static-prefix-gcc14}
build=${LMBLENDER_CYCLES_BUILD_DIR:-$repo/build/cycles-static}
case "$(uname -s)" in
  Darwin)
    default_cc=clang default_cxx=clang++
    # macOS cannot link fully static executables; libSystem/libc++ stay dynamic.
    exe_linker_flags=''
    ;;
  *)
    default_cc=/usr/bin/gcc-14 default_cxx=/usr/bin/g++-14
    exe_linker_flags='-static -static-libstdc++ -static-libgcc'
    ;;
esac
cc=${LMBLENDER_CC:-$default_cc}
cxx=${LMBLENDER_CXX:-$default_cxx}
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
# Blender's FindOpenImageDenoise module expects legacy `common` / `dnnl_cpu`
# archive names. OIDN 2.x's static CMake package instead exposes core and CPU
# device archives as transitive targets, so provide those names as symlinks in
# a build-only prefix. The upstream source tree is not changed.
oidn_install=${OIDN_INSTALL_PREFIX:-$repo/build/oidn-static-install}
oidn_find_root="$build/oidn-find-prefix"
host_libdir=${BLENDER_LIBDIR:-/home/ubuntu/lib/linux_aarch64}
cycles_libdir="$build/libdir-with-static-oidn"
if [[ -f "$oidn_install/lib/libOpenImageDenoise.a" && \
      -f "$oidn_install/lib/libOpenImageDenoise_core.a" && \
      -f "$oidn_install/lib/libOpenImageDenoise_device_cpu.a" ]]; then
  mkdir -p "$oidn_find_root/lib" "$oidn_find_root/include"
  ln -sfn "$oidn_install/include/OpenImageDenoise" "$oidn_find_root/include/OpenImageDenoise"
  ln -sfn "$oidn_install/lib/libOpenImageDenoise.a" "$oidn_find_root/lib/libOpenImageDenoise.a"
  ln -sfn "$oidn_install/lib/libOpenImageDenoise_core.a" "$oidn_find_root/lib/libcommon.a"
  ln -sfn "$oidn_install/lib/libOpenImageDenoise_core.a" "$oidn_find_root/lib/libdnnl_common.a"
  ln -sfn "$oidn_install/lib/libOpenImageDenoise_device_cpu.a" "$oidn_find_root/lib/libdnnl_cpu.a"
else
  echo "Static OIDN archives missing; run scripts/build-oidn-static.sh first" >&2
  exit 1
fi
# Blender's legacy cache migration logic switches .a to a shared OIDN library
# whenever it sees one under LIBDIR. Use a build-only LIBDIR view that keeps
# the other host packages but omits that shared OIDN directory.
mkdir -p "$cycles_libdir"
for entry in "$host_libdir"/*; do
  [[ -e "$entry" ]] || continue
  [[ $(basename "$entry") == openimagedenoise ]] && continue
  ln -sfn "$entry" "$cycles_libdir/$(basename "$entry")"
done
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
  -DEigen3_DIR="$deps_install/eigen/share/eigen3/cmake" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DTIFF_LIBRARY="$static_prefix/lib/libtiff.a" \
  -DTIFF_INCLUDE_DIR="$static_prefix/include" \
  -DZLIB_LIBRARY="$deps_install/zlib/lib/libz.a" \
  -DZLIB_INCLUDE_DIR="$deps_install/zlib/include" \
  -DJPEG_LIBRARY="$deps_install/jpeg/lib/libjpeg.a" \
  -DJPEG_INCLUDE_DIR="$deps_install/jpeg/include" \
  -DPNG_LIBRARY="$deps_install/png/lib/libpng.a" \
  -DPNG_PNG_INCLUDE_DIR="$deps_install/png/include" \
  -DCMAKE_PROJECT_INCLUDE="$static_targets" \
  -DCMAKE_EXE_LINKER_FLAGS="$exe_linker_flags" \
  -DOPENIMAGEDENOISE_ROOT_DIR="$oidn_find_root" \
  -DOPENIMAGEDENOISE_INCLUDE_DIR="$oidn_find_root/include" \
  -DOPENIMAGEDENOISE_LIBRARY="$oidn_find_root/lib/libOpenImageDenoise.a" \
  -DOPENIMAGEDENOISE_OPENIMAGEDENOISE_LIBRARY="$oidn_find_root/lib/libOpenImageDenoise.a" \
  -DOPENIMAGEDENOISE_COMMON_LIBRARY="$oidn_find_root/lib/libcommon.a" \
  -DOPENIMAGEDENOISE_DNNL_CPU_LIBRARY="$oidn_find_root/lib/libdnnl_cpu.a" \
  -DBUILD_SHARED_LIBS=OFF \
  -DWITH_BLENDER=OFF -DWITH_CYCLES=OFF -DWITH_CYCLES_STANDALONE=ON \
  -DWITH_CYCLES_STANDALONE_GUI=OFF -DWITH_PYTHON=OFF \
  -DWITH_CYCLES_OSL=OFF -DWITH_CYCLES_TEST_OSL=OFF -DWITH_LLVM=OFF \
  -DWITH_CYCLES_EMBREE=OFF -DWITH_CYCLES_PATH_GUIDING=OFF \
  -DWITH_OPENVDB=OFF -DWITH_OPENSUBDIV=OFF \
  -DWITH_OPENIMAGEDENOISE=ON -DWITH_TBB=ON -DWITH_TBB_MALLOC_PROXY=OFF \
  -DWITH_USD=OFF -DWITH_HYDRA=OFF -DWITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF \
  -DWITH_CYCLES_DEVICE_CUDA=OFF -DWITH_CYCLES_DEVICE_HIP=OFF \
  -DWITH_CYCLES_DEVICE_OPTIX=OFF -DWITH_CYCLES_DEVICE_ONEAPI=OFF \
  -DWITH_CYCLES_DEVICE_METAL=OFF -DWITH_CUDA_DYNLOAD=OFF \
  -DWITH_GHOST=OFF -DWITH_X11=OFF -DWITH_OPENGL=OFF \
  -DWITH_VULKAN_BACKEND=OFF -DWITH_HARFBUZZ=OFF -DWITH_FRIBIDI=OFF \
  -DWITH_OPENAL=OFF -DWITH_SDL=OFF -DWITH_CODEC_SNDFILE=OFF -DWITH_CODEC_FFMPEG=OFF \
  -DWITH_FFTW3=OFF -DWITH_INPUT_NDOF=OFF -DWITH_XR_OPENXR=OFF -DWITH_GMP=OFF \
  -DWITH_MANIFOLD=OFF -DWITH_POTRACE=OFF -DWITH_HARU=OFF -DWITH_AUDASPACE=OFF \
  -DWITH_ALEMBIC=OFF -DWITH_MATERIALX=OFF -DWITH_DRACO=OFF -DWITH_IMAGE_WEBP=OFF \
  -DWITH_INTERNATIONAL=OFF -DWITH_FREESTYLE=OFF -DWITH_LIBMV=OFF -DWITH_QUADRIFLOW=OFF \
  -DWITH_MOD_OCEANSIM=OFF -DWITH_MOD_REMESH=OFF -DWITH_BULLET=OFF \
  -DWITH_GHOST_WAYLAND=OFF -DWITH_GHOST_X11=OFF -DWITH_PULSEAUDIO=OFF -DWITH_JACK=OFF -DWITH_PIPEWIRE=OFF \
  -DLIBDIR="$cycles_libdir"
if [[ ${CYCLES_CONFIGURE_ONLY:-0} == 1 ]]; then
  exit 0
fi
cmake --build "$build" --target cycles --parallel "$jobs"
file "$build/bin/cycles"
"$repo/scripts/check-static.sh" "$build/bin/cycles"
if [[ "$(uname -s)" == Darwin ]]; then
  # Record the system frameworks/libraries Cycles' own executable links so the
  # host (CMakeLists.txt) can reuse them; drop archives and object files.
  python3 - "$build/build.ninja" "$build/host-link-extras.txt" <<'PY'
import re, shlex, sys
text = open(sys.argv[1]).read()
block = re.search(r"^build bin/cycles: .*?(?=^build |\Z)", text, re.S | re.M).group(0)
flags = []
for key in ("LINK_FLAGS", "LINK_LIBRARIES"):
    m = re.search(rf"^\s+{key} = (.*)$", block, re.M)
    if m:
        flags += shlex.split(m.group(1))
items, i = [], 0
while i < len(flags):
    f = flags[i]
    if f == "-framework" and i + 1 < len(flags):
        items.append(f"-framework {flags[i + 1]}"); i += 2; continue
    if f.startswith("-l") or f.startswith("-Wl,-framework"):
        items.append(f)
    i += 1
unique = list(dict.fromkeys(items))
open(sys.argv[2], "w").write(" ".join(unique) + "\n")
print("host link extras:", " ".join(unique))
PY
fi
