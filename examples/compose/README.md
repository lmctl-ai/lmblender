# lmcad to lmblender composition

`./examples/compose/run.sh` builds two solids with lmcad, exports STL, imports those files into lmblender, and renders them with CPU Cycles. It writes `build/compose-mug.png` and `build/compose-plate.png`.

Set `LMCAD_SOURCE`, `LMCAD_BIN`, or `LMBLENDER_BIN` to override this host's defaults. The default lmcad executable is the static `/home/ubuntu/lmcad-poc-build/lmcad-build/lmcad`; no new OCCT build is required if it is already available.

STL transfers tessellated triangles only. STL has no material or object-name channel, so lmblender assigns a new material after import. Coordinates are interpreted in the source model's exported units; this demo assumes lmcad coordinates map directly to Cycles world units. `mug-render.lms` and `plate-render.lms` show the lmblender side of the pipeline.
