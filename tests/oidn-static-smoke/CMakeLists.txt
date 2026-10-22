cmake_minimum_required(VERSION 3.20)
project(oidn_static_smoke LANGUAGES CXX)
find_package(OpenImageDenoise 2.5 REQUIRED)
add_executable(oidn_static_smoke main.cpp)
target_compile_features(oidn_static_smoke PRIVATE cxx_std_17)
target_link_libraries(oidn_static_smoke PRIVATE OpenImageDenoise)
# Fully static executables are Linux-only; macOS keeps libSystem/libc++ dynamic
# (scripts/check-static.sh enforces the per-OS rule).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(oidn_static_smoke PRIVATE -static -static-libstdc++ -static-libgcc)
endif()
