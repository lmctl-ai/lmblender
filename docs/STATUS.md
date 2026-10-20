# Full-check status

Generated: 2026-10-20 15:53 UTC

Source revision tested: `36e2e80` plus tested worktree changes

## Results

- `BUILD_JOBS=2 ./test.sh`: PASS.
- Strict pixel and file checks: PASS; 12 gallery images are at least 640×480.
- Static linkage: PASS (`ldd` reports “not a dynamic executable” for both binaries).
- Binary sizes: `lmblender` 80784872 bytes; standalone Cycles 79072920 bytes.
- QA provider `agy/gemini-4-argon-medium`: PASS on all 12 named images; exact responses follow.

## QA responses

- gltf-json: PASS | score 5/5 | Exactly two separate upright panels on the left and right are fully inside the frame with a clear gap between them, and both display crisp red-and-yellow checker squares.
- glb: PASS | score 5/5 | Exactly two separate upright panels on the left and right are fully inside the frame with a clear gap between them, and both display crisp red-and-yellow checker squares.
- primitives: PASS | score 5/5 | Five distinct primitives (red cube, blue sphere, green cylinder, yellow cone, and magenta torus) are cleanly separated and fully framed over the checker floor, with distinguishable colors and smooth shading on the sphere, cylinder, and torus.
- environment: PASS | score 5/5 | Centered reflective metallic sphere clearly reflects the colored horizon with distinct warm yellow/orange and cool cyan/blue sky regions as well as the visible checker floor below it.
- sky: PASS | score 5/5 | Checker ground plane, sky horizon with a smooth brightness gradient from the warm horizon toward the cooler blue upper sky, and the centered amber sphere subject with balanced shading and no white clipping are all clearly visible.
- transparent: PASS | score 5/5 | One glossy blue sphere is fully inside the frame over a checker floor, surrounded by a clean transparent film background that is sharply distinct from the sphere.
- dof: PASS | score 5/5 | Three colored spheres (red, green, blue) appear in a diagonal row without clipping or overlap, with the middle green sphere in crisp focus and visibly much sharper than the blurred blue far sphere.
- fog: PASS | score 5/5 | A lit red sphere sits on a checkerboard floor inside a blue-tinted volumetric haze (with a visible volumetric spotlight cone), and the floor's checker contrast visibly fades with distance into the haze.
- subsurface: PASS | score 5/5 | A warm orange wax-like sphere is clearly visible and fully framed in the center, showing a smooth lit-to-shadow subsurface gradient rather than a flat silhouette.
- glass: PASS | score 5/5 | A clear glass sphere sits over a checkerboard floor with the checker pattern visibly refracted through its interior, localized highlights, and no conspicuous firefly specks.
- procedural: PASS | score 5/5 | Two distinct objects are visible inside the frame with their shadows, with a red/yellow checker material on the left object and blue procedural noise variation on the right object.
- image-texture: PASS | score 5/5 | The textured cube is fully visible inside the frame with red and yellow checker regions clearly appearing across all three visible faces.
