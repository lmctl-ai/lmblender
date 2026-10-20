# Render feature QA

## Cycles sky texture (`examples/sky-texture.lms`)

- Render: 640×480 PNG, CPU Cycles, 16 samples, 35° sun elevation, 12° rotation, sky intensity 0.1.
- Numeric checklist (`tests/sky_quality.py`): exact 640×480 dimensions; mean RGB-channel luminance for rows 0–79 differs from rows 380–459 by at least 2/255; fewer than 5% of pixels have all RGB channels at or above 250.
- Latest measured result: top mean 188.76/255, horizon mean 14.55/255, difference 174.21/255; clipped pixel fraction below 5%.
- Visual inspection: broad blue/cyan upper sky fading to a dark horizon, centered tetra silhouette; the sky gradient is obvious and the render is not mostly clipped. The tetra is an emissive fallback, so this image verifies background sky output, not diffuse illumination from the sky.

Thumbnail: [sky-texture.png](img/sky-texture.png).

## Equirectangular environment and transparent film (`examples/environment-texture.lms`, `examples/transparent-film.lms`)

- Environment render: 640×480 PNG, CPU Cycles, 16 samples, equirectangular generated 128×64 four-color image, 22° rotation, strength 0.65.
- Numeric checklist (`tests/environment_quality.py`): dimensions are exactly 640×480; upper and lower 80-row bands differ by at least 20 total mean RGB levels.
- Measured upper RGB `(173.5,193.0,98.7)`, lower RGB `(42.4,193.4,197.5)`, L1 difference `230.2/255`.
- Visual inspection: the upper yellow band and lower cyan band are visible, with the tetra in frame. PNG is the regression format; the binding also accepts OIIO-decodable EXR/HDR within the same file and pixel limits.
- Transparent render: 640×480 RGBA PNG; `tests/transparent_alpha_quality.py` requires at least one-third transparent pixels and a nontrivial opaque object silhouette. Measured 284,685 transparent and 21,775 opaque pixels.
- Visual inspection: background is transparent (shown black by the viewer) and the tetra silhouette remains opaque. This verifies transparent world film, not shadow-catching compositing.

Thumbnails: [environment-texture.png](img/environment-texture.png), [transparent-film.png](img/transparent-film.png).

## Depth of field (`examples/camera-dof.lms`)

- Render: 640×480 PNG, CPU Cycles, 32 samples; a red object is placed at the configured focus distance and a blue object farther from the camera. Camera aperture is 1.5 and focus distance is 17.5.
- Numeric checklist (`tests/dof_quality.py`): exact 640×480 dimensions; computes two-pixel RGB edge contrast on each object's color mask and requires focal-plane contrast to exceed far-object contrast by 15%.
- Measured edge contrast: focus red `22.41`, far blue `19.19`, ratio `1.17` (passes `>=1.15`).
- Visual inspection: the red shape has a crisp boundary while the blue shape is visibly softened; both objects are in frame.
- Motion blur is not claimed: an early direct-Cycles animated-transform prototype produced a blank frame and was removed. It needs a valid Cycles motion-step setup before being exposed as a supported binding.

Thumbnail: [camera-dof.png](img/camera-dof.png).

## Volume, subsurface, and glass experiments (`examples/volume-fog.lms`, `examples/subsurface.lms`, `examples/glass-transmission.lms`)

All are 640×480 RGBA Cycles renders. The scripts use closed UV-sphere geometry and paired no-effect controls; `tests/material_effect_quality.py` compares RGB pixels and requires MAD >=0.35/255.

- Volume scatter test: same transparent sphere and lighting, with and without scatter density 8.0. Matched-control MAD is `0.001/255`, below the threshold. The volume node currently has no measurable render effect in this direct CCL setup, so volume rendering is **not verified/supported**. The high-density exploratory image has visible grain and is not a quality pass.
- Subsurface test: matched opaque control. Render RGB is constant `(38,43,56)` over the whole image, so the object disappears. The comparison MAD is not sufficient evidence; subsurface is **not verified/supported**.
- Glass/transmission test: transmission 1.0 against an otherwise matched opaque sphere. MAD is `13.175/255`. Visual inspection shows a faint translucent sphere against the lit environment; this is a functional transmission signal, not a Blender-parity claim.
- The comparison scripts and failed numeric measurements are retained deliberately to make the current limitations reproducible.

Thumbnails: [volume-fog.png](img/volume-fog.png), [subsurface.png](img/subsurface.png), [glass-transmission.png](img/glass-transmission.png).
