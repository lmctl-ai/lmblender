# Relinking and rebuild notes

Release binaries are statically linked (GNU GCC 14 and glibc on Linux; see `docs/PLATFORMS.md` for the experimental macOS builds). Each release ships its complete corresponding source as `lmblender-source-vX.Y.Z.tar.gz`; `SOURCE_OFFER.md` lists what that asset contains and `SOURCES.md` inside it records every component's version, URL, hash, and license.

To rebuild and relink from the source asset:

1. Unpack `lmblender-source-vX.Y.Z.tar.gz`. `lmblender/` is the repository at the release commit; `upstream/` holds the pinned Blender subset, the patched Cycles overlay, and the third-party archives.
2. Follow `lmblender/BUILD.md`: `scripts/fetch-cycles-source.sh` (or unpack `upstream/blender-*-subset.tar.gz` to `build/blender-src-upstream-v5.2.2`), `scripts/bootstrap-static-deps.sh`, `scripts/build-deps.sh`, `scripts/build-oidn-static.sh`, `scripts/configure-cycles-static.sh`, then `cmake -S . -B build -DLMBLENDER_FULL_STATIC=ON && cmake --build build`. Point `BLENDER_PACKAGES_DIR` at `upstream/` to reuse the shipped archives; Blender's `build_environment` otherwise re-downloads the same hash-verified files.
3. To relink against a different C/C++ runtime, configure with `-DLMBLENDER_FULL_STATIC=OFF` (dynamic glibc/libstdc++) or supply your own static runtime archives; the link line is in `CMakeLists.txt` (`STATIC_DEPS`, `CYCLES_ARCHIVES`).

glibc, libstdc++, and libgcc come from the Ubuntu 24.04 `gcc-14` and `libc6-dev` packages (GPL-3.0 System Libraries; the GCC Runtime Library Exception applies to libstdc++/libgcc). They are not re-shipped in the source asset.

Verify a release binary with:

```sh
./lmblender --version
ldd ./lmblender
```

The expected Linux `ldd` result is `not a dynamic executable`. The executable still uses glibc interfaces such as `getpwuid`; static glibc binaries run on compatible Linux kernels and CPU architectures and should be rebuilt with the target toolchain for other environments.

## Windows x64

Windows releases use Visual Studio 2022's x64 MSVC toolchain and the static CRT
(`/MT`, CMake `MSVC_RUNTIME_LIBRARY=MultiThreaded`), with standalone Cycles and
CPU-only static dependencies. No Python or GUI runtime is required. From an x64
MSVC developer environment with Git Bash, CMake, Ninja, Python (build-time only),
and ISPC available, follow the Windows job in `.github/workflows/platforms.yml`:
`scripts/fetch-cycles-source.sh`, `scripts/build-deps-portable.sh`, then
`scripts/configure-cycles-portable.sh`. Configure the host with
`CYCLES_BUILD_DIR=build/cycles-static` and
`LMBLENDER_STATIC_PREFIX=build/portable-deps/prefix` (absolute native Windows
paths when passing these to CMake). The workflow records the exact configure
command and toolchain version.

The source asset includes the pinned standalone Cycles build system as
`upstream/cycles-standalone-<commit>.tar.gz`, its third-party source notices,
the Blender subset, and all dependency source archives. The repository's
`configure-cycles-portable.sh` reconstructs the overlay and applies the shipped
`cycles-msvc-guarded-allocator.patch`. Rebuild the dependency prefix, Cycles,
and host together when changing compiler or CRT options; mixing `/MT` and `/MD`
archives is unsupported. MSVC and the Windows SDK are toolchain prerequisites,
not redistributed in the source package.

Run `lmblender-windows-x64.exe --version` and `scripts/check-static.sh` under
Git Bash with `dumpbin` on PATH. The first version line must contain exactly
one semantic version token matching the release; renderer/library versions
appear on separate lines. The dependency check rejects VC redistributable and
third-party DLL imports. Every platform archive includes the license texts and
these relinking instructions; bare binaries also have adjacent notices,
source-offer, and relinking documents on the release page.
