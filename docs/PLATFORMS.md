# Platform support plan

Release assets follow the lmscript `install=` convention `lmblender-OS-ARCH[.exe]`:

| Asset | Runner | Status |
|---|---|---|
| `lmblender-linux-x64` | `ubuntu-24.04` | shipped (fully static, `ldd`: not a dynamic executable) |
| `lmblender-linux-arm64` | `ubuntu-24.04-arm` | shipped (fully static) |
| `lmblender-macos-arm64` | `macos-15` (Apple silicon, 3 cores / 7 GB) | phase 2, step 1 |
| `lmblender-windows-x64.exe` | `windows-2022` (4 cores / 16 GB) | phase 2, step 2 (highest risk) |
| `lmblender-macos-x64` | `macos-15-intel` (as in lmctl-ai/lmpy) | phase 2, step 3 |

lmctl-ai/lmpy ships the same five names from one matrix (`macos-15`, `macos-15-intel`,
`windows-2022` with MinGW-w64) and a release job that `needs:` every platform, attaches the bare
executables, per-platform `.tar.gz`/`.zip` archives and `SHA256SUMS`. lmblender follows that shape.

## How the Linux build works today

1. `fetch-cycles-source.sh`: sparse, pinned Blender v5.2.2 checkout (Cycles 4.5.0 sources).
2. `bootstrap-static-deps.sh`: configures Blender `build_files/build_environment` (downloads
   every pinned package from Blender's hash-checked lib-source mirror), builds ~20
   `external_*` targets, and copies the needed `<build>/Release/<name>` prefixes into a harvest dir.
3. `build-deps.sh`: rebuilds TBB, Imath, OpenEXR, TIFF, OpenColorIO, and OpenImageIO as static archives.
4. `build-oidn-static.sh`: static CPU-only OIDN 2.5 using the pinned ISPC 1.30 binary.
5. `configure-cycles-static.sh`: Blender's top-level CMake with `WITH_CYCLES_STANDALONE=ON`,
   Blender-only features off, `LIBDIR` = harvest dir, and a source overlay that applies
   `patches/cycles-*.patch`.
6. `test.sh`: host build, schema/protocol checks, renders and QA scripts; then `ldd` and release staging.

Cold Linux build: about 43 min to the test step (bootstrap 30, image deps 5, OIDN under 1,
Cycles 2). Warm (prefix cache hit): about 11 min for the whole job.

## (a) Dependency build scripts

| Item | macOS arm64 / x64 | Windows x64 |
|---|---|---|
| build_environment | Supported upstream on macOS (Blender devs run `make deps`). `check_software.cmake` needs Homebrew `autoconf automake bison dos2unix flex libtool meson ninja pkg-config yasm`, with brew bison on PATH. | Supported only through `windows/build_deps.cmd` (MSVC, `/MD`). Several external targets in our list (Python, libheif and its codecs pulled in by OpenImageIO) are slow and fragile on Windows. |
| Harvest list | Same as Linux, plus `freetype` and `bzip2`, which `platform_apple.cmake` always requires. `sse2neon` on arm64 only. | Not applicable if Blender's top-level CMake is bypassed (see below). |
| `build-deps.sh` | Portable CMake; drop the GCC compiler-id check for Apple clang. Set `CMAKE_OSX_DEPLOYMENT_TARGET` (13.3 like lmpy, for `std::to_chars`). | Needs `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` (`/MT`) on every build, plus Windows-specific library names (`*.lib`). |
| OIDN | Pinned ISPC has `ispc-v1.30.0-macOS.universal.tar.gz`. OIDN CPU device builds on Apple clang (Metal device off). | `ispc-v1.30.0-windows.zip`. OIDN builds with MSVC; static build already supported (`OIDN_STATIC_LIB`). |
| Cycles configure | Blender top-level CMake on Apple uses `platform_apple.cmake`. It requires a `LIBDIR`, which works with our harvest dir exactly as on Linux. Extra unconditional finds: ZLIB, BZip2, Freetype, OpenEXR, Epoxy, PNG, JPEG, fmt, OIIO, OCIO. | `platform_win32.cmake` requires MSVC 2022 17.14.14+ and a `LIBDIR` containing `.git` with Blender's precompiled `lib/windows_x64` layout (169 hard-coded `${LIBDIR}/...` paths, `/MD` CRT). Blender's precompiled lib repos (projects.blender.org `lib-windows_x64`) return HTTP 403 to non-browser clients from this host. |
| Overlay patches | Harmless. The BVH2 fix is a source-level `make_int4` rewrite, valid on every compiler; the motion patch is gated on `GNU` + aarch64. Needs `patch`. | Same, plus `patch` from Git for Windows. |
| Shell scripts | bash 3.2 on macOS: avoid `mapfile`/`${var,,}`; `sha256sum` becomes `shasum -a 256`, `md5sum` becomes `md5 -r`; `realpath -m` is not in BSD realpath (use python). | Run under Git Bash (`shell: bash`). Paths need `cygpath -m` before passing to CMake/MSVC. |

**Windows recommendation:** don't use Blender's top-level CMake. Build Cycles with the standalone
Cycles build system: `CMakeLists.txt` and `src/cmake` from github.com/blender/cycles. With
`WITH_LIBS_PRECOMPILED=OFF` it resolves every dependency through `*_ROOT_DIR` hints. Feed it Blender
v5.2.2's `intern/cycles` sources (our pin, which reports Cycles 4.5.0; the GitHub tag `v5.2.0`
reports 5.3 and differs in 55 files, so its sources are not used). The dependencies come from a
new `scripts/build-deps-portable.sh`, which builds plain CMake projects from the tarballs that the
build_environment configure has already downloaded and hash-checked: zlib, libpng, libjpeg-turbo,
zstd, fmt, pugixml, OpenJPEG, Brotli, expat, TBB, Imath, OpenEXR (with its vendored OpenJPH),
TIFF, OpenColorIO, OpenImageIO, and OIDN. That avoids Python, libheif and Epoxy entirely and gives
`/MT` control over every archive. The same script can later replace the build_environment build
step on macOS and Linux, which cuts their cold builds too.

