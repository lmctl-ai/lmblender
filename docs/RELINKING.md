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
