# Platform status (macOS / Windows)

Shipped since v0.3.4: linux-arm64, linux-x64 (fully static), macos-arm64, macos-x64, windows-x64.exe.
The `platforms` workflow jobs are still continue-on-error, so a green workflow run does NOT mean those jobs
passed: read the per-job conclusions (`gh run view RUN --json jobs`). Automatic main-push releases require all five platform packages in dist, regardless of workflow-level
results. Tag releases and manual dry runs remain tolerant unless strict assembly is requested.

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

### Windows x64 verification (2026-10-22)

- Tested code `30bba1a444ee0f8004a688dced65fa59f855b1b6`. **Windows job conclusion: success**, [run 39840532704 / job 120040054074](https://github.com/lmctl-ai/lmblender/actions/runs/39840532704/job/120040054074), not merely the continue-on-error workflow conclusion.
- MSVC standalone Cycles + portable static dependency prefix remains the solution: `/MT`, no Blender top-level CMake, Python or GUI runtime. Host build and `--version`, full `test.sh`, explicit DLL check, render smoke, release staging, and artifact upload all passed. The full suite includes 287 hostile import/texture/bytecode/source cases; this Release job is not an ASan build. Optional external-Blender oracle checks remain conditional as before.
- `dumpbin` imports for the host are exactly `KERNEL32.DLL` and `ADVAPI32.DLL`. No VC runtime or third-party DLLs. The required bare executable `lmblender-windows-x64.exe` is 70,555,648 bytes, staged alongside checksums/notices. [CI artifact 12454415884](https://github.com/lmctl-ai/lmblender/actions/runs/39840532704/artifacts/12454415884) contains it; nothing was tagged or published as a release.
- Warm-cache Windows job: 19:29:30–19:43:21 UTC (13m51s); full suite 19:31:02–19:43:07 (12m05s). Earlier cold dependency build was 28m34s and standalone Cycles about 5m05s; see preceding investigation notes.
- Reconciled in main with dependency cache and static dependency / standalone Cycles build.

## macOS findings (2026-10-22)

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
- The host link failed with `ld: library 'Foundation' not found`: `separate_arguments()` splits the recorded
  `-framework Foundation` into two list items and target_link_libraries turns `Foundation` into `-lFoundation`.
  As plain link options CMake then de-duplicates the repeated `-framework` token; each pair is passed as a
  `SHELL:-framework X` link option group.

### macOS arm64 + x64 verification (2026-10-22)

- Tested tip `004ec4d` (= origin/main `e022bee` + three host-side commits: Apple-only
  Cycles archive list, SSE4.2-only Embree on Intel, `SHELL:-framework X` link options). Verification run
  https://github.com/lmctl-ai/lmblender/actions/runs/39844042875: every job success (macos-arm64 job
  120052468817, macos-x64 job 120052468832, windows too). Same result one commit earlier on run 39841404317.
- Both jobs ran the full `test.sh` suite (schema/protocol checks, every example and QA render, denoise,
  animation, presets): arm64 10m02s, x64 14m20s. `build/lmblender --version` prints
  `lmblender 0.3.3 (DSL bytecode v5)` / `Cycles 4.5.0` / `OpenImageDenoise 2.5.0` on both.
- `otool -L build/lmblender` on both: only /System/Library/Frameworks (Foundation, IOKit, Carbon, ColorSync,
  CoreFoundation, CoreGraphics, plus Accelerate on arm64 for OIDN's BNNS path), `/usr/lib/libSystem.B.dylib`
  and `/usr/lib/libc++.1.dylib`. No Homebrew, @rpath or third-party dylib.
- Release staging produced the bare executables `lmblender-macos-arm64` (84,341,912 bytes) and
  `lmblender-macos-x64` (89,689,200 bytes), the `.tar.gz` bundles, sidecar `.sha256` files and `.ldd`
  (otool) listings; uploaded as artifacts `lmblender-macos-arm64` / `lmblender-macos-x64`.
- Warm-cache job wall time: arm64 11m03s, x64 16m10s. Cold (dependency cache miss): arm64 about 50 min,
  x64 about 110 min, dominated by Blender's build_environment bootstrap.
- Follow-ups (not blockers): the Windows and macOS jobs are still `continue-on-error` and outside the release
  job's `needs`; promoting them is the Lead's call. Cycles archives target macOS 11.2 while the host targets
  13.3 (ld64 warnings only); platform_apple's extra frameworks (Carbon, ColorSync, CoreGraphics) come from
  OCIO/OIIO and are harmless system links.
### Tag release wiring — PlatAstra

- `platforms.yml` is callable by CI for v* tags and explicit dry runs. The single release job waits for platform completion but requires only both Linux builds and source to succeed; platform uploads remain success-only after tests/dependency validation/staging. Unique per-platform dependency report names prevent merge collisions; only assembly generates SHA256SUMS.
- Added branch workflow_dispatch `dry_run` (default true), which executes builds, source packaging, assembly validation and an `assembled-release-dist` upload, while skipping publication. No tag is needed.
- Assembly fixtures pass for Linux-only (optional failures) and all platforms. Guards reject missing source, partial optional platforms, absent Windows relinking info, and checksum mismatches. Source packaging now includes the pinned standalone Cycles tree needed to rebuild Windows. Per-platform relinking docs accompany bare binaries; complete license texts remain in each archive. `test.sh` already checks exactly one semantic version token matching VERSION on the first `--version` line, including the Windows executable.
- Before full validation, auditing `build-deps-portable.sh` against the source manifest found `ROBINMAP` missing (header-only OpenImageIO dependency). Added Blender's pinned v1.3.0 SHA-256/source archive and the upstream MIT text to notices/package contents. Initial dry run 39843146290 was cancelled to validate the complete package in one subsequent run.

### Release wiring verified — PlatAstra, 2026-10-22

- Branch `platastra/release-wiring`, final tested code `81b1cd7727897eade4be76ad4f9fef136ad8f065`, rebased on main `14820de` to include PlatFable's green macOS fixes. Only this evidence note follows the tested commit.
- **All five platform jobs, source, and release assembly succeeded** in [dry run 39849101628](https://github.com/lmctl-ai/lmblender/actions/runs/39849101628). [Release job 120086448796](https://github.com/lmctl-ai/lmblender/actions/runs/39849101628/job/120086448796) ran the guard fixtures, downloaded the same-run reusable-workflow artifacts, validated packages/source/checksums, and uploaded the assembled directory. The publication step's conclusion is explicitly `skipped`.
- Inspected the release job's complete dist listing: `lmblender-linux-arm64`, `lmblender-linux-x64`, `lmblender-macos-arm64`, `lmblender-macos-x64`, `lmblender-windows-x64.exe`; each has its versioned tar.gz, sidecars, notices, source-offer, relinking document, and uniquely named dependency report. One `lmblender-source-v0.3.3.tar.gz` plus sidecar. **42 assets covered by one SHA256SUMS, 43 files total.** [Assembled artifact 12458392472](https://github.com/lmctl-ai/lmblender/actions/runs/39849101628/artifacts/12458392472).
- Failure-path evidence: before rebasing, both macOS jobs failed in [39843339645](https://github.com/lmctl-ai/lmblender/actions/runs/39843339645), yet [release job 120075370266](https://github.com/lmctl-ai/lmblender/actions/runs/39843339645/job/120075370266) succeeded with both Linux targets, Windows, and source. It logged both macOS platforms absent and verified 26 assets; [fallback artifact 12456024396](https://github.com/lmctl-ai/lmblender/actions/runs/39843339645/artifacts/12456024396). No failed-platform artifact was uploaded or merged.
- Final Windows job [120075823211](https://github.com/lmctl-ai/lmblender/actions/runs/39849101628/job/120075823211) passed full test.sh (287 hostile cases), dependency check and render smoke. First version line: `lmblender 0.3.3 (DSL bytecode v5)`; the test asserts exactly one semantic-version token equal to VERSION. Imports remain only KERNEL32.DLL and ADVAPI32.DLL. Bare exe: 70,555,648 bytes.
- Source packaging resolves 35 components and ships 30 upstream archives, including the Windows standalone Cycles tree and robin-map. Package guards require the corresponding-source asset/sidecar and per-platform notices, source-offer, relinking document, and archive license files. Local assembly fixtures, checksum fixtures, shell syntax checks, source-manifest resolution, and actionlint 1.7.12 passed.
- Reproduce without a tag: `gh workflow run ci.yml --repo lmctl-ai/lmblender --ref platastra/release-wiring -f dry_run=true`. Tag pushes call the same optional platform workflow and assembly checks, then the one CI publication step. No tags or GitHub releases were created/updated during validation. Lead merges.

## Automatic releases

Every push to `main` that CI accepts is released automatically. The `version` job in `.github/workflows/ci.yml` picks `MAJOR.MINOR.<run number>` (MAJOR.MINOR come from the version source in the repo, the patch is the workflow run number), every build job runs `scripts/stamp-version.sh` to write that version into the working tree before building (never committed), so the binary reports it. When all platform builds and tests pass, the `release` job creates the tag `vMAJOR.MINOR.N` on the pushed commit and publishes the assets, marked latest. Pull requests and manual runs never release. Pushing a `v*` tag by hand still releases exactly that version. MAJOR.MINOR is the committed base: `project(... VERSION x.y.z)` in `CMakeLists.txt` (the `VERSION` file for lmblender).

## Complete automatic releases (2026-10-24)

- v0.3.127 was published with only four bare executables; the Windows asset was absent. Keep that release intact; the next complete successful automatic release supersedes it.
- Main-push release assembly now sets `REQUIRE_PLATFORMS=all`. Every bare binary, checksum, archive, notices, source-offer and relinking file is required before publication. Manual validation can exercise the exact gate with `dry_run=true require_all_platforms=true`.
- CI-called platform jobs use per-run concurrency groups, separate from standalone platform workflows. `cancel-in-progress: false` alone does not protect a pending job from replacement. Cancelled parent runs cannot enter release assembly; cancelled individual platforms cannot pass the strict asset gate.
- Version job, run-number stamping, source stamping and publication target design are unchanged. Fixtures reject every four-of-five platform combination and missing Windows checksum/notices, while preserving tolerant manual assembly.
- Actual v0.3.127 failure: [Windows job 120781030838](https://github.com/lmctl-ai/lmblender/actions/runs/40061038645/job/120781030838) failed `migrate-system-timeouts.py --check` with CP1252 UnicodeDecodeError on UTF-8 text. [Release job 120787579121](https://github.com/lmctl-ai/lmblender/actions/runs/40061038645/job/120787579121) nevertheless published. The first strict dry run reproduced that Windows failure. Make the maintenance script's text encoding explicitly UTF-8 so the complete-bundle dry run can also pass; do not suppress the check.
- The next Windows run passed the maintenance check but found the same locale-dependent decoding in the inline README gallery test. Set `PYTHONUTF8=1` for Windows build/test tools so all repository fixtures use UTF-8; retain the full test suite.
- **Negative integration proof:** [strict dry-run release job 120793198616](https://github.com/lmctl-ai/lmblender/actions/runs/40066201938/job/120793198616) validated both Linux and both macOS packages, then failed with `missing release asset: lmblender-windows-x64.exe`. Both upload and publication were skipped: a real 4/5 bundle cannot pass.
- **Positive integration proof:** [strict dry run 40068661002](https://github.com/lmctl-ai/lmblender/actions/runs/40068661002), tested commit `2d03d55`, passed every individual platform job, source and release assembly. [Release job 120801794350](https://github.com/lmctl-ai/lmblender/actions/runs/40068661002/job/120801794350) logged `Required release platforms: all`, validated all five, and generated one SHA256SUMS covering 42 assets (43 files total). Inspected the complete dist listing: five bare binaries, sidecars, archives, notices, source-offers, relinking docs, dependency reports, and corresponding source. [Assembled artifact 12551395175](https://github.com/lmctl-ai/lmblender/actions/runs/40068661002/artifacts/12551395175); publication explicitly skipped.
- Windows [job 120797271682](https://github.com/lmctl-ai/lmblender/actions/runs/40068661002/job/120797271682) passed the full regression suite, README gallery and packaging; imports only KERNEL32.DLL and ADVAPI32.DLL. `--version` reported `lmblender 0.3.6 (DSL bytecode v5)` and passed the single-version-token check. Windows finished roughly 14 minutes into the run; all-platform assembly finished after roughly 25 minutes.
- Rebased onto concurrent main commit `9916825`, which independently makes Python file encodings explicit. Retained that upstream maintenance-script implementation and kept Windows `PYTHONUTF8=1` for inline Python checks. Workflow/assembly/test logic is identical to the successful dry run; version/stamp design unchanged. Assembly fixtures, checksum fixtures, maintenance check under a non-UTF8 locale, actionlint and diff whitespace validation pass after rebase. No releases deleted or modified during validation.

## Tutorial Windows install failure (2026-10-24)

- [Tutorial run 40070058608](https://github.com/lmctl-ai/lmblender/actions/runs/40070058608) fails before rendering on Windows: every page's `_dsl_ install=...@latest` reports that the checksum file has no entry for `lmblender-windows-x64.exe`. The missing images are a consequence, not a Python path/quoting error.
- Latest at diagnosis is v0.3.128, whose assets contain no Windows executable or sidecar. Its merged SHA256SUMS therefore cannot satisfy the installer. The strict automatic-release fix already on main must publish a complete successor; do not weaken checksum verification or pin tutorials to conceal a broken Latest.
- Tutorial CI now also runs after successful push-to-main CI, so it tests the newly published Latest instead of retaining only a result from before release completion. Existing push/manual triggers and release gating are unchanged.
