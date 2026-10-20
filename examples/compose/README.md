# lmcad to lmblender composition

`./examples/compose/run.sh` builds two solids with lmcad, exports STL, imports those files into lmblender, and renders them with CPU Cycles. It writes `build/compose-mug.png` and `build/compose-plate.png`.

Set `LMCAD_SOURCE`, `LMCAD_BIN`, or `LMBLENDER_BIN` to override this host's defaults. The default lmcad executable is the static `/home/ubuntu/lmcad-poc-build/lmcad-build/lmcad`; no new OCCT build is required if it is already available.

STL transfers tessellated triangles only. STL has no material or object-name channel, so lmblender assigns a new material after import. Coordinates are interpreted in the source model's exported units; this demo assumes lmcad coordinates map directly to Cycles world units. `mug-render.lms` and `plate-render.lms` show the lmblender side of the pipeline.

## Composition QA and coordinates

The mug recipe maps lmcad's `revolve_y` (+Y-up) STL geometry to the renderer's +Z-up convention with `mesh.rotate_x(90)` and offsets its foot to z=0. STL itself contains no axis metadata, so this conversion is explicit in the example rather than guessed for arbitrary imports. `scene.fit_camera(42)` fits geometry bounds in a Z-up view. The mug uses a diffuse ceramic material, a large matte ground plane, and area key/fill lights. The plate uses a lit vertical backdrop behind the perforations so the holes read as open. `tests/compose_mug_quality.py` asserts a blue body mask, upright principal axis within 10 degrees, body luminance variation, visible dark cavity relative to the rim, and a floor region measurably darker than a lit patch. These checks are tied to this camera/layout and intentionally fail if composition drifts.
