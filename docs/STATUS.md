# Full-check status

Generated: 2026-10-20 22:37 UTC

Source revision tested: `4153841 plus tested worktree changes`

## Results

- `BUILD_JOBS=2 ./test.sh`: PASS.
- Video sequences: four 24-frame 640×360 sequences passed all per-frame numeric gates; ffmpeg encoding was attempted when available.
- Strict pixel and file checks: PASS; 14 gallery images are at least 640×480.
- Static linkage: PASS (`ldd` reports “not a dynamic executable” for both binaries).
- Binary sizes: `lmblender` 81003352 bytes; standalone Cycles 79138952 bytes.
- QA provider `agy/gemini-4-argon-medium`: PASS on all 18 named images; exact responses follow.

## QA responses

- gltf-json: PASS | score 5/5 | Exactly two separate upright checker-textured panels appear on the left and right, both fully inside the frame with a clear gap between them, and red/yellow checker squares are clearly visible on both.
- glb: PASS | score 5/5 | Exactly two separate upright checker-textured panels appear on the left and right, both fully inside the frame with a clear gap between them, and red/yellow checker squares are clearly visible on both.
- primitives: PASS | score 5/5 | All five distinct primitives (red cube, blue sphere, green cylinder, yellow cone, magenta torus) are separated, fully framed, non-overlapping, clearly color-distinguishable, and smoothly shaded on the curved forms.
- environment: PASS | score 5/5 | A reflective metallic sphere sits over a visible checkerboard floor and clearly reflects the colored horizon along with distinct warm (yellow/orange) and cool (cyan/blue) sky regions.
- sky: PASS | score 5/5 | Both the checkerboard ground plane and sky horizon are clearly visible, sky brightness grades smoothly from the bright horizon toward the upper blue sky, and the central sphere subject is well-exposed without white clipping.
- transparent: PASS | score 5/5 | One blue sphere is clearly visible and fully inside the frame against a transparent background that is cleanly distinct from the sphere and checkerboard floor.
- dof: PASS | score 5/5 | Three colored spheres (red, green, blue) appear in a row without overlapping or clipping, and the middle green sphere is crisply in focus while the far blue sphere is visibly blurred by depth of field.
- fog: PASS | score 5/5 | A lit red sphere sits on a checkerboard floor inside a blue-tinted volumetric haze, and the floor's checker contrast visibly fades with distance into the haze.
- subsurface: PASS | score 5/5 | A warm orange wax-like sphere is clearly visible and fully framed in the center, showing a smooth lit-to-shadow subsurface gradient rather than a flat silhouette.
- glass: PASS | score 5/5 | A clear glass sphere sits over a checkerboard floor with the checker pattern visibly refracted through its interior, localized highlights, and no conspicuous firefly specks.
- procedural: PASS | score 5/5 | Two distinct objects are visible inside the frame with their shadows, with a red/yellow checker material on the left object and blue procedural noise variation on the right object.
- image-texture: PASS | score 5/5 | The textured cube is fully visible inside the frame with red and yellow checker regions clearly appearing across all three visible faces.
- material-maps: PASS | score 5/5 | Two smooth spheres sit apart and fully framed without overlapping or cropping; the left orange sphere displays irregular normal-map relief and varying metallic/roughness highlights across its surface, while the right sphere alternates gold and blue regions from a texture-driven shader mix.
- backface-culling: PASS | score 5/5 | One bright green square panel is clearly visible and unclipped on the left while the corresponding right panel area is empty gray background due to backface culling.
- video-level1: PASS | score 5/5 | Both the CODER and RESEARCH orb teams have readable labels, every visible ring and satellite orb is fully inside the frame, the peek view is visibly closer than level1-0.png, and the orb groups remain clearly separated.
- video-level2: PASS | score 5/5 | The central yellow LEAD orb and the surrounding cyan, green, pink, and purple agents are clearly visible, the "CODER", "LEAD", and "BROWSER" labels are readable, the message arrows and red-orange token are clearly visible, and the entire scene is fully framed.
- video-level3: PASS | score 5/5 | The "HUMAN", "LEAD", "TEAM", and "RESULT" cards are all clearly visible in left-to-right order with readable text, connected by arrows between consecutive cards, and the red-orange message token is clearly visible on the middle arrow without hiding any card labels.
- video-level4: PASS | score 5/5 | The "INPUT", "BUILD", "REVIEW", and "SHIP" labels are clearly visible in left-to-right order, the large green checkmark is fully inside the frame next to the final "SHIP" stage, and the moving red-orange token is clearly visible at the end stage.
