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
