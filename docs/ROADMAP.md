# Roadmap

| Area | Status | Evidence / remaining work |
|---|---|---|
| lmscript DSL v5 host and embedded schema | Usable | `scene.version()` and rendering examples; `./test.sh` checks schema embedding and the static ELF. |
| Static CPU Cycles executable | Usable | `scripts/configure-cycles-static.sh`; XML oracle, direct renderer, and `ldd: not a dynamic executable` gates pass. |
| OBJ/STL import and direct Cycles rendering | Usable | OBJ and ASCII/binary STL mesh imports; direct C++ nodes build meshes, objects, shaders, camera, lights, and world. |
| Materials and image textures | Partial | Principled, checker, noise, and bounded PNG/JPEG/EXR path binding; PNG checker fixture verifies face UVs. JPEG/EXR inputs, packed assets, and explicit color space controls are unverified. |
| Camera and transforms | Partial | Perspective/look-at and orthographic projection; fitted framing, translations, scales, and instance API. Panoramic optics/DOF are absent. |
| Image output | Partial | PNG and linear EXR. Separate depth/normal/albedo passes and Standard/Filmic/AgX display transforms remain. |
| CPU denoising | Not linked | Static prefix contains no OIDN archive; host installation provides shared OIDN `.so` files only. A static-friendly OIDN rebuild or documented dynamic optional mode is needed. |
| Instance geometry sharing | Pending | Current renderer builds one Cycles mesh node per object. Prototype reuse rendered a blank frame and was reverted; memory-saving sharing needs a verified scene-node ownership/geometry-manager path. |
| lmcad composition gallery | Partial | Mug and plate-with-holes compose examples have numeric and visual QA in `docs/MILESTONE3-QA.md`. Bracket and multi-angle turntable compositions remain. |
| Static XML oracle | Test-only | Retained to compare output against direct Cycles construction; production rendering uses the C++ node API. |
