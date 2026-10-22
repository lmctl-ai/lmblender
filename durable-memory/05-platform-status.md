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

- Windows run [39833128994](https://github.com/lmctl-ai/lmblender/actions/runs/39833128994), job 120014297816, passed the rendering regression assertions through the direct Cycles/XML oracle. The subsequent hostile corpus failed before compilation: Python CreateProcess does not resolve bare `lmscript` through PATHEXT to npm's `.cmd` launcher. Resolve the compiler once with `shutil.which` (also honors explicit LMSCRIPT), preserving the full corpus and assertions. Run-level success is not evidence: the Windows job failed because jobs use continue-on-error.
- Follow-up [39839086836/job/120035061008](https://github.com/lmctl-ai/lmblender/actions/runs/39839086836/job/120035061008) resolved the npm launcher successfully; the corpus then exposed Windows `Path` backslashes embedded unescaped in generated DSL (`unknown escape \h`). Serialize input and bytecode paths with `Path.as_posix()` for portable DSL literals. Rendering checks again passed; suite reached the corpus after 6m57s.

### Windows x64 green handoff — PlatAstra, 2026-10-22

- Branch `platastra/windows-static`, tested code `30bba1a444ee0f8004a688dced65fa59f855b1b6`. **Windows job conclusion: success**, [run 39840532704 / job 120040054074](https://github.com/lmctl-ai/lmblender/actions/runs/39840532704/job/120040054074), not merely the continue-on-error workflow conclusion.
- MSVC standalone Cycles + portable static dependency prefix remains the solution: `/MT`, no Blender top-level CMake, Python or GUI runtime. Host build and `--version`, full `test.sh`, explicit DLL check, render smoke, release staging, and artifact upload all passed. The full suite includes 287 hostile import/texture/bytecode/source cases; this Release job is not an ASan build. Optional external-Blender oracle checks remain conditional as before.
- `dumpbin` imports for the host are exactly `KERNEL32.DLL` and `ADVAPI32.DLL`. No VC runtime or third-party DLLs. The required bare executable `lmblender-windows-x64.exe` is 70,555,648 bytes, staged alongside checksums/notices. [CI artifact 12454415884](https://github.com/lmctl-ai/lmblender/actions/runs/39840532704/artifacts/12454415884) contains it; nothing was tagged or published as a release.
- Warm-cache Windows job: 19:29:30–19:43:21 UTC (13m51s); full suite 19:31:02–19:43:07 (12m05s). Earlier cold dependency build was 28m34s and standalone Cycles about 5m05s; see preceding investigation notes.
- After green, fetched and ran `git rebase origin/main`; already up to date on `51831ca`. Opus's Windows fixes are reconciled in this branch (one dependency cache and one static dependency/standalone Cycles route). Only this handoff note follows the tested code commit. Lead can merge the branch.

## macOS findings (PlatFable, 2026-10-22, branch platfable/macos)

Runner timings observed (runs 39805494859, 39820850743):
- macos-15 (arm64, 3 cores): bootstrap-static-deps 26-29 min, build-deps 5-6 min, OIDN <1 min; dependency cache
  saved after about 36 min; Cycles configure 19 s, build+link 2 min. macos-15-intel (x64, 4 cores): bootstrap
  51-72 min, build-deps 13-20 min, OIDN 2-3 min; cache saved after 68-95 min; Cycles build 6 min.
- The workflow `shell: bash` resolves to /bin/bash 3.2 on both macOS runners: no empty-array `"${a[@]}"`
  under `set -u`, no mapfile. GNU coreutils/gnu-sed/gpatch come from the gnubin PATH entries. Homebrew CMake is
  4.4.x there (Ubuntu has 3.28), which changes several Find modules (config-first lookups, see below).

Cycles configure/link failures seen on macOS and their causes (fixes landed on main by Opus in aee57c0,
affd2bb, 09e6abd; PlatFable reproduced them independently on platfable/macos):
- `platform_apple.cmake:58 Mac OSX requires pre-compiled libs at LIBDIR`: it demands `LIBDIR/.git` (Blender's
  precompiled-lib checkout marker) -> empty `.git` entry in the build-only LIBDIR view.
- x64 `missing static Embree archive libembree_avx.a`: AppleClang cannot build a static multi-ISA Embree, so
  Darwin-x86_64 builds only SSE4.2; only `libembree_sse42.a` exists there (configure script probes; host CMake
  lists sse42 only on APPLE).
- Blender's Apple CMake writes the standalone executable to `intern/cycles/app/Blender.app/Contents/MacOS/cycles`,
  not `bin/cycles`.
- First macOS `cycles` link depended on `@rpath/png.framework/...` and `@rpath/libz.1.dylib`: OpenImageIO bakes
  the literal PNG/ZLIB library paths it was configured with into `OpenImageIOTargets.cmake`
  (INTERFACE_LINK_LIBRARIES), and CMake's find modules prefer the harvest's png.framework/libz.dylib over the
  archives. Linux never noticed because `-static` forces archives. Fix: bootstrap deletes the shared
  zlib/png/freetype libraries and frameworks from the harvest on Darwin (passing ZLIB/PNG/JPEG `*_LIBRARY`
  archives explicitly to TIFF/OCIO/OIIO in build-deps.sh works as well).
- Avoid exposing the static prefix as a `LIBDIR/*` subdirectory: with CMake 4, FindTIFF then picks
  `TiffConfig.cmake`, whose `TIFF::tiff` links an undefined `CMath::CMath` (generate-time error). With the
  explicit `TIFF_LIBRARY` and module-mode lookup this never happens.
- Blender forces CMAKE_OSX_DEPLOYMENT_TARGET=11.2 for the Cycles archives; the host keeps 13.3. ld64 warns that
  the 13.3-target OIIO objects are newer than the 11.2 link target; warnings only.
- With those fixes both macOS `cycles` executables link only /System frameworks + libSystem + libc++ (otool -L).

Host build (test.sh) on macOS:
- `libbf_intern_libc_compat.a` is Linux-only (Blender adds intern/libc_compat under `UNIX AND NOT APPLE`); the
  host CMake skips it on Apple and lists only `libembree_sse42.a` on Intel macOS.
