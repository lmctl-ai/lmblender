# Platform status (macOS / Windows)

Shipped: linux-arm64, linux-x64 (fully static, `ldd` says "not a dynamic executable").
Not shipped: macos-arm64, macos-x64, windows-x64.exe. The `platforms` workflow builds them as experimental jobs
(continue-on-error), so a green workflow run does NOT mean those jobs passed: read the per-job conclusions
(`gh run view RUN --json jobs`). The tag-triggered release job `needs` only the Linux build + source jobs, so
Linux can release without the other platforms.

Known facts:
- Blender's precompiled lib repos (lib-windows_x64) return HTTP 403 to non-browser clients: Windows must not use
  Blender's top-level CMake; plan is standalone Cycles + portable dependency script (see docs/PLATFORMS.md).
- macOS: build_environment is supported upstream; needs Homebrew tools and `shasum -a 256`, no `mapfile`, no
  `realpath -m` (bash 3.2/BSD userland). Deployment target 13.3 (for `std::to_chars`).
- Windows: `/MT` static CRT on every archive, Git Bash `shell: bash`, `cygpath -m` for CMake paths.
- Shared dev host lesson: heavy parallel C++ jobs OOM the 15 GB box; the memguard compiler shim and the CI
  per-job concurrency exist because of it.

## Windows investigation (PlatAstra, 2026-10-22)

- Branch: `platastra/windows-static`; using standalone Cycles + MSVC `/MT`.
- Main Windows job https://github.com/lmctl-ai/lmblender/actions/runs/39805494859/job/119917500529
  built the portable dependencies successfully in 8m10s (15:31:27–15:39:37 UTC),
  then failed at Cycles configuration: `Could NOT find Zstd (missing: ZSTD_LIBRARY)`.
  The installed archive is `prefix/lib/zstd_static.lib`; provide that explicit
  Windows-only hint. The successful dependency build favors continuing MSVC
  over introducing a MinGW port. End-to-end runtime validation remains pending.
- Static inspection of pinned standalone Cycles `src/cmake/configure_build.cmake`
  found hard-coded `/MD` configuration flags. The overlay removes those flags
  so CMP0091 and `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` select the CRT.
- Portable path now builds static CPU Embree to meet `test.sh`'s existing Embree
  requirement. OCIO's private archives are copied into the prefix, so the host
  archive list and dependency cache do not depend on its temporary build tree.
- Added Windows-only workflow dispatch, a separate dependency prefix cache,
  and the shared regression step. Windows checkout preserves LF bytes; host
  stdout uses binary mode for exact schema/protocol output. Tests use native
  paths and Python-created real directory symlinks under Windows. CI pending.
- Host warning flags now use `/W4` on MSVC. Both standalone Cycles and the
  host suppress TBB headers' implicit `tbb12.lib` linkage: our explicitly linked
  archive is `tbb.lib`, and the old FindTBB path does not propagate the modern
  imported target's static-link compile definitions.
- First branch CI: https://github.com/lmctl-ai/lmblender/actions/runs/39816335988/job/119955746633
  passed Zstd discovery and CMake generation. Cold dependencies took about
  11m19s. Cycles then failed compiling `util/guarded_allocator.h`: missing
  `std::_Container_proxy` and `std::allocator`. Add `<memory>` in the portable
  overlay and the host's forced configuration header (the host includes the
  pristine pinned source headers). The log also confirms `/MT` overrode the
  earlier hard-coded `/MD`; removing `/MD` eliminates that conflicting flag.
- Rebased over main's macOS LIBDIR and Windows cache/Zstd changes. This branch
  retains the explicit `ZSTD_LIBRARY` hint instead of installing a duplicate
  `zstd.lib` archive; the dependency script therefore remains identical to the
  in-flight Embree-enabled cache build. There is one dependency-cache path.
- First branch cold timing breakdown: OCIO about 3m02s, OIIO about 3m51s,
  OIDN about 1m11s. Archive fetch/extraction took about 38s.
- Adopted main `09e6abd`'s direct expat/yaml-cpp/pystring/minizip-ng builds:
  OCIO ExternalProjects did not inherit `/MT`. This supersedes copying their
  archives from the OCIO build tree. The pystring wrapper must call
  `cmake_minimum_required` before `project` to initialize CMP0091 correctly.
- Adopted main's `cycles-msvc-guarded-allocator.patch`, which also gates the
  MSVC container-proxy specialization on `_ITERATOR_DEBUG_LEVEL`. The portable
  host now compiles against that same source overlay; full prefix caches include
  the overlay. This supersedes the forced `<memory>` host-header workaround.
- Main Windows log https://github.com/lmctl-ai/lmblender/actions/runs/39823687761/job/119981239882
  exposed OCIO discovery failures for expat/yaml-cpp/minizip-ng. OCIO's find
  modules skip installed CONFIG packages when `*_ROOT` is defined. Let
  `CMAKE_PREFIX_PATH` discover those packages, retain their static definitions
  in the generated targets file, and build minizip with `MZ_COMPAT=OFF` and
  the default include directory (the prior override nested include paths twice).
- Stopped obsolete runs 39816999171 and 39818470239 after adopting the direct
  OCIO dependency builds; their old helpers cannot satisfy the static CRT gate.
- Canceled job log https://github.com/lmctl-ai/lmblender/actions/runs/39816999171/job/119957985877
  shows Embree compilation from 16:58:50 to 17:52:44 UTC, reaching 269/271
  steps without a compiler error. This was CPU compilation, not a download
  stall. Windows now builds one SSE4.2 Embree ISA (same feature coverage; no
  AVX/AVX2 dispatch variants) to reduce cold cost. Cycles uses the exported
  `embree` target instead of its legacy finder that assumes all ISA archives.
- Run https://github.com/lmctl-ai/lmblender/actions/runs/39826754516/job/119991962316
  built and cached the complete static prefix in 28m34s, then built Cycles and
  passed its OS-DLL import gate. Host compile failed because `/FI"path"` in
  CMake preserved literal quotes in the header filename. Use one quoted CMake
  argument `"/FI${path}"`; the next run reuses the Cycles cache.
- Single-ISA Embree measured 16m53s. Cycles built in about 5m05s and imports
  only `KERNEL32.DLL` and `ADVAPI32.DLL`.
- Host retry https://github.com/lmctl-ai/lmblender/actions/runs/39832095729/job/120010582714
  passed forced-include lookup; fmt requires `/utf-8`, and the watchdog's GCC
  `__atomic_load_n` is unavailable on MSVC. Add `/utf-8` and use the pinned
  Blender `atomic_load_uint64` portability helper without disabling the watchdog.
- Run https://github.com/lmctl-ai/lmblender/actions/runs/39832510840/job/120012076849
  compiled the host and ran `--version`, then entered `test.sh`. The release
  checksum fixture failed because GNU binary-mode sums mark filenames with `*`,
  while the cross-check assumed a text-mode filename. Accept either marker
  without changing file digests; exercise the existing fixture with both modes.
