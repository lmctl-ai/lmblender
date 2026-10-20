# Full-check status

Generated: 2026-10-20 23:52 UTC

Source revision tested: `d098df6 plus tested worktree changes`

## Results

- `BUILD_JOBS=2 ./test.sh`: PASS.
- Video sequences: four 24-frame 640×360 sequences passed all per-frame numeric gates; ffmpeg encoding was attempted when available.
- Strict pixel and file checks: PASS; 14 gallery images are at least 640×480.
- Static linkage: PASS (`ldd` reports “not a dynamic executable” for both binaries).
- Binary sizes: `lmblender` 81230304 bytes; standalone Cycles 79138952 bytes.
- QA provider `agy/gemini-4-argon-medium`: PASS on all 18 named images; exact responses follow.

## QA responses

- gltf-json: PASS | score 5/5 | Exactly two separate upright checker-textured panels appear on the left and right, both fully inside the frame with a clear gap between them, and red/yellow checker squares are clearly visible on both.
- glb: PASS | score 5/5 | Exactly two separate upright checker-textured panels appear on the left and right, both fully inside the frame with a clear gap between them, and red/yellow checker squares are clearly visible on both.
- primitives: PASS | score 5/5 | All five distinct primitives (red cube, blue sphere, green cylinder, yellow cone, magenta torus) are separated, fully framed, non-overlapping, clearly color-distinguishable, and smoothly shaded on the curved forms.
- environment: PASS | score 5/5 | A reflective metallic sphere sits over a visible checkerboard floor and clearly reflects the colored horizon along with distinct warm (yellow/orange) and cool (cyan/blue) sky regions.
- sky: PASS | score 5/5 | Both the checkerboard ground plane and sky horizon are clearly visible, sky brightness grades smoothly from the bright horizon toward the upper blue sky, and the central sphere subject is well-exposed without white clipping.
- transparent: PASS | score 5/5 | One blue sphere is clearly visible and fully inside the frame against a transparent background that is cleanly distinct from the sphere and checkerboard floor.
- dof: PASS | score 5/5 | Three colored spheres (red, green, blue) appear in a row without overlapping or clipping, and the middle green sphere is crisply in focus while the far blue sphere is visibly blurred by depth of field.
- fog: PASS | score 5/5 | 1) A lit red sphere subject is clearly visible inside a blue-tinted volumetric haze; 2) the haze visibly reduces the checkerboard floor contrast with distance; 3) the image shows a well-lit sphere, volumetric spotlight cone, and checkerboard floor rather than an isolated blob. ([volume-fog.png](file:///home/ubuntu/lmblender/build/volume-fog.png))
- subsurface: PASS | score 5/5 | 1) A warm orange/amber wax-like sphere is clearly visible in the center; 2) the sphere shows a smooth lit-to-shadow subsurface scattering gradient across its surface; 3) it is 3D-shaded rather than a flat silhouette and is completely inside the frame. ([subsurface.png](file:///home/ubuntu/lmblender/build/subsurface.png))
- glass: PASS | score 5/5 | 1) A clear glass sphere is visibly positioned over a checkerboard floor; 2) the checkerboard pattern is cleanly refracted and inverted through the center of the sphere; 3) specular highlights are localized and clean with no conspicuous firefly specks. ([glass-transmission.png](file:///home/ubuntu/lmblender/build/glass-transmission.png))
- procedural: PASS | score 5/5 | 1) Two distinct triangular wedge objects are clearly visible; 2) the left object displays a red/yellow checker procedural material and the right displays a blue mottled noise variation; 3) both objects and their ground shadows are completely inside the frame. ([procedural-materials.png](file:///home/ubuntu/lmblender/build/procedural-materials.png))
- image-texture: PASS | score 5/5 | 1) The textured cube is completely visible inside the frame; 2) red and yellow checker regions from the image texture clearly appear across the top, front-left, and front-right faces; 3) the cube is richly textured rather than plain gray. ([image-texture.png](file:///home/ubuntu/lmblender/build/image-texture.png))
- material-maps: PASS | score 5/5 | 1) Exactly two smooth spheres sit separated on the ground plane and are fully framed; 2) the left orange/copper sphere exhibits visible irregular normal-map relief ripples and quadrant variations in metallic/roughness highlights; 3) the right sphere clearly alternates gold and blue in a texture-driven checker/stripe mix; 4) neither sphere overlaps or is cropped. ([material-maps.png](file:///home/ubuntu/lmblender/build/material-maps.png))
- backface-culling: PASS | score 5/5 | 1) One bright green square panel is clearly visible on the left; 2) the corresponding right panel area is empty dark gray because the red panel is viewed from its culled backface; 3) both left and right regions are unclipped inside the frame and the distinction is unmistakable. ([backface-culling.png](file:///home/ubuntu/lmblender/build/backface-culling.png))
- video-level1: PASS | score 5/5 | 1) Both CODER and RESEARCH (as well as PLANNER) orb teams have clearly readable labels; 2) every visible ring and satellite orb is completely inside the frame; 3) [level1-8.png](file:///home/ubuntu/lmblender/build/video/level1-8.png) is visibly closer/larger than [level1-0.png](file:///home/ubuntu/lmblender/build/video/level1-0.png); 4) the orb groups remain cleanly separated. ([level1-8.png](file:///home/ubuntu/lmblender/build/video/level1-8.png))
- video-level2: PASS | score 5/5 | 1) One central yellow LEAD orb and the cyan, green, pink, and purple agents are all visible; 2) the CODER, LEAD, and BROWSER labels are visible below their respective nodes; 3) connecting message arrows and the red-orange token are clearly visible; 4) the entire scene is well inside the frame. ([level2-12.png](file:///home/ubuntu/lmblender/build/video/level2-12.png))
- video-level3: PASS | score 5/5 | 1) HUMAN, LEAD, TEAM, and RESULT cards are all visible in left-to-right order; 2) the text on all four cards is readable; 3) white arrows clearly connect consecutive cards; 4) the red-orange message token is visible along the middle arrow without hiding any card labels. ([level3-12.png](file:///home/ubuntu/lmblender/build/video/level3-12.png))
- video-level4: PASS | score 5/5 | 1) INPUT, BUILD, REVIEW, and SHIP labels are visible in left-to-right order below the four stage orbs; 2) the green checkmark is large and completely inside the frame; 3) the green checkmark is positioned right beside the final SHIP stage; 4) the moving red-orange token is clearly visible at the final SHIP orb. ([level4-23.png](file:///home/ubuntu/lmblender/build/video/level4-23.png))

## Named preset catalog

- Added six versioned JSON presets embedded into the static binary; `--presets` and `--preset-show NAME` expose the data.
- Added DSL support for `use_preset`, version-1 `load_preset`, validated numeric overrides, and palette material creation. Preset changes preserve user meshes/material references while replacing generated lights and ground.
- `test.sh` renders six 320x240 denoised preset previews and checks exact dimensions, expected orange/blue palette masks, clipping bounds, luminance spread, and CLI JSON. The separate external-JSON and preset-switch examples also execute successfully.
- Inspected all six preview images: both diagram styles show colored forms on a dark field; studio and turntable show grounded soft-lit product forms; glass-showcase shows a translucent sphere and studio floor; technical-clean frames its orange part and neutral sphere orthographically.
- Preset numeric QA and image samples are in `tests/preset_quality.py`, `examples/presets/`, and `docs/img/preset-*.png`.
