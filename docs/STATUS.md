# Full-check status

Generated: 2026-10-24 00:30 UTC

Source revision tested: `09b0854 (clean worktree)`

## Results

- `BUILD_JOBS=2 ./test.sh`: PASS.
- Video sequences: four 24-frame 640×360 sequences passed all per-frame numeric gates; ffmpeg encoding was attempted when available.
- Strict pixel and file checks: PASS; 20 gallery images are at least 640×480.
- Static linkage: PASS (`ldd` reports “not a dynamic executable” for both binaries).
- Binary sizes: `lmblender` 90503640 bytes; standalone Cycles 88386432 bytes.
- QA provider `agy/gemini-3.8-flash-high`: PASS on all 24 named images; exact responses follow.

## QA responses

- gltf-json: PASS
- glb: PASS
- primitives: **PASS**
- environment: PASS
- sky: PASS
- transparent: PASS
- dof: **PASS**
- fog: **PASS**
- subsurface: PASS
- glass: **PASS**
- procedural: PASS
- image-texture: PASS
- material-maps: PASS
- backface-culling: PASS
- strands: PASS
- loft-outline: PASS
- spot-falloff: PASS
- light-falloff: **PASS**
- bridge: **PASS**
- obj-mtl: **PASS**
- video-level1: PASS
- video-level2: PASS
- video-level3: **PASS**
- video-level4: PASS

## Comparison with the 2026-10-22 Claude review

The [historical Claude review](CLAUDE_QA_2026-10-22.md) reported 4 PASS and 20 FAIL on its first pass over the same 24 named checks. This default Gemini run passed 24/24 after two corrections: the bridge checklist now describes the intentional sphere-over-cube occlusion visible in the matched Blender reference, and the OBJ/MTL scene now has a readable localized blue-cube highlight. The former was a checklist mismatch; the latter was a real lighting/material presentation defect. No failing numeric gate was removed.

The separate README story gallery review (`scripts/story-qa.sh`) passed 12/12 specific visual checklists with the same default Gemini judge; its responses are in `build/story-qa/`. Eleven story images were regenerated and passed numeric checks in `test.sh`. The gear-pair image used the committed reference because `lmcad` was not on PATH for this run.
