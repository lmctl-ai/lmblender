# Render feature QA

All feature-gallery outputs are 640×480 CPU Cycles PNGs. Numeric checks run from `test.sh`; checks are based on image pixels and reject missing/clipped subjects, flat outputs, fireflies, and the expected effect being absent.

## OBJ/MTL materials, UVs, and per-face slots

`examples/obj-mtl-gallery.lms` imports two OBJ cubes with explicit corner UVs and normals. `usemtl` assigns a red checker/bump material to the left cube and a glossy blue material with an opacity map to the right. Flat-material and no-bump controls use the same geometry and lighting. `tests/obj_mtl_quality.py` requires more than 2,500 pixels of each color, image margins, a color-texture/control difference above 7/255, an opacity-map/control difference above 1/255, and a bump/no-bump difference above 0.5/255. Measured: 18,126 red and 21,524 blue pixels; texture MAD 69.96/255, opacity MAD 7.46/255, and bump MAD 4.34/255. An MTL path escaping the OBJ directory returns structured `OBJ_MTL_INVALID` before rendering; a huge coordinate returns `OBJ_INVALID`. I viewed the 640×480 gallery image: both cubes are fully framed, their color assignment is distinct, and the red checker follows the UVs.

Thumbnail: [obj-mtl-gallery.png](img/obj-mtl-gallery.png).

## glTF JSON and GLB mesh import

`examples/gltf-import.lms` and `examples/glb-import.lms` load the same fixture: two nodes reference one textured vertical quad mesh, translated to opposite sides. Both produce a 640×480 Cycles render with the external PNG checker multiplied by the glTF base-color factor. The numeric gate checks at least 2,500 colored surface pixels, at least 800 pixels on each side, a left/right subject centroid gap above 100 px, and at least three quantized color bins. Current results for each format: 32,869 subject pixels, 171.6 px centroid gap, and 39 color bins. `tests/gltf-invalid.lms` additionally asserts malformed GLB JSON returns structured `GLTF_INVALID` with a readable message.

| Fixture | Size | Colored subject pixels | Node centroid gap | Color bins | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| `docs/img/gltf-node-texture-import.png` (`.gltf` + external `.bin`/PNG) | 640×480 | 32,869 | 171.6 px | 39 | PASS |
| `docs/img/glb-node-texture-import.png` (GLB + external PNG) | 640×480 | 32,869 | 171.6 px | 39 | PASS |

## Built-in primitives (`examples/primitives-gallery.lms`)

The gallery creates a red cube, blue smooth UV sphere, green smooth cylinder, yellow cone, and magenta smooth torus directly through the DSL, plus a checker floor and area lights. The 96-sample render uses OIDN. `tests/primitives_quality.py` checks five distinct color masks, minimum silhouette area, image-edge margins, left-to-right ordering, and no silhouette overlap. The denoised row shows all five forms and their contact shadows; no OBJ fixture is used.

Thumbnail: [primitives-gallery.png](img/primitives-gallery.png).

## Environment reflection (`examples/environment-texture.lms`)

A smooth metallic UV sphere reflects the generated environment map, including its cyan horizon and yellow/orange sky bands; the checker floor provides a second reflection reference. The render is denoised. `tests/environment_quality.py` checks 640×480, RGB variance above 180 and more than 500 colors in the sphere region, plus a top-to-horizon color delta above 30. Latest sphere-region variance: `7908.9`; unique RGB colors: `9378`.

Thumbnail: [environment-texture.png](img/environment-texture.png).

## Sky and transparent film (`examples/sky-texture.lms`, `examples/transparent-film.lms`)

The sky scene uses a lit, smooth terracotta sphere on a checker floor under the procedural sky. The transparent-film scene uses a smooth blue sphere, contact shadow, and a small checker ground patch on RGBA transparent film. Numeric checks require a measurable top-to-horizon sky luminance difference, low clipping, at least one-third transparent pixels, and an in-frame sphere mask with projected width/height between 0.90 and 1.10. Latest sky row means are `188.01` at the top and `95.99` at the horizon (delta `92.02`); transparent film has `166,368` transparent and `139,829` opaque pixels, with sphere projected aspect `1.014`.

Thumbnails: [sky-texture.png](img/sky-texture.png), [transparent-film.png](img/transparent-film.png).

## Depth of field (`examples/camera-dof.lms`)

A red near sphere, green focus-plane sphere, and blue far sphere sit in a row over a checker floor. Denoising is enabled for the 96-sample render, and the key/fill energy is balanced so the floor is not clipped. `tests/dof_quality.py` measures mean boundary gradients per color mask and requires the focused sphere to be at least 15% sharper than both defocused spheres. Latest gradients: near red `18.91`, focused green `59.46`, far blue `3.34`.

Thumbnail: [camera-dof.png](img/camera-dof.png).

## Volume scatter (`examples/volume-fog.lms`)

