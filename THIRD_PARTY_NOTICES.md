# Third-party notices and source obligations

The `lmblender` release is a statically linked executable. This file records the principal bundled components; the corresponding upstream license texts shipped in `licenses/upstream/` are authoritative. Blender-derived helper objects in the standalone Cycles archives also make the Blender `COPYING` terms relevant. Review the exact link closure and license obligations before redistributing a changed build.

| Component | Use in binary | License text |
| --- | --- | --- |
| Blender source helpers and guarded allocator | Blender-derived Cycles support archives | `licenses/upstream/Blender-GPL-3.0-or-later.txt` |
| Cycles | CPU renderer and scene libraries | `licenses/upstream/Cycles-Apache-2.0.txt`, plus Blender source terms above for linked helpers |
| OpenImageDenoise 2.5 | CPU denoising | `licenses/upstream/OpenImageDenoise-Apache-2.0.txt` |
| OpenImageIO | PNG/EXR input and output | `licenses/upstream/OpenImageIO-BSD.txt` |
| OpenColorIO | color transforms | `licenses/upstream/OpenColorIO-license.txt` |
| OpenEXR and Imath | EXR support | `licenses/upstream/OpenEXR-license.rst`, `licenses/upstream/Imath-license.md` |
| oneTBB | task scheduling | `licenses/upstream/oneTBB-license.txt` |
| libpng, libjpeg, zlib, zstd | image and compression support | matching files in `licenses/upstream/` |
| pugixml, fmt, expat | XML, formatting, and support | matching files in `licenses/upstream/` |
| cpp-dsl-sdk | vendored DSL v5 VM | `licenses/cpp-dsl-sdk-MIT.txt`; source pin in `sdk.lock` |
| nlohmann/json | vendored glTF JSON parser | `licenses/nlohmann-json-MIT.txt` |
| GNU libc, libstdc++, libgcc | static C/C++ runtime | `licenses/upstream/GPL-3.txt` and `licenses/upstream/LGPL-2.1.txt`; provide corresponding relinkable objects/source as required by their terms |

The build additionally links TIFF, OpenJPH, minizip-ng, yaml-cpp, pystring, and other support code pulled by the configured OpenImageIO/OpenColorIO builds. Their notices are carried by the upstream dependency source trees and should be added to `licenses/upstream/` when those build options change. This release was built for Linux aarch64 with the pinned source/dependency configuration documented in `docs/RELINKING.md`.
