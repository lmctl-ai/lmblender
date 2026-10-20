# License audit: static host

Audit basis: the exact static link closure in `CMakeLists.txt`, the configured Cycles build, and the upstream file headers for Blender 5.2.2. This is a distribution engineering record, not legal advice. Re-audit the resolved archives whenever build flags or dependencies change.

## Conclusion

The combined `lmblender` executable is distributed under **GPL-3.0-or-later**. The link closure includes Blender intern libraries marked GPL-2.0-or-later. Those files do not say GPL-2.0-only; selecting GPL-3.0-or-later is therefore available. Cycles is Apache-2.0, which is compatible with GPL-3.0, but not with a GPL-2.0-only combined work. No GPL-2.0-only component was found in the audited link closure. The release must carry this notice and the full GPL-3.0 text in `LICENSE`.

The project-authored host and helper source files are kept MIT-licensed where individually marked. `cpp/view_luts.h` is deliberately segregated and marked GPL-3.0-or-later because it contains sampled output derived from the Blender 5.2 OCIO color configuration. The precise upstream OCIO configuration notice refers to `ocio-license.txt`, which is absent from the checked-out v5.2.2 source tree; treat the LUT as GPL-covered unless that source notice is recovered and reviewed.

## Linked components

| Component in this build | Evidence / source | License and shipped text | MIT-build implication |
| --- | --- | --- | --- |
| `libcycles_device`, `scene`, `session`, `bvh`, `subd`, `graph`, `util`, `integrator`, `kernel_cpu` | `CYCLES_BUILD_DIR/lib`; `intern/cycles` in Blender tag v5.2.2 | Apache-2.0; `licenses/upstream/Cycles-Apache-2.0.txt` | Replace/reimplement these Blender-derived renderer libraries or obtain an alternative license. |
| `libbf_intern_sky` | `intern/sky/CMakeLists.txt`, source SPDX headers | GPL-2.0-or-later; `licenses/upstream/Blender-GPL-3.0-or-later.txt` | Must be removed/replaced. |
| `libbf_intern_libc_compat` | `intern/libc_compat/CMakeLists.txt`, source SPDX headers | GPL-2.0-or-later; same Blender GPL text | Must be removed/replaced. |
| `libbf_intern_guardedalloc` | `intern/guardedalloc/CMakeLists.txt`, source SPDX headers | GPL-2.0-or-later; same Blender GPL text | Must be removed/replaced; replace allocator support too. |
| Blender `intern/atomic` support compiled into guardedalloc | `intern/guardedalloc` CMake closure and headers | Mixed Blender GPL wrapper and upstream-origin code notices; Blender GPL text is shipped | Must audit/replace with the allocator’s permissively licensed alternative. |
| `cpp/view_luts.h` | Generated from Blender `release/datafiles/colormanagement/config.ocio` and its Filmic/AgX transforms | Treated as GPL-3.0-or-later pending recovery of the config-referenced `ocio-license.txt`; Blender GPL text shipped | Replace LUT data with independently implemented / separately licensed transforms. |
| OpenImageIO and OIIO_Util | `LMBLENDER_STATIC_PREFIX/lib/libOpenImageIO*.a` | BSD-3-Clause; `licenses/upstream/OpenImageIO-BSD.txt` | Replace only if desired; no GPL constraint. Its static plugin loader uses `dlopen`, although `ldd` reports a static executable. |
| OpenColorIO | static prefix `libOpenColorIO.a` | BSD-3-Clause; `licenses/upstream/OpenColorIO-license.txt` | No GPL constraint. |
| OpenEXR, OpenEXRCore, Imath, IlmThread, Iex, OpenJPH | static prefix and `openjph` archives | BSD-family notices in `licenses/upstream/OpenEXR-license.rst`, `Imath-license.md`, and `OpenJPH-license.txt` | No GPL constraint. |
| OpenImageDenoise core and CPU device | `build/oidn-static-install/lib/libOpenImageDenoise*.a` | Apache-2.0; `licenses/upstream/OpenImageDenoise-Apache-2.0.txt` | No GPL constraint. |
| ISPC-generated OIDN CPU kernels | Generated from the OIDN CPU device `.ispc` sources by the build-time ISPC compiler; OIDN archives are linked, the compiler executable is not | OIDN Apache-2.0 applies to the OIDN kernel sources; the upstream ISPC compiler license is BSD-3-Clause and is included as `licenses/upstream/ISPC-BSD-3-Clause.txt` for build-tool provenance | No GPL constraint; re-audit if third-party ISPC snippets or a runtime library are introduced. |
| oneTBB | static prefix `libtbb.a` | Apache-2.0 with LLVM exception; `licenses/upstream/oneTBB-license.txt` | No GPL constraint. |
| zlib, libpng, libjpeg-turbo, libtiff, zstd | dependency/static prefix archives | zlib/libpng/IJG/BSD-style licenses; `licenses/upstream/zlib-license.txt`, `libpng-license.txt`, `libjpeg-license.md`, `libtiff-license.md`, `zstd-license.txt` | No GPL constraint in selected builds. |
| pugixml, fmt, expat, pystring, yaml-cpp, minizip-ng | dependency / OCIO extension archives | MIT/zlib/BSD-style; matching files in `licenses/upstream/` | No GPL constraint. |
| `sse2neon.h` | `${BLENDER_LIBDIR}/sse2neon` from the static dependency prefix (`/home/ubuntu/blender-deps/install/sse2neon` on this host) | MIT; `licenses/upstream/sse2neon-MIT.txt` | No GPL constraint; keep its notice with the build source. |
| cpp-dsl-sdk (`dslvm.hpp`) | vendored file pinned by `sdk.lock` | MIT; `licenses/cpp-dsl-sdk-MIT.txt` | No GPL constraint. |
| nlohmann/json | vendored headers in `third_party/nlohmann` | MIT; `licenses/nlohmann-json-MIT.txt` | No GPL constraint. |
| libc / libstdc++ / libgcc | static host link flags `-static -static-libstdc++ -static-libgcc` | glibc LGPL-2.1-or-later; GCC runtime components carry GPL-3.0-or-later with the GCC Runtime Library Exception where applicable. Debian GCC 14 copyright and exception text: `licenses/upstream/GCC-14-runtime-copyright.txt`; glibc text: `licenses/upstream/LGPL-2.1.txt`. | No GPL-3 constraint from runtime exceptions, but static-link relinking obligations remain. |