A large low-density scattering volume encloses a lit coral subject over a checker floor; an area key and a blue spot behind the subject produce broad haze without a tight glass-dome outline. The no-volume control has the same lights and geometry but no shell or scatter node. The rear spot placement keeps the distant checker visibly more attenuated after removal of the implicit fallback light. `tests/material_effect_quality.py` compares the pair and checks the lit subject, blue-tinted haze, and distant-floor contrast attenuation. Latest measurements: pair MAD `38.268/255`, `5,626` lit red subject pixels, `50,486` blue haze pixels, blue-minus-red haze `28.0`, and far/near checker contrast ratios `0.596`/`0.806` relative to the control. The viewed render shows the sphere and cyan spot through visible blue-gray haze; the checker floor remains legible.

Thumbnail: [volume-fog.png](img/volume-fog.png).

## Subsurface-style translucency (`examples/subsurface.lms`)

A warm wax-colored smooth sphere uses Cycles' Principled subsurface weight/radius/scale with a warm backlight and broad area key. The paired opaque control shares the same material and lighting. QA requires pair MAD of at least `3/255`, more than 5,000 warm subject pixels, and a luminance range above 25 inside that color mask. Latest measurements: pair MAD `6.605/255`, `6,157` subject pixels, and `180.75` luminance levels of lit-to-shadow range. The viewed sphere has a broad lit-to-shadow transition; the example demonstrates Principled subsurface shading without claiming skin-realistic transmission.

Thumbnail: [subsurface.png](img/subsurface.png).

## Glass transmission (`examples/glass-transmission.lms`)

A clearer smooth glass sphere with a glossy highlight sits above the checker floor. Denoising and conservative direct/indirect clamp reduce fireflies. QA compares it with an opaque control, requires more than 1,000 unique colors in the refracted sphere region, core luminance standard deviation above 12, and fewer than 2% isolated near-white pixels. Latest measurements: pair MAD `10.412/255`, sphere-core checker standard deviation `38.45`, isolated bright pixels `0.000%`.

Thumbnail: [glass-transmission.png](img/glass-transmission.png).

## LMCAD composition reference

The gallery includes the independently modeled LMCAD mug import as a real production-shaped subject. STL triangle vertices are welded for smooth shading; the 640×480 denoised image has a visible opening, handle, soft key/fill lighting, and floor shadow.

Thumbnail: [compose-mug.png](img/compose-mug.png).

## Independent visual review

Opus reviewed all nine thumbnails directly on 2026-10-20 and marked each PASS against the scene-specific checklist. It confirmed the fog cone and subject are visible, the wax has a soft broad transition, the glass sphere refracts the checker without isolated fireflies, and the mug is upright with its opening and handle visible. Remaining polish notes: the mug has a visible rim crease, the generated environment is a simple yellow/cyan placeholder, the sky scene has a few floor sparkles, and the transparent-film checker plane has a visible edge. These are documented limitations, not numeric QA failures.

## BlenderCLI GLB bridge (`examples/bridge/`)

The BlenderCLI worker creates a red cube, blue sphere, and gold cone, then exports
world-space evaluated meshes and Principled base color, metallic, roughness, and
emission to GLB. lmblender imports the scene nodes and renders with a camera
matched to BlenderCLI's 50 mm metadata camera. The falsifiable comparison test
uses per-image corner-background subtraction, requires silhouette IoU above
`0.92`, and requires common-mask RGB MAD below `44/255`. Latest: IoU `0.9322`,
MAD `43.57/255`; the image has three distinct colored subjects. Visual review
shows good object placement and transferred colors, with expected lighting and
color-management differences. Camera/light/world data and parent hierarchy are
not included in the current export.

Thumbnail: [bridge-from-blendercli.png](img/bridge-from-blendercli.png).

## Strand helper (`examples/strands-showcase.lms`)

The left grass patch and right radial strand sculpture are generated by the
new `hair_patch`, `strand`, and `hair_sphere` APIs. The pixel gate requires
960×720, at least 700 green pixels, 1,000 gold pixels, and an in-frame mask.
Latest: 19,620 green and 61,054 gold pixels. My visual inspection confirms
the distinct tall curved blade, the tapered strands, and complete framing.
Independent Claude review passed the blade and framing checks but called the
gold silhouette a spiky burr rather than flowing fur. This is polygonal
illustrative hair, with no grooming/simulation or native Cycles curve hair.

Thumbnail: [strands-showcase.png](img/strands-showcase.png).

## Toon edges and profile loft (`examples/loft-outline.lms`)

A red cube receives a narrow dark stroke on each hard edge; a cyan square
profile follows a bent 3D path. Against an identical no-outline control,
the numeric gate measured 2,423 newly darkened red-edge pixels while retaining
28,458 red core pixels and 16,687 cyan loft pixels at 960×720. Claude visual
review passed all three checks (clean narrow strokes, connected untwisted
loft, complete framing), quality 4/5. The outline is a hard-edge stroke,
not view-dependent silhouette extraction for smooth shapes.

Thumbnail: [loft-outline.png](img/loft-outline.png).

## Point/area falloff (`examples/light-falloff.lms`)

Two red receivers share a point source and two blue receivers share a disk area
source. The numeric gate requires all four color masks and a near/far luminance
ratio above `1.18` for point and `1.12` for area. The rendered measurement is
recorded by `tests/light_falloff_quality.py`; this verifies the renderer's
distance response as well as the two source APIs.