## (b) Host code POSIX assumptions (`cpp/`)

The host is already mostly portable C++20. It uses `std::filesystem`, `std::thread` and
`std::chrono`; there is no fork/exec, signals, rlimit, symlink checks, `ssize_t` or thread
affinity. Remaining items:

| Location | POSIX use | Portable replacement |
|---|---|---|
| `main.cpp` `#include <unistd.h>` | `getpid()` for temp-file suffixes (6 call sites) | Small `cpp/platform.hpp` helper: `_getpid()` from `<process.h>` on `_WIN32`, `getpid()` elsewhere. |
| `main.cpp` APNG staging | `::mkdtemp("/tmp/lmblender-apng-XXXXXX")` | `std::filesystem::temp_directory_path()` plus a random suffix and `create_directory` retry loop. |
| `main.cpp` atomic publish | `std::filesystem::rename(tmp, out)` | Already portable. MSVC's STL uses `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`, which replaces an existing target. |
| `main.cpp` | `M_PI` (22 uses) | `_USE_MATH_DEFINES` compile definition on MSVC; no code change. |
| `cpp/dslvm.hpp`, `cpp/gltf_import.hpp` | none found | — |
| `CMakeLists.txt` | `dl util m pthread`, `-Wl,--start-group`, `-static*`, `-march` / SSE flags | Per-OS branches (see (c)). |

## (c) Static linking rules per OS

