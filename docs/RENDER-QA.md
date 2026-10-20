# Render feature QA

All feature-gallery outputs are 640×480 CPU Cycles PNGs. Numeric checks run from `test.sh`; checks are based on image pixels and reject missing/clipped subjects, flat outputs, fireflies, and the expected effect being absent.

## Built-in primitives (`examples/primitives-gallery.lms`)

The gallery creates a red cube, blue smooth UV sphere, green smooth cylinder, yellow cone, and magenta smooth torus directly through the DSL, plus a checker floor and area lights. `tests/primitives_quality.py` checks five distinct color masks, minimum silhouette area, image-edge margins, left-to-right ordering, and no silhouette overlap. The rendered row shows all five forms and their shadows; no OBJ fixture is used.

Thumbnail: [primitives-gallery.png](img/primitives-gallery.png).

## Environment reflection (`examples/environment-texture.lms`)

A smooth metallic UV sphere reflects the generated four-band environment map, including its cyan horizon and yellow/orange upper band; the checker floor provides a second reflection reference. `tests/environment_quality.py` checks 640×480, RGB variance above 180 and more than 500 colors in the sphere region, plus a top-to-horizon color delta above 30. Latest sphere-region variance: `8380.1`; unique RGB colors: `9855`.

Thumbnail: [environment-texture.png](img/environment-texture.png).

## Sky and transparent film (`examples/sky-texture.lms`, `examples/transparent-film.lms`)

The sky scene uses a lit, smooth terracotta sphere under the procedural sky. The transparent-film scene uses a smooth blue sphere on RGBA transparent film. Numeric checks require a strong top-to-horizon sky luminance difference, low clipping, at least one-third transparent pixels, and an in-frame opaque sphere silhouette. Latest sky row means are `188.76` at the top and `3.60` at the horizon (delta `185.16`); transparent film has `274,251` transparent and `32,292` opaque pixels.

Thumbnails: [sky-texture.png](img/sky-texture.png), [transparent-film.png](img/transparent-film.png).

## Depth of field (`examples/camera-dof.lms`)

A red near sphere, green focus-plane sphere, and blue far sphere sit in a row over a checker floor. Denoising is enabled for the 96-sample render. `tests/dof_quality.py` measures mean boundary gradients per color mask and requires the focused sphere to be at least 15% sharper than both defocused spheres. Latest gradients: near red `24.20`, focused green `73.20`, far blue `3.83`.

Thumbnail: [camera-dof.png](img/camera-dof.png).

## Volume scatter (`examples/volume-fog.lms`)

A translucent scattering sphere encloses a lit coral subject over a checker floor; an area key and blue spot produce visible colored haze. The paired no-volume control is the same scene without the scatter node. `tests/material_effect_quality.py` compares the pair and requires a non-flat effect, at least 1,500 lit red subject pixels, and at least 10,000 blue haze pixels in the shell region. Latest measurements: pair MAD `21.549/255`, `10,745` lit subject pixels, and `44,662` blue haze pixels. The checker remains visible through the low-density shell.

Thumbnail: [volume-fog.png](img/volume-fog.png).

## Subsurface-style translucency (`examples/subsurface.lms`)

A warm wax-colored smooth sphere uses a translucent BSDF mixed with its Principled surface, with a warm backlight and soft area key. This is a visible translucency approximation; it does not claim the Principled BSSRDF implementation. The paired no-translucency control shares the same scene. QA requires a measurable pair difference, more than 5,000 warm subject pixels, and a luminance range above 25 inside that color mask. Latest measurements: pair MAD `1.896/255`, `35,269` subject pixels, and `186.77` luminance levels of lit-to-shadow range.

Thumbnail: [subsurface.png](img/subsurface.png).

## Glass transmission (`examples/glass-transmission.lms`)

A smooth glass sphere with a glossy highlight sits above the checker floor. Denoising and conservative direct/indirect clamp reduce fireflies. QA compares it with an opaque control, requires more than 1,000 unique colors in the refracted sphere region, core luminance standard deviation above 12, and fewer than 2% isolated near-white pixels. Latest measurements: pair MAD `8.867/255`, sphere-core checker standard deviation `27.97`, isolated bright pixels `0.000%`.

Thumbnail: [glass-transmission.png](img/glass-transmission.png).

## LMCAD composition reference

The gallery includes the independently modeled LMCAD mug import as a real production-shaped subject, with a visible opening, handle, soft key/fill lighting, and floor shadow.

Thumbnail: [compose-mug.png](img/compose-mug.png).
