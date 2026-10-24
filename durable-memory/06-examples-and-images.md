# Examples and README images

- Every example self-executes: `out = ARGV...; mkdir("images"); mkdir("build"); p = _dsl_ ...; savedsl(p, out);
  r = system ${dsl_cli(p)} --dsl ${out}; assert(r.code == 0);` and writes `images/<name>.png` in the current
  directory, so `lmctl autorun <raw URL>` from an empty directory produces the picture. Feature/regression
  examples keep the long `system(argv, 3600000)` form (lmscript's default system() timeout is 60 s).
- README gallery = 12 story examples (caustic-ball-lens, sunset-long-shadows, god-rays-window, cornell-box,
  backlit-skin, gear-pair-render [lmcad + lmblender], caffeine-molecule, dna-double-helix, salt-crystal,
  saturn-rings, kepler-orbit [GIF via ffmpeg], bouncing-ball-strobe). Each must stay under ~60 s on 4 cores:
  900 px wide, 24-64 samples + OIDN. `examples/render-all.sh` reruns them in a temp dir and copies to docs/images/.
- A second README table, "Scenes shared with lmcad, lmmagnum and lmfilament", holds ball-stick, buckyball, truss-bridge (lmcad),
  pbr-spheres, material-sandbox, model-viewer, text (lmmagnum/lmfilament). They were ported by swapping the scene API calls
  (Z-up here, Y-up there: (x,y,z) -> (x,-z,y)); test.sh compiles and runs each, no image-quality gate.
  Added from magnum-examples: shadows, multidraw, cubemap (sky + mirror spheres), bullet (closed-form ballistics), fluidsimulation3d/2d
  (analytic flows drawn as tubes). multidraw is rendered at 700x500 to stay under the 300 KiB image budget.
- test.sh: compile_dsl truncates at the first savedsl and rewrites `"images/` to `"$build/`, so regression
  checks still read `$build/<file>`; story renders get tests/story_quality.py gates; README autorun URLs and
  docs/images magic bytes/size limits are checked. gear-pair-render runs only when lmcad is on PATH (SKIP printed).
- DSL blocks: no comments, no arrays, no `len`/`exp` (use pow(e, x)); string building with `+`/str() works.
  label() accepts letters and spaces only (size argument = advance per character).
- Lamps are not hit by BSDF rays (Cycles light use_mis default false): no caustics/reflections of lamps.
  Use emissive meshes for lamps that must reflect or focus (DRAW_NEEDS has the host request).
- Mesh.rotate_* bakes into vertices but scale stays an object transform, so scale-after-rotate shears; build
  flat annuli with loft() instead of scaling a rotated torus. Instances rendered flat-shaded through v0.3.6 (fixed on main after v0.3.6;
  `instance.set_smooth(true)` works on older hosts).
