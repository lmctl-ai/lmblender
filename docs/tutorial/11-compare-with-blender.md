# 11. Compared with Blender

Goal: show, with measured numbers, how close lmblender's renders are to Blender's for the same scenes, what the comparison found wrong in lmblender, and what still differs.

Every top-level example in [examples/](../../examples/) was rendered twice: once by lmblender, and once by Blender 5.2.2 (Cycles, CPU) from a Python script that builds the same scene with `bpy`. Each example has its own page with both images side by side, the Python port, the timings, the commands, and the differences found.

![procedural-materials: lmblender left, Blender right](blender/images/procedural-materials--procedural-materials.jpg)

Left: lmblender. Right: Blender. Mean difference 0.94 of 255, close to the median pair (0.52); the table below lists every example.

## How the comparison was made

- The Blender side uses [lmb_bpy.py](blender/py/lmb_bpy.py), a bpy module with one method per lmblender call. It follows the host's C++ code: the same vertex lists for primitives, strands, lofts, and outlines; the same light orientation, units, and camera matrix; the same Principled BSDF settings; textures in object coordinates; the same keyframe interpolation; still renders that ignore keys; and Cycles settings matched to the host (Standard view, box filter, bounces, seed, light clamps, adaptive sampling).
- Each port in [blender/py/](blender/py/) was generated from the example's `_dsl_` block by [lms2py.py](blender/lms2py.py), then checked by hand.
- "Mean difference" is the mean absolute RGB difference between the two PNGs, in 0 to 255 steps. "Silhouette overlap" is the intersection over union of the pixels that differ from the corner background color.
- lmblender was built from this repository with the fixes listed below; a release after v0.3.4 is needed before `lmscript`'s `install=...@latest` downloads them. Host: Linux arm64, four-core Neoverse-N1 (AWS Graviton2), four render threads on both sides.
- Timings are single wall-clock runs of the whole program, including about 0.8 s of Blender startup and lmscript compile time, measured one example at a time; the regression suite sometimes ran on the same machine during this run, so compare the two numbers within a row, not across rows.

## Summary

- 44 examples ported; all 44 ran on both sides.
- 39 examples produce PNG images. Up to three images per example were compared (first, middle, and last frame for sequences), 51 pairs in all. The median mean difference is 0.52 of 255 and the worst is 5.53.
- Total time: lmblender 796 s, Blender 689 s. Small scenes take about one second on both sides. lmblender was faster on `embree-heavy` (3.7 s against 11.6 s) and slower on `motion-blur` (74.2 s against 16.4 s), where it renders nine full temporal samples.