This list excludes non-linked Blender deps and features disabled for the static Cycles build (Python, GUI, OpenVDB, OpenSubdiv, Embree, OSL, LLVM, GPU backends). `ffmpeg` is only invoked externally by optional example scripts and is not linked into `lmblender`.

## Source separation and future MIT build

`cpp/main.cpp`, `cpp/gltf_import.hpp`, the DSL examples, and the video QA helper are marked MIT. The 5×7 extruded ASCII label glyphs are authored as geometry tables in `cpp/main.cpp`; no external font file or font data is bundled. Vendored SDK and JSON code remain under their own MIT notices. Blender-derived material is kept separate in `cpp/view_luts.h`; the two `patches/cycles-*.patch` files target Apache-licensed Cycles sources/CMake and are not copied into `blender-src` by release packaging. A future MIT combined binary must first replace all `bf_intern_*` GPL code, remove or replace the Blender-derived LUT data, and audit all new linked archives. No third-party code contribution should be accepted without a license/provenance note.

## Source and relinking

The release archive includes this audit, `LICENSE`, upstream license texts, and `docs/RELINKING.md`. Rebuild instructions pin Blender source and dependency options. For a public binary, attach the matching source archive or a durable source URL at the exact release tag, including overlay patches and build scripts; preserve complete license notices and provide the object/link materials required for the statically linked LGPL/GCC runtime components. The current locally built tarball is not yet a complete corresponding-source bundle; see the release gate in `docs/RELINKING.md` before publishing a public binary.
