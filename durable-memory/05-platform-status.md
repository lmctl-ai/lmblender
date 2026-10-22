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
