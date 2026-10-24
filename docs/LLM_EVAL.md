# Blind LLM evaluation

lmblender is driven by LLMs, so its documentation must be sufficient for an agent with no prior context.
This evaluation measures that directly.

## Method

- Ten scene briefs (`tests/llm-eval/briefs/`) cover:
  - a product shot;
  - glass on a checker floor;
  - an APNG turntable;
  - a dark diagram with labels;
  - grass (`hair_patch`);
  - imported OBJ+MTL / glTF / image-textured assets;
  - volumetric light;
  - a text title;
  - depth/normal passes;
  - a multi-frame camera move.
- For each brief, a fresh agent (Claude Sonnet, general-purpose, no conversation context) worked in an isolated directory. It was given only:
  - the brief;
  - a copy of `docs/LLM_GUIDE.md`;
  - `schema/lmblender.dsl.json`;
  - the released `lmblender-linux-arm64` v0.3.6 binary and the `lmscript` compiler.
- It was not allowed to read examples, the repository, `lmscript help` (round 2) or the web.
- One `lmscript scene.lms` invocation is one attempt, with at most 4 attempts.
- **Success** means the final run exits 0 and the output matches the brief. Every output image was then judged by a reviewer (score out of 5).
- Renders were limited to 64 samples and 2 threads.

## Results

| Brief | Baseline (guide at bb9b75e) | Round 2 (guide at bc50929) | Round-2 review |
|---|---|---|---|
| 01 product shot | fail (5 attempts, no image) | **pass, 1 attempt** | 4/5 gold sphere, soft key/fill, contact shadow; gold slightly pale |
| 02 glass on checker | fail (5, no image) | **pass, 1** | 4/5 checker visibly refracted and inverted |
| 03 turntable APNG | fail (6, no image) | **pass, 1** | 4/5 24-frame orbit around a standing torus; light floor fireflies |
| 04 dark diagram | pass, 2 (3/5) | **pass, 3** | 3/5 readable nodes and labels; flat orbs, wedge arrows |
| 05 grass | fail (4, no image) | **pass, 1** | 3/5 grass, soil and rock; visible tile seams (see follow-ups) |
| 06 imported assets | pass, 4 (3/5; needed a workaround) | **pass, 2** | 4/5 OBJ+MTL, glTF and textured cube framed; cube sides untextured |
| 07 volumetric light | fail (4, no image) | **pass, 1** | 5/5 visible beam in haze onto a white cube |
| 08 text label | fail (4, wrong image) | **pass, 1** | 3/5 readable gold HELLO WORLD on a plinth; small, bright floor |
| 09 render passes | fail (4, no image) | **pass, 1** | 4/5 combined/depth/normal/albedo; depth contrast compressed |
| 10 camera move | fail (images only by running lmblender outside lmscript) | **pass, 1** | 3/5 clear dolly-in; floor checker too fine (moiré) |
| **Total** | **2/10, median 3 attempts per success** | **10/10, median 1 attempt** | mean 3.7/5 |

## Friction points and fixes

1. **No complete program.** The guide showed `_dsl_ cli=... install=...;` and a `system` one-liner, but never the
   block structure: `p = _dsl_ ...;`, the body, then a bare `end _dsl_` line, followed by `savedsl(p, path)`.
   Every baseline agent spent one to three attempts reverse-engineering it from compiler errors.
   *Fix:* a verified, copyable skeleton at the top of the guide, with the lmscript-vs-DSL rules: no int→float
   conversion and no list literals in the body.
2. **Renders over 60 s silently vanished.** The documented `system ${dsl_cli(p)} --dsl FILE;` uses lmscript's
   default 60 s `system` timeout and discards lmblender's exit code and JSON. Any longer render (turntables,
   sequences, volumes, grass at 2 threads) killed the renderer with exit 0, no message and no image. This was the
   main cause of the 8 baseline failures.
   *Fix:* the skeleton uses `system([exe, "--dsl", file], 3600000)`, prints `stdout`, and exits on a non-zero code.
   The guide explains the limit.
3. **Methods with bare typed signatures.** 25 host methods could not be found by name in the parameter table (about 15 had no description at all), including `ground_plane`,
   `checker_ground`, `wall_plane_y`, `studio_setup`, `Material.volume`/`subsurface`, `camera_keyframe`, the mesh
   bounds and `light_clamp`. Agents guessed argument order, for example "r,g,b,roughness,z" for `ground_plane`.
   *Fix:* every host method now has a row. `scripts/update-llm-guide.py --check`, which `test.sh` runs, fails
   when a method has no row.
4. **Geometry conventions.** Agents also had to guess:
   - `label` orientation and anchor (flat in XY, facing +Z);
   - primitive origins (centred);
   - the sun vector (direction of travel);
   - `hair_patch` layout;
   - `render_sequence` padding (`{frame}` vs `####`);
   - `render_passes` file names;
   - checker tile size (1/scale units).

   *Fix:* each is stated explicitly, with a short example where a wrong guess is costly.
5. **Exposure calibration.** Agents also guessed:
   - light powers (400 vs 4000 for the same scene);
   - `glow_orb` strength;
   - dark `world_color` values;
   - fog spot power;
   - fireflies.

   The guide's halo advice was also wrong: a larger orb is opaque and hides the core.

   *Fix:* starting values for every light type, glow/world calibration, a fog power note, and `light_clamp(10, 5)`
   for sparkle. The halo advice is corrected.

Other fixes:
- glTF missing-file errors now name the URI and say where it resolves. Before, they were a generic "image file is
  missing or unreadable".
- The baseline brief 06 asset folder lacked the glTF's external `checker.png`. That was an evaluation setup error,
  fixed for round 2.

## Product follow-ups (not documentation)

- `Scene.cube` + `image_material`: only the top face shows the image, and the side faces render a flat average.
  The cube primitive's side UVs appear degenerate.
- `hair_patch` blade positions depend only on the blade index, so equal patches tiled side by side repeat exactly.
  Seeding the pattern from the patch centre would allow seamless lawns. The guide now warns about this.
- `arrow_between` produces a single tapered wedge. A shaft-plus-head arrow is the common diagram need. The guide
  shows a workaround with `tube_between` + `cone`.
- `glow_orb` has no halo/bloom option.

## Reproducing

Prepare one directory per brief containing `BRIEF.md`, `docs/LLM_GUIDE.md`, `docs/lmblender.dsl.json`, an empty
`out/` and, for brief 06, `assets/`. Brief 06 uses `tests/fixtures/obj-mtl/study.*` and its PNGs,
`tests/fixtures/gltf/gallery.glb` and `checker.png`, and `tests/fixtures/checker-texture.png`. Give each
fresh agent the prompt above (inputs restricted, 4 attempts, the report format ATTEMPTS / SUCCESS / ERRORS /
CONFUSIONS) and judge the images it produces.
