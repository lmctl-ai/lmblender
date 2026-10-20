# Render feature QA

All feature-gallery outputs are 640×480 CPU Cycles PNGs. Numeric checks run from `test.sh`; checks are based on image pixels and reject missing/clipped subjects, flat outputs, fireflies, and the expected effect being absent.

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

A large low-density scattering volume encloses a lit coral subject over a checker floor; an area key and blue spot produce broad haze without a tight glass-dome outline. The no-volume control has the same lights and geometry but no shell or scatter node. `tests/material_effect_quality.py` compares the pair and checks the lit subject, blue-tinted haze, and distant-floor contrast attenuation. Latest measurements: pair MAD `36.936/255`, `5,264` lit red subject pixels, `54,736` blue haze pixels, blue-minus-red haze `28.6`, and far/near checker contrast ratios `0.723`/`0.812` relative to the control. The viewed render shows the sphere and cyan spot through visible blue-gray haze; the checker floor remains legible.

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
