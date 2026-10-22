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
