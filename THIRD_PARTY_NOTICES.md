# Third-party notices and source obligations

The combined static executable is distributed under GPL-3.0-or-later because its audited link closure includes Blender `bf_intern_sky`, `bf_intern_libc_compat`, and `bf_intern_guardedalloc` archives licensed GPL-2.0-or-later. The Blender GPL and all component license files are included. See [docs/LICENSE_AUDIT.md](docs/LICENSE_AUDIT.md) for the exact closure and MIT-rebuild implications.

The `lmblender` release is a statically linked executable. This file records the principal bundled components; the corresponding upstream license texts shipped in `licenses/upstream/` are authoritative. Blender-derived helper objects in the standalone Cycles archives also make the Blender `COPYING` terms relevant. Review the exact link closure and license obligations before redistributing a changed build.

| Component | Use in binary | License text |
| --- | --- | --- |
| Blender intern sky, libc compat, guarded allocation, and GPL-derived color LUT table | `libbf_intern_sky.a`, `libbf_intern_libc_compat.a`, `libbf_intern_guardedalloc.a`, `cpp/view_luts.h` | `LICENSE`, `licenses/upstream/Blender-GPL-3.0-or-later.txt` |
| Cycles | CPU renderer and scene libraries | `licenses/upstream/Cycles-Apache-2.0.txt`, plus Blender source terms above for linked helpers |
| OpenImageDenoise 2.5 | CPU denoising | `licenses/upstream/OpenImageDenoise-Apache-2.0.txt` |
| ISPC-generated OIDN CPU kernels | OIDN CPU device routines generated from OIDN `.ispc` source; ISPC is a build-time compiler, not linked | OIDN Apache-2.0 applies to OIDN code; compiler provenance: `licenses/upstream/ISPC-BSD-3-Clause.txt` |
| OpenImageIO | PNG/EXR input and output | `licenses/upstream/OpenImageIO-BSD.txt` |
| OpenColorIO | color transforms | `licenses/upstream/OpenColorIO-license.txt` |
| OpenEXR and Imath | EXR support | `licenses/upstream/OpenEXR-license.rst`, `licenses/upstream/Imath-license.md` |
| oneTBB | task scheduling | `licenses/upstream/oneTBB-license.txt` |
| Embree 4.4.1 | CPU ray traversal and BVH | `licenses/upstream/Embree-Apache-2.0.txt` |
| libpng, libjpeg, zlib, zstd | image and compression support | matching files in `licenses/upstream/` |
| libtiff, OpenJPH | image and compression support | `licenses/upstream/libtiff-license.md`, `OpenJPH-license.txt` |
| pugixml, fmt, expat, pystring, yaml-cpp, minizip-ng | XML, formatting, OCIO/OIIO support | matching files in `licenses/upstream/` |
| sse2neon | Cycles x86-intrinsics compatibility on aarch64 | `licenses/upstream/sse2neon-MIT.txt` |
| cpp-dsl-sdk | vendored DSL v5 VM | `licenses/cpp-dsl-sdk-MIT.txt`; source pin in `sdk.lock` |
| nlohmann/json | vendored glTF JSON parser | `licenses/nlohmann-json-MIT.txt` |
| GNU libc, libstdc++, libgcc | static C/C++ runtime | `licenses/upstream/GCC-14-runtime-copyright.txt`, `licenses/upstream/LGPL-2.1.txt`; provide corresponding relinkable objects/source as required by their terms |

The static toolchain also includes libc, libstdc++, and libgcc runtime code; follow the LGPL/GCC runtime exception and relinking requirements in `docs/LICENSE_AUDIT.md` and `docs/RELINKING.md`. The build uses OpenImageIO's static plugin loader, which may call `dlopen` at runtime even though `ldd` reports `not a dynamic executable`. Each release ships its complete corresponding source as `lmblender-source-vX.Y.Z.tar.gz` (see `SOURCE_OFFER.md`); the `SOURCES.json` manifest inside it records the exact version, URL, hash, and license of every component in this table.