| Example | lmblender s | Blender s | Mean difference /255 | Silhouette overlap |
|---|---|---|---|---|
| [animation-tween](blender/animation-tween.md) | 21.3 | 21.7 | 1.06 | 0.98 |
| [animation-visibility-optics](blender/animation-visibility-optics.md) | 5.0 | 6.1 | 0.31 | 0.95 |
| [backface-culling](blender/backface-culling.md) | 1.3 | 2.0 | 0.33 | 1.00 |
| [camera-dof](blender/camera-dof.md) | 15.1 | 13.8 | 2.28 | 0.97 |
| [camera-look-at](blender/camera-look-at.md) | 0.5 | 1.2 | 0.33 | 1.00 |
| [denoise-quality](blender/denoise-quality.md) | 2.0 | 2.5 | 2.97 | 0.97 |
| [display-transforms](blender/display-transforms.md) | 0.5 | 1.3 | 0.32 | not meaningful (flat panel) |
| [embree-heavy](blender/embree-heavy.md) | 3.7 | 11.6 | 2.56 | 0.96 |
| [environment-texture](blender/environment-texture.md) | 35.3 | 52.8 | 2.09 | 1.00 |
| [glass-opaque-control](blender/glass-opaque-control.md) | 54.5 | 39.1 | 0.79 | 0.98 |
| [glass-transmission](blender/glass-transmission.md) | 54.1 | 45.0 | 1.22 | 0.98 |
| [glb-import](blender/glb-import.md) | 2.4 | 2.4 | 0.31 | 1.00 |
| [gltf-emission](blender/gltf-emission.md) | 1.5 | 3.4 | 0.41 | 1.00 |
| [gltf-import](blender/gltf-import.md) | 2.6 | 3.6 | 0.31 | 1.00 |
| [image-texture](blender/image-texture.md) | 27.0 | 25.5 | 2.47 | 0.99 |
| [import-mesh](blender/import-mesh.md) | 0.4 | 1.2 | no image | no image |
| [import-ply](blender/import-ply.md) | 0.6 | 1.4 | 0.34 | 1.00 |
| [light-falloff](blender/light-falloff.md) | 19.9 | 18.1 | 0.89 | 0.97 |
| [loft-outline](blender/loft-outline.md) | 30.1 | 29.6 | 0.40 | 1.00 |
| [material-maps](blender/material-maps.md) | 59.2 | 47.2 | 0.42 | 1.00 |
| [material-nodes-control](blender/material-nodes-control.md) | 20.1 | 17.9 | 0.33 | 1.00 |
| [material-nodes](blender/material-nodes.md) | 20.9 | 18.2 | 0.34 | 1.00 |
| [material-principled](blender/material-principled.md) | 0.8 | 2.0 | 0.33 | 1.00 |
| [mesh-instances](blender/mesh-instances.md) | 1.1 | 1.8 | 0.33 | 0.98 |
| [motion-blur](blender/motion-blur.md) | 74.2 | 16.4 | 0.87 | 0.99 |
| [obj-mtl-gallery](blender/obj-mtl-gallery.md) | 11.9 | 10.8 | 3.23 | 0.99 |
| [orthographic-camera](blender/orthographic-camera.md) | 26.2 | 23.9 | 0.48 | 1.00 |
| [primitives-gallery](blender/primitives-gallery.md) | 28.1 | 23.9 | 0.88 | 0.98 |
| [procedural-materials](blender/procedural-materials.md) | 20.2 | 18.4 | 0.94 | 1.00 |
| [render-exr](blender/render-exr.md) | 0.7 | 1.4 | no image | no image |
| [render-mesh-xml-oracle](blender/render-mesh-xml-oracle.md) | 0.6 | 2.0 | 0.33 | 1.00 |
| [render-mesh](blender/render-mesh.md) | 1.0 | 1.6 | 0.33 | 1.00 |
| [render-passes](blender/render-passes.md) | 0.7 | 2.0 | no image | no image |
| [render-settings](blender/render-settings.md) | 0.6 | 1.5 | 0.34 | 0.99 |
| [render-timeout](blender/render-timeout.md) | 11.8 | 3.0 | no image | no image |
| [sky-texture](blender/sky-texture.md) | 22.2 | 24.3 | 5.53 | 0.98 |
| [spot-falloff](blender/spot-falloff.md) | 16.4 | 12.2 | 0.48 | 0.98 |
| [strands-showcase](blender/strands-showcase.md) | 45.2 | 45.8 | 1.52 | 0.98 |
| [subsurface-control](blender/subsurface-control.md) | 21.1 | 16.9 | 1.16 | 1.00 |
| [subsurface](blender/subsurface.md) | 33.3 | 31.5 | 1.44 | 1.00 |
| [transparent-film](blender/transparent-film.md) | 11.0 | 16.0 | 0.52 | 1.00 |
| [version](blender/version.md) | 0.3 | 1.1 | no image | no image |
| [volume-control](blender/volume-control.md) | 24.5 | 20.3 | 0.97 | 0.99 |
| [volume-fog](blender/volume-fog.md) | 66.0 | 46.8 | 1.65 | 1.00 |

## What the comparison found in lmblender

The first version of this page compared Blender with the v0.3.4 release and found the problems below. All are fixed in this repository; the numbers above come from the fixed build.

