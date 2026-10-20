# Render feature QA

## Cycles sky texture (`examples/sky-texture.lms`)

- Render: 640×480 PNG, CPU Cycles, 16 samples, 35° sun elevation, 12° rotation, sky intensity 0.1.
- Numeric checklist (`tests/sky_quality.py`): exact 640×480 dimensions; mean RGB-channel luminance for rows 0–79 differs from rows 380–459 by at least 2/255; fewer than 5% of pixels have all RGB channels at or above 250.
- Latest measured result: top mean 188.76/255, horizon mean 14.55/255, difference 174.21/255; clipped pixel fraction below 5%.
- Visual inspection: broad blue/cyan upper sky fading to a dark horizon, centered tetra silhouette; the sky gradient is obvious and the render is not mostly clipped. The tetra is an emissive fallback, so this image verifies background sky output, not diffuse illumination from the sky.

Thumbnail: [sky-texture.png](img/sky-texture.png).