| OS | Rule | Gate in CI |
|---|---|---|
| Linux | `-static -static-libstdc++ -static-libgcc` (fully static) | `ldd`: "not a dynamic executable" |
| macOS | Fully static executables are unsupported. Link every third-party archive statically; libSystem and libc++ stay dynamic (system libraries present on every macOS). Linking libc++ statically (`-nostdlib++` plus a toolchain `libc++.a`) is possible with a Homebrew LLVM but buys nothing, because libSystem must be dynamic anyway. Recommended: Apple clang, dynamic libc++. `-mmacosx-version-min=13.3`. | `otool -L`: allow only `/usr/lib/libSystem.B.dylib`, `/usr/lib/libc++.1.dylib` and `/System/Library/Frameworks/*` (Accelerate/Foundation if OIDN or TBB pull them in). |
| Windows | MSVC `/MT` (static CRT) for the host and every dependency (`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`), so no VC++ redistributable is needed. | `dumpbin /dependents`: allow only OS DLLs (`KERNEL32`, `ADVAPI32`, `USER32`, `SHELL32`, `OLE32`, `WS2_32`, `BCRYPT`, …); reject `VCRUNTIME*`, `MSVCP*`, `tbb*.dll`, `OpenImageDenoise*.dll`. |

## (d) GCC 14 workarounds

`patches/cycles-bvh2-instance-sharing.patch` is a plain source fix that is correct on every compiler,
so it stays applied everywhere. `patches/cycles-motion-vectorizer.patch` only adds
`-fno-tree-vectorize` under `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` on aarch64, so it is a no-op for
Apple clang and MSVC. The Linux-only BVH sharing regression test (`mesh-instances`) runs on every
platform through `test.sh`, so a clang/MSVC miscompile of the same loop would still be caught.

## (e) ISPC / OIDN CPU kernels per architecture

The pinned ISPC 1.30.0 ships `linux`, `linux.aarch64`, `macOS.universal`, and `windows` archives.
OIDN 2.5 CPU kernels cover SSE4/AVX2/AVX-512 on x64 and NEON on arm64. Cycles builds its own
SSE4.2/AVX2 kernel variants on x64 and NEON (via sse2neon) on arm64. The host must use the same
baseline flags as the Cycles archives (`-msse4.2` on x86-64 Linux/macOS, nothing extra for MSVC x64,
`-march=armv8.2-a+...` only for Linux arm64; Apple clang targets `apple-m1` by default).

## (f) Runner budgets and caching

| Runner | Cores / RAM | Expected cold build | Notes |
|---|---|---|---|
| `ubuntu-24.04(-arm)` | 4 / 16 GB | about 43 min | measured |
| `macos-15` arm64 | 3 / 7 GB | 60–90 min (estimate) | `BUILD_JOBS=3`; OIIO/OCIO compile memory fits in 7 GB |
| `macos-15-intel` | 4 / 14 GB | 60–90 min (estimate) | |
| `windows-2022` | 4 / 16 GB | 60–120 min (estimate) | MSVC compile of OIIO/OCIO is slow; the 6 h job limit is ample |

Reuse the two-level prefix cache (dependency prefix, then dependency + Cycles prefix), keyed on OS,
architecture, compiler version and script hashes. Per-repository cache storage is 10 GB; each
Linux prefix is a few hundred MB, so five platforms fit.

## Effort estimate and order

1. **macOS arm64** (1–2 days of CI iteration): same pipeline as Linux arm64, with Apple clang,
   Homebrew tools, the Freetype/BZip2 harvest, the `platform.hpp` shim (also needed for Windows),
   and Apple link rules plus the `otool` gate.
2. **Windows x64** (3–5 days, highest risk): `build-deps-portable.sh` plus the standalone Cycles
   build system with `/MT`, and Git Bash for `test.sh`. If it can't pass in this cycle, ship it as an
   experimental `continue-on-error` job and record the exact blocker here.
3. **macOS x64** (about 0.5 day after step 1): same job as macOS arm64 on `macos-15-intel`,
   with the x86 ISPC and SSE4.2 host flags.

The release job `needs:` every required platform. Experimental platforms don't block it, and the
job publishes whichever binaries were built.