1. Denoiser crash on arm64 CPUs without SVE. The v0.3.3 and v0.3.4 linux-arm64 releases stop with `Illegal instruction` in OpenImageDenoise when denoising, on CPUs such as AWS Graviton2. ISPC had tuned the kernels for the CI runner's CPU, which has SVE. `scripts/build-oidn-static.sh` now pins a baseline CPU on Linux and CI runs `scripts/check-no-sve.sh` on the arm64 executable.
2. Dark tones rendered too dark. The display lookup table's first step was at 0.093 linear, and interpolating across it undershot the sRGB curve by up to 20 of 255. The table now uses a log1p(64 v) shaper (mean error about 0.1 to 0.4 of 255 against OpenColorIO), and the Standard view is computed exactly.
3. PNG and JPEG color textures were not decoded from sRGB, because lmblender runs Cycles without an OpenColorIO config. Image and environment textures in 8- or 16-bit formats are now tagged so Cycles decodes them.
4. The fallback camera was squeezed horizontally when a scene sets no camera. It now gets the same aspect correction as explicit cameras.
5. Example scripts printed only standard error, so failures showed `lmblender exit 3:` with no message. They now print the host's JSON error, print the result on success, and allow renders longer than lmscript's default 60 second `system()` limit, which had stopped `motion-blur` and `volume-fog` on a loaded machine. Two examples that needed files generated by `test.sh` now use committed fixtures, and the `motion-blur` control image now uses the frame 2 pose it was meant to show.

Before and after for the examples these fixes changed most:

| Example | Before (v0.3.4 code) | After | Main cause |
|---|---|---|---|
| material-principled | 10.44 | 0.33 | dark tones and fallback camera |
| gltf-import | 19.40 | 0.31 | sRGB texture |
| glb-import | 19.40 | 0.31 | sRGB texture |
| environment-texture | 19.33 | 2.09 | sRGB environment map |
| primitives-gallery | 2.82 | 0.88 | dark tones |
| procedural-materials | 10.95 | 0.94 | dark tones |
| sky-texture | 14.21 | 5.53 | dark tones |
| spot-falloff | 9.73 | 0.48 | dark tones |

The BlenderCLI bridge example gives the same picture: rendered by Blender 5.2 with the same scene, the bridge GLB now differs by 0.86 of 255 (10.51 before the fixes); `test.sh` checks this with a matched-rig reference.

## What still differs

1. `motion-blur`: Blender's motion blur interpolates the baked subframe keys while lmblender renders nine temporal samples, so the streaks differ (mean difference up to 2.96).
2. `sky-texture`: 5.53 of 255. Both use the multiple-scattering sky and the images look alike, but lmblender shows bright sparkles of the sun disc on the glossy floor that Blender does not; Blender likely samples the sky by importance where the standalone setup does not. This was not investigated further.
3. `obj-mtl-gallery`: Blender's OBJ importer converts MTL materials itself, so the materials are close but not identical (3.23).
4. Blender has no APNG writer, so `render_apng` becomes a PNG sequence.
5. `render_passes` becomes one multilayer EXR plus a combined PNG, not lmblender's four EXR and PNG pairs; the pass images and the EXR outputs were not compared numerically.
6. `render_timeout` has no true equivalent: Blender's time limit stops sampling and saves an image, while lmblender cancels and reports `RENDER_TIMEOUT`.
7. Blender's glTF importer splits vertices differently, so the vertex-count checks in the glTF examples report 8 where lmblender reports 4.

## Not compared

The examples in `examples/presets/`, `examples/video/`, `examples/compose/` (needs lmcad), `examples/bridge/` (compared through `test.sh` instead), and `examples/dsl/` were not ported. Presets and the video scenes use calls (`use_preset`, `label`, `arrow_between`, `ring_arc`) that the bpy module does not implement yet.

## Reproduce

From the repository root, with lmscript and a Blender 5.x executable on your PATH:

```sh
mkdir -p build
lmscript examples/camera-look-at.lms
blender -b --factory-startup --python docs/tutorial/blender/py/camera-look-at.py
```

lmblender writes `build/camera-look-at.png` and Blender writes `blender/camera-look-at.png`; each example page shows the expected console output. Until a release after v0.3.4 is published, `lmscript` downloads v0.3.4, which still has the issues above; put an lmblender built from this repository ([BUILD.md](../../BUILD.md)) first on your PATH to reproduce these numbers.

Next: write your own scene, starting from page 6 or 7, and look up every call in [LLM_GUIDE.md](../LLM_GUIDE.md). Back to the [tutorial index](index.md).
