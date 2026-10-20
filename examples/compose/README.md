# lmcad to lmblender composition

`./examples/compose/run.sh` builds three solids with lmcad, exports STL, imports those files into lmblender, and renders them with CPU Cycles. It writes mug and plate renders, three bracket camera studies, plus eight-angle mug and bracket sets (`build/compose-{mug,bracket}-angle-0.png` through `-7.png`). `tests/turntable_quality.py` checks every image's dimensions, nonuniform content, visible object coverage, and distinct adjacent camera views.

The `studio-mug.lms`, `studio-bracket.lms`, and `studio-plate.lms` examples exercise `scene.studio_setup()`: after import (and any source-axis correction), it computes the transformed mesh bounds, fits the perspective camera, places a matte floor below the model, sets neutral world fill, and adds warm key, cool fill, and rear area lights. Run them with `./examples/compose/run.sh`; each output is 640×480 and checked for colored subject coverage, a nontrivial luminance range, and a minimum mean luminance by `tests/studio_quality.py`.

Set `LMCAD_SOURCE`, `LMCAD_BIN`, or `LMBLENDER_BIN` to override this host's defaults. The default lmcad executable is the static `/home/ubuntu/lmcad-poc-build/lmcad-build/lmcad`; no new OCCT build is required if it is already available.

STL transfers tessellated triangles only. STL has no material or object-name channel, so lmblender assigns a new material after import. Coordinates are interpreted in the source model's exported units; this demo assumes lmcad coordinates map directly to Cycles world units. `mug-render.lms`, `plate-render.lms`, and `bracket-render.lms` show the lmblender side of the pipeline. The turntable examples use eight sequential camera poses, rendered as individual stills rather than an encoded animation.

## Composition QA and coordinates

The mug recipe maps lmcad's `revolve_y` (+Y-up) STL geometry to renderer +Z-up with `mesh.rotate_x(90)` and offsets its foot to z=0. STL has no axis metadata, so conversion is explicit. `scene.fit_camera(38)` fits transformed bounds at render aspect. This particular lmcad mug receives a 1.2x scale in its horizontal plane to reach a normal mug profile; this is an explicit gallery adjustment, not an import default. A broad area key/fill, soft sun and world ambient illuminate the diffuse ceramic over a matte floor. `tests/compose_mug_quality.py` compares the projected image silhouette aspect to the adjusted source X/Y spans, checks upright angle, body luminance mean and p5-p95 spread, opening/rim contrast, and lit-floor/shadow contrast. The plate uses a lit vertical backdrop behind perforations.

`tests/compose_bracket_quality.py` checks image dimensions, steel-color coverage, and luminance contrast for each camera view. These numeric checks do not prove the shape is correct; inspect the images for recognizable U geometry and framing. Opus visual review results are recorded in `docs/MILESTONE3-QA.md`.
