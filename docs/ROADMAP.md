# Roadmap

| Area | Status | Evidence / remaining work |
|---|---|---|
| lmscript DSL v5 host and embedded schema | Usable | `scene.version()` and rendering examples; `./test.sh` checks schema embedding and the static ELF. |
| Static CPU Cycles executable | Usable | `scripts/configure-cycles-static.sh`; XML oracle, direct renderer, and `ldd: not a dynamic executable` gates pass. |
| OBJ/STL import and direct Cycles rendering | Usable | OBJ and ASCII/binary STL mesh imports; direct C++ nodes build meshes, objects, shaders, camera, lights, and world. |
| Materials and image textures | Partial | Principled, checker, noise, and bounded PNG/JPEG/EXR path binding; PNG checker fixture verifies face UVs. JPEG/EXR inputs, packed assets, and explicit color space controls are unverified. |
| Camera and transforms | Partial | Perspective/look-at and orthographic projection; fitted framing, translations, scales, and instance API. Panoramic optics/DOF are absent. |
| Image output | Partial | PNG supports embedded Blender 5.2 Standard/Filmic/AgX display LUTs; EXR remains scene-linear. Blender golden renders pass at RGB RMSE 0.80/255 (Standard), 0.73/255 (Filmic), and 0.80/255 (AgX). Separate depth/normal/albedo passes remain. |
| CPU denoising | Not linked | Static prefix contains no OIDN archive; host installation provides shared OIDN `.so` files only. A static-friendly OIDN rebuild or documented dynamic optional mode is needed. |
| Instance geometry sharing | Pending | Current renderer builds one Cycles mesh node per object. Prototype reuse rendered a blank frame and was reverted; memory-saving sharing needs a verified scene-node ownership/geometry-manager path. |
| lmcad composition gallery | Partial | Mug, plate-with-holes, and a three-angle U-channel bracket compose examples have numeric and visual QA in `docs/MILESTONE3-QA.md`. Bracket views are separate still renders, not an animated turntable. |
| Static XML oracle | Test-only | Retained to compare output against direct Cycles construction; production rendering uses the C++ node API. |
