cmake_minimum_required(VERSION 3.24)
project(lmblender LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LMBLENDER_FULL_STATIC "Link a fully static standalone host" ON)
set(BLENDER_SOURCE_TREE "/home/ubuntu/blender-src" CACHE PATH "Blender source tree used for the Cycles API")
set(CYCLES_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/cycles-static" CACHE PATH "Static Cycles build directory")
set(LMBLENDER_STATIC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/static-prefix-gcc14" CACHE PATH "Static OIIO/OCIO/EXR/TBB prefix")
set(BLENDER_DEPS_INSTALL "/home/ubuntu/blender-deps/install" CACHE PATH "Harvested Blender dependencies")
set(LMBLENDER_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/schema/lmblender.dsl.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LMBLENDER_SCHEMA}")
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
set(LMBLENDER_CYCLES_DEFINES "")
if(EXISTS "${CYCLES_BUILD_DIR}/CMakeCache.txt")
  file(STRINGS "${CYCLES_BUILD_DIR}/CMakeCache.txt" _cycles_oidn_config REGEX "^WITH_OPENIMAGEDENOISE:BOOL=ON$")
  if(_cycles_oidn_config)
    string(APPEND LMBLENDER_CYCLES_DEFINES "#define WITH_OPENIMAGEDENOISE 1\n")
  endif()
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h"
  "#pragma once\n#include <cstring>\n#if defined(__aarch64__)\n#include <arm_neon.h>\n#endif\n#define CCL_NAMESPACE_BEGIN namespace ccl {\n#define CCL_NAMESPACE_END }\n${LMBLENDER_CYCLES_DEFINES}")
add_executable(lmblender cpp/main.cpp)
target_include_directories(lmblender PRIVATE cpp "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_options(lmblender PRIVATE -Wall -Wextra -Wpedantic)
if(EXISTS "${CYCLES_BUILD_DIR}/lib/libcycles_session.a")
  set(CYCLES_SOURCE "${BLENDER_SOURCE_TREE}/intern/cycles")
  target_sources(lmblender PRIVATE "${CYCLES_SOURCE}/app/oiio_output_driver.cpp" "${CYCLES_SOURCE}/app/cycles_xml.cpp")
  target_include_directories(lmblender PRIVATE
    "${CYCLES_SOURCE}" "${CYCLES_BUILD_DIR}/intern/cycles" "${CYCLES_BUILD_DIR}"
    "${BLENDER_SOURCE_TREE}/intern/atomic"
    "/home/ubuntu/lib/linux_aarch64/sse2neon"
    "${BLENDER_DEPS_INSTALL}/pugixml/include"
    "${LMBLENDER_STATIC_PREFIX}/include")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/oidn-static-install/include/OpenImageDenoise/oidn.hpp")
    target_include_directories(lmblender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/build/oidn-static-install/include")
  endif()
  target_compile_options(lmblender PRIVATE -march=armv8.2-a+dotprod+fp16+lse -include "${CMAKE_CURRENT_BINARY_DIR}/generated/cycles_config.h")
  target_compile_definitions(lmblender PRIVATE WITH_CYCLES_DEVICE_CPU WITH_TBB WITH_OCIO WITH_PUGIXML WITH_SSE2NEON WITH_SYSTEM_PUGIXML OIIO_STATIC_DEFINE=1 OpenColorIO_SKIP_IMPORTS _FILE_OFFSET_BITS=64 _LARGEFILE64_SOURCE _LARGEFILE_SOURCE __LITTLE_ENDIAN__)
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
  target_link_libraries(lmblender PRIVATE "-Wl,--start-group" ${CYCLES_ARCHIVES} ${STATIC_DEPS} "-Wl,--end-group" dl util m pthread)
  if(LMBLENDER_FULL_STATIC AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(lmblender PRIVATE -static -static-libstdc++ -static-libgcc)
  endif()
else()
  message(STATUS "Static Cycles API build not found; building the DSL host/import spike only")
endif()
