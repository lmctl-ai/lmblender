# Corresponding source

Every `lmblender` binary release (`vX.Y.Z` at <https://github.com/lmctl-ai/lmblender/releases>) is a combined work distributed under GPL-3.0-or-later (see `LICENSE` and `docs/LICENSE_AUDIT.md`). Each release page carries, next to the binaries, the complete corresponding source as one asset:

`lmblender-source-vX.Y.Z.tar.gz` (with `.sha256`), built by `scripts/release-source.sh` in the CI `source` job, containing:

- `lmblender/`: the repository tree at the release commit (`GIT_COMMIT` inside names it): the host C++ source, DSL schema and presets, the vendored DSL VM (`cpp/dslvm.hpp`, pinned by `sdk.lock`), the vendored nlohmann/json headers, the Cycles patches under `patches/`, every build script, and all license texts.
- `upstream/blender-v5.2.2-<commit>-subset.tar.gz`: the exact Blender source subset the build compiles (Cycles, `intern/sky`, `intern/guardedalloc`, `intern/libc_compat`, `intern/atomic`, `intern/mikktspace`, `build_files/`), taken with `git archive` from the commit `scripts/fetch-cycles-source.sh` verifies. This covers the GPL-2.0-or-later Blender intern archives linked into the binary.
- `upstream/cycles-overlay-patched-<commit>.tar.gz`: the patched `intern/cycles` tree the static Cycles archives are actually built from (output of `scripts/make-cycles-source-overlay.py`).
- `upstream/cycles-standalone-<commit>.tar.gz`: the pinned standalone Cycles build system and third-party sources used for Windows. The repository includes the overlay script and additional MSVC patch that reconstruct the Windows build tree.
- `upstream/<package>`: the pinned source archive of every linked third-party library and of the build-only helpers Blender's `build_environment` needs (oneTBB, OpenEXR, Imath, OpenImageIO, OpenColorIO, OpenImageDenoise, pugixml, robin-map, zlib, libpng, libjpeg-turbo, libtiff, zstd, fmt, expat, yaml-cpp, pystring, minizip-ng, OpenJPH, OpenJPEG, Eigen, sse2neon, brotli, libepoxy, FreeType, bzip2; Embree whenever `WITH_CYCLES_EMBREE=ON`). Each file is verified against the hash Blender pins in `build_files/build_environment/cmake/versions.cmake` (or, for OIDN, the SHA-256 in `scripts/bootstrap-static-deps.sh`) before it is packed.
- `SOURCES.json` and `SOURCES.md`: the manifest with version, upstream URL, Blender mirror URL, hash, license, and role of every component, plus the toolchain description. ISPC is listed with hashes as a build-time compiler; it is not linked and its binaries are not re-shipped.

`BUILD.md` documents the build; `docs/RELINKING.md` covers relinking the statically linked C/C++ runtime. The CI release job refuses to publish binaries when the source asset is missing or does not match the tag (`.github/workflows/ci.yml`, job `release`), and `scripts/release.sh --publish` enforces the same locally.

## Written offer

GNU libc, libstdc++, and libgcc are System Libraries in the sense of GPL-3.0 section 1 and are taken from the Ubuntu 24.04 `gcc-14` and `libc6-dev` packages named in `SOURCES.json`. If any other component listed in `SOURCES.json` is missing from a release's source asset, or you cannot download it from the recorded URLs, open an issue at <https://github.com/lmctl-ai/lmblender/issues> naming the release tag and component. We will provide a copy of that source, for the cost of physically performing the distribution, to anyone who possesses the object code, for at least three years after the release date (GPL-3.0 section 6(b)).
