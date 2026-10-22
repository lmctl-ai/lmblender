# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LMBLENDER_VERSION LIMIT_COUNT 1)
if(NOT LMBLENDER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z.-]+)?(\\+[0-9A-Za-z.-]+)?$")
  message(FATAL_ERROR "VERSION must contain one semantic version")
endif()
project(lmblender VERSION "${LMBLENDER_VERSION}" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LMBLENDER_FULL_STATIC "Link a fully static standalone host" ON)
set(BLENDER_SOURCE_TREE "${CMAKE_CURRENT_SOURCE_DIR}/build/blender-src-upstream-v5.2.2" CACHE PATH "Blender source tree used for the Cycles API")
set(CYCLES_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/cycles-static" CACHE PATH "Static Cycles build directory")
set(LMBLENDER_STATIC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/static-prefix-gcc14" CACHE PATH "Static OIIO/OCIO/EXR/TBB prefix")
set(BLENDER_DEPS_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/build/blender-deps-harvest" CACHE PATH "Harvested Blender dependencies")
set(BLENDER_LIBDIR "${BLENDER_DEPS_INSTALL}" CACHE PATH "Blender support headers, including sse2neon")
set(LMBLENDER_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/schema/lmblender.dsl.json")
set(LMBLENDER_PRESETS "${CMAKE_CURRENT_SOURCE_DIR}/presets/presets.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LMBLENDER_SCHEMA}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LMBLENDER_PRESETS}")
file(READ "${LMBLENDER_SCHEMA}" LMBLENDER_SCHEMA_HEX HEX)
string(LENGTH "${LMBLENDER_SCHEMA_HEX}" LMBLENDER_SCHEMA_HEX_LENGTH)
math(EXPR LMBLENDER_SCHEMA_LAST_BYTE "${LMBLENDER_SCHEMA_HEX_LENGTH} - 2")
set(LMBLENDER_SCHEMA_INITIALIZER "")
foreach(offset RANGE 0 ${LMBLENDER_SCHEMA_LAST_BYTE} 2)
  string(SUBSTRING "${LMBLENDER_SCHEMA_HEX}" ${offset} 2 byte)
  string(APPEND LMBLENDER_SCHEMA_INITIALIZER "0x${byte},")
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/schema_embedded.hpp"
  "#pragma once\n#include <cstddef>\nnamespace lmblender { inline constexpr unsigned char embedded_schema[] = {${LMBLENDER_SCHEMA_INITIALIZER}}; inline constexpr std::size_t embedded_schema_size = sizeof(embedded_schema); }\n")
file(READ "${LMBLENDER_PRESETS}" LMBLENDER_PRESET_HEX HEX)
string(LENGTH "${LMBLENDER_PRESET_HEX}" LMBLENDER_PRESET_HEX_LENGTH)
math(EXPR LMBLENDER_PRESET_LAST_BYTE "${LMBLENDER_PRESET_HEX_LENGTH} - 2")
set(LMBLENDER_PRESET_INITIALIZER "")
foreach(offset RANGE 0 ${LMBLENDER_PRESET_LAST_BYTE} 2)
  string(SUBSTRING "${LMBLENDER_PRESET_HEX}" ${offset} 2 byte)
  string(APPEND LMBLENDER_PRESET_INITIALIZER "0x${byte},")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/presets_embedded.hpp"
  "#pragma once\n#include <cstddef>\nnamespace lmblender { inline constexpr unsigned char embedded_presets[] = {${LMBLENDER_PRESET_INITIALIZER}}; inline constexpr std::size_t embedded_presets_size = sizeof(embedded_presets); }\n")
set(LMBLENDER_CYCLES_DEFINES "")
if(EXISTS "${CYCLES_BUILD_DIR}/CMakeCache.txt")
  file(STRINGS "${CYCLES_BUILD_DIR}/CMakeCache.txt" _cycles_oidn_config REGEX "^WITH_(CYCLES_)?OPENIMAGEDENOISE:BOOL=ON$")
  if(_cycles_oidn_config)
    string(APPEND LMBLENDER_CYCLES_DEFINES "#define WITH_OPENIMAGEDENOISE 1\n")
  endif()
  file(STRINGS "${CYCLES_BUILD_DIR}/CMakeCache.txt" _cycles_embree_config REGEX "^WITH_CYCLES_EMBREE:BOOL=ON$")
  if(_cycles_embree_config)
    string(APPEND LMBLENDER_CYCLES_DEFINES "#define WITH_EMBREE 1\n#define EMBREE_MAJOR_VERSION 4\n")
  endif()
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h"
  "#pragma once\n#include <cstring>\n#if defined(__aarch64__)\n#include <arm_neon.h>\n#endif\n#define CCL_NAMESPACE_BEGIN namespace ccl {\n#define CCL_NAMESPACE_END }\n${LMBLENDER_CYCLES_DEFINES}")
add_executable(lmblender cpp/main.cpp)
target_include_directories(lmblender PRIVATE cpp third_party "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
  target_compile_options(lmblender PRIVATE /W4)
else()
  target_compile_options(lmblender PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(lmblender PRIVATE LMBLENDER_VERSION="${PROJECT_VERSION}")
if(EXISTS "${CYCLES_BUILD_DIR}/lmblender-portable-cycles.txt")
  # Cycles built by scripts/configure-cycles-portable.sh (standalone Cycles
  # build system) against scripts/build-deps-portable.sh; used for Windows.
  # Use the same patched headers and app sources as the standalone archives.
  set(CYCLES_SOURCE "${CYCLES_BUILD_DIR}-source/overlay/src")
  target_sources(lmblender PRIVATE "${CYCLES_SOURCE}/app/oiio_output_driver.cpp" "${CYCLES_SOURCE}/app/cycles_xml.cpp")
  target_include_directories(lmblender PRIVATE "${CYCLES_SOURCE}" "${BLENDER_SOURCE_TREE}/intern/atomic"
    "${LMBLENDER_STATIC_PREFIX}/include" "${LMBLENDER_STATIC_PREFIX}/include/OpenEXR" "${LMBLENDER_STATIC_PREFIX}/include/Imath")
  if(DEFINED SSE2NEON_ROOT_DIR)
    target_include_directories(lmblender PRIVATE "${SSE2NEON_ROOT_DIR}" "${SSE2NEON_ROOT_DIR}/include")
  endif()
  target_compile_definitions(lmblender PRIVATE WITH_CYCLES_DEVICE_CPU WITH_TBB WITH_OCIO WITH_PUGIXML WITH_SYSTEM_PUGIXML
    OIIO_STATIC_DEFINE=1 OIDN_STATIC_LIB OpenColorIO_SKIP_IMPORTS)
  if(MSVC)
    target_compile_options(lmblender PRIVATE "/FI${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h" /Zc:__cplusplus /permissive- /bigobj)
    target_compile_definitions(lmblender PRIVATE _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS __TBB_NO_IMPLICIT_LINKAGE=1)
    set_property(TARGET lmblender PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
  else()
    target_compile_options(lmblender PRIVATE -include "${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
      target_compile_definitions(lmblender PRIVATE WITH_SSE2NEON)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
      target_compile_options(lmblender PRIVATE -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2)
    endif()
  endif()
  file(GLOB _portable_archives "${CYCLES_BUILD_DIR}/lib/*${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${LMBLENDER_STATIC_PREFIX}/lib/*${CMAKE_STATIC_LIBRARY_SUFFIX}")
  # Skip the legacy-named OIDN copies (common/dnnl_*) made for FindOpenImageDenoise.
  list(FILTER _portable_archives EXCLUDE REGEX "/(lib)?(common|dnnl_common|dnnl_cpu|zlib|tiffxx)${CMAKE_STATIC_LIBRARY_SUFFIX}$")
  if(WIN32)
    target_link_libraries(lmblender PRIVATE ${_portable_archives} ws2_32 shlwapi psapi dbghelp)
  elseif(APPLE)
    target_link_libraries(lmblender PRIVATE ${_portable_archives} "-framework Foundation")
  else()
    target_link_libraries(lmblender PRIVATE "-Wl,--start-group" ${_portable_archives} "-Wl,--end-group" dl m pthread)
    if(LMBLENDER_FULL_STATIC AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_options(lmblender PRIVATE -static -static-libstdc++ -static-libgcc)
    endif()
  endif()
elseif(EXISTS "${CYCLES_BUILD_DIR}/lib/libcycles_session.a")
  set(CYCLES_SOURCE "${BLENDER_SOURCE_TREE}/intern/cycles")
  target_sources(lmblender PRIVATE "${CYCLES_SOURCE}/app/oiio_output_driver.cpp" "${CYCLES_SOURCE}/app/cycles_xml.cpp")
  target_include_directories(lmblender PRIVATE
    "${CYCLES_SOURCE}" "${CYCLES_BUILD_DIR}/intern/cycles" "${CYCLES_BUILD_DIR}"
    "${BLENDER_SOURCE_TREE}/intern/atomic"
    "${BLENDER_LIBDIR}/sse2neon"
    "${BLENDER_DEPS_INSTALL}/pugixml/include"
    "${LMBLENDER_STATIC_PREFIX}/include")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/oidn-static-install/include/OpenImageDenoise/oidn.hpp")
    target_include_directories(lmblender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/build/oidn-static-install/include")
  endif()
  target_compile_options(lmblender PRIVATE -include "${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    # Match the arm64 Cycles archives, which use sse2neon for their SIMD types.
    if(NOT APPLE)
      target_compile_options(lmblender PRIVATE -march=armv8.2-a+dotprod+fp16+lse)
    endif()
    target_compile_definitions(lmblender PRIVATE WITH_SSE2NEON)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    # Match intern/cycles/CMakeLists.txt, which builds all Cycles code with SSE4.2.
    target_compile_options(lmblender PRIVATE -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2)
  endif()
  target_compile_definitions(lmblender PRIVATE WITH_CYCLES_DEVICE_CPU WITH_TBB WITH_OCIO WITH_PUGIXML WITH_SYSTEM_PUGIXML OIIO_STATIC_DEFINE=1 OpenColorIO_SKIP_IMPORTS _FILE_OFFSET_BITS=64 _LARGEFILE64_SOURCE _LARGEFILE_SOURCE __LITTLE_ENDIAN__)
  set(CYCLES_LIB "${CYCLES_BUILD_DIR}/lib")
  set(CYCLES_ARCHIVES
    "${CYCLES_LIB}/libcycles_device.a" "${CYCLES_LIB}/libcycles_scene.a"
    "${CYCLES_LIB}/libcycles_session.a" "${CYCLES_LIB}/libcycles_bvh.a"
    "${CYCLES_LIB}/libcycles_subd.a" "${CYCLES_LIB}/libcycles_graph.a"
    "${CYCLES_LIB}/libcycles_util.a" "${CYCLES_LIB}/libcycles_integrator.a"
    "${CYCLES_LIB}/libcycles_kernel_cpu.a" "${CYCLES_LIB}/libbf_intern_sky.a"
    "${CYCLES_LIB}/libbf_intern_libc_compat.a" "${CYCLES_LIB}/libbf_intern_guardedalloc.a")
  set(STATIC_DEPS
    "${LMBLENDER_STATIC_PREFIX}/lib/libOpenImageIO.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libOpenImageIO_Util.a"
    "${BLENDER_DEPS_INSTALL}/png/lib/libpng.a"
    "${BLENDER_DEPS_INSTALL}/zlib/lib/libz.a"
    "${BLENDER_DEPS_INSTALL}/jpeg/lib/libjpeg.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libtiff.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libtbb.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libOpenColorIO.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/deps-gcc14/opencolorio/ext/dist/lib/libexpat.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/deps-gcc14/opencolorio/ext/dist/lib/libpystring.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/deps-gcc14/opencolorio/ext/dist/lib/libyaml-cpp.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/deps-gcc14/opencolorio/ext/dist/lib/libminizip-ng.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libOpenEXR-3_4.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libOpenEXRCore-3_4.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libImath-3_2.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libIlmThread-3_4.a"
    "${LMBLENDER_STATIC_PREFIX}/lib/libIex-3_4.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/deps-gcc14/openexr/external/OpenJPH/src/core/libopenjph.a"
    "${BLENDER_DEPS_INSTALL}/zstd/lib/libzstd.a"
    "${BLENDER_DEPS_INSTALL}/fmt/lib/libfmt.a"
    "${BLENDER_DEPS_INSTALL}/pugixml/lib/libpugixml.a")
  if(_cycles_embree_config)
    list(APPEND STATIC_DEPS "${LMBLENDER_STATIC_PREFIX}/lib/libembree4.a")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
      foreach(_isa IN ITEMS sse42 avx avx2)
        list(APPEND STATIC_DEPS "${LMBLENDER_STATIC_PREFIX}/lib/libembree_${_isa}.a")
      endforeach()
    endif()
    foreach(_component IN ITEMS lexers math simd sys tasking)
      list(APPEND STATIC_DEPS "${LMBLENDER_STATIC_PREFIX}/lib/lib${_component}.a")
    endforeach()
  endif()
  if(EXISTS "${CYCLES_BUILD_DIR}/CMakeCache.txt")
    file(STRINGS "${CYCLES_BUILD_DIR}/CMakeCache.txt" _cycles_oidn_config REGEX "^WITH_OPENIMAGEDENOISE:BOOL=ON$")
    if(_cycles_oidn_config)
      set(OIDN_STATIC_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/build/oidn-static-install" CACHE PATH "Static CPU OIDN prefix used by Cycles")
      list(APPEND STATIC_DEPS
        "${OIDN_STATIC_INSTALL}/lib/libOpenImageDenoise.a"
        "${OIDN_STATIC_INSTALL}/lib/libOpenImageDenoise_core.a"
        "${OIDN_STATIC_INSTALL}/lib/libOpenImageDenoise_device_cpu.a")
    endif()
  endif()
  foreach(_archive IN LISTS CYCLES_ARCHIVES STATIC_DEPS)
    if(NOT EXISTS "${_archive}")
      message(FATAL_ERROR "Missing static Cycles dependency: ${_archive}; build Cycles first")
    endif()
  endforeach()
  if(APPLE)
    # ld64 resolves archive cycles itself. Reuse the system frameworks/libraries
    # recorded from Cycles' own `cycles` link line; every third-party library
    # stays a static archive, and only libSystem/libc++ remain dynamic.
    set(_cycles_link_extras "")
    if(EXISTS "${CYCLES_BUILD_DIR}/host-link-extras.txt")
      file(READ "${CYCLES_BUILD_DIR}/host-link-extras.txt" _cycles_link_extras)
      separate_arguments(_cycles_link_extras UNIX_COMMAND "${_cycles_link_extras}")
    endif()
    target_link_libraries(lmblender PRIVATE ${CYCLES_ARCHIVES} ${STATIC_DEPS} ${_cycles_link_extras})
  else()
    target_link_libraries(lmblender PRIVATE "-Wl,--start-group" ${CYCLES_ARCHIVES} ${STATIC_DEPS} "-Wl,--end-group" dl util m pthread)
  endif()
  if(LMBLENDER_FULL_STATIC AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(lmblender PRIVATE -static -static-libstdc++ -static-libgcc)
  endif()
else()
  message(STATUS "Static Cycles API build not found; building the DSL host/import spike only")
endif()
