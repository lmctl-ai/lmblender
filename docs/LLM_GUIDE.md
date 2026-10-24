# lmblender guide for DSL authors

`lmblender` executes lmscript DSL v5 bytecode and constructs a Cycles CPU scene directly in C++. It does not launch Blender or execute Python. For zero-setup use, declare `_dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";`, save the block with `savedsl`, and run the selected binary as `system ${dsl_cli(p)} --dsl FILE;`. `@latest` resolves to the newest non-draft release and updates a cached executable when its version differs; use an exact tag such as `@v0.3.4` for reproducibility or a fixed offline cache. A matching executable already on `PATH` takes precedence. For local development, put the build binary first on `PATH`. The source schema is at `schema/lmblender.dsl.json`. `lmblender --dsl-schema` prints the compact human summary, while `lmblender --dsl-schema --json` prints the exact embedded schema bytes; `--schema` remains a legacy JSON alias. `lmblender --version` prints the host, Cycles, and OpenImageDenoise versions. `lmblender --dsl-signatures` prints the host API; `lmscript --dsl-signatures schema/lmblender.dsl.json` prints the schema API. `test.sh` checks byte identity and signature parity.

Use the `_dsl_ cli= install=` form above to bind to lmblender. The R44 `import "github:OWNER/REPO@TAG PROGRAM"` form applies to hosts that provide an `lmapi` `--cli-schema`. lmblender exposes an lmscript DSL schema through `--dsl-schema`, not an `lmapi` CLI schema, so that import form does not apply.

## A complete program

Copy this skeleton; everything between the `_dsl_` line and `end _dsl_` is lmblender DSL, everything outside it is plain lmscript that compiles and runs it:

```text
mkdir("out", true);
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(1.0, 48, 24, true);
ball.set_material(scene.material(0.1, 0.3, 0.8, 0.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0));
scene.ground_plane(-1.0, 20.0, 0.8, 0.8, 0.8);
scene.area_light(-4.0, -5.0, 6.0, 0.0, 0.0, 0.0, 1.0, 0.9, 0.8, 500.0, 4.0);
scene.camera_look_at(0.0, -6.0, 2.5, 0.0, 0.0, 0.0, 45.0);
scene.render_png("out/ball.png", 64, 640, 480);
end _dsl_
savedsl(p, "out/scene.dslb");
let exe = dsl_cli(p).get(0, "lmblender");
let run = system([exe, "--dsl", "out/scene.dslb"], 3600000);
println(run.stdout);
if (run.code != 0) { println(f"lmblender failed ({run.code}): {run.stdout} {run.stderr}"); exit(1); }
```

Run it with `lmscript scene.lms`. Rules that are easy to get wrong:

- The block opens with an assignment statement `p = _dsl_ cli=... install=...;` that **ends with `;` on the same line**; the DSL body follows on the next lines and the block closes with a line that is exactly `end _dsl_` (no `;`). `p` holds the compiled block.
- `savedsl(p, path)` writes the bytecode; `dsl_cli(p)` returns the resolved program as a one-element list, e.g. `["/usr/local/bin/lmblender"]`.
- **Always pass a timeout and check the result.** `system([...], ms)` returns `{code, stdout, stderr}`; without the second argument lmscript kills the renderer after **60 seconds** and nothing is printed, so a longer render silently produces no image. The shorthand `system ${dsl_cli(p)} --dsl FILE;` has the same 60 s limit and discards errors; prefer the list form above. On failure lmblender prints one JSON line (`{"ok": false, "code": ..., "message": ..., "line": ...}`) to stdout and exits 3.
- Relative output and input paths resolve against the directory you run `lmscript` in, and output directories must exist (`mkdir("out", true)` before the block).
- Inside the block: `let x = ...;`, `x = ...;`, `if`/`while`, `+ - * /`, comparisons and `sin`/`cos` work; there is **no implicit int→float conversion** (`float(i) * 0.5`, not `i * 0.5`). `scene` is the only context object; meshes and materials are values returned by its methods.

## Coordinates, units, defaults

Coordinates are right-handed, Z-up, with +X right and +Y away from the default viewing side. Distances are scene units; there is no implicit conversion from millimeters or meters. Color channels are linear floats in [0,1] unless noted. All APIs are positional and have no optional arguments; where an API needs a default, set it explicitly using the listed method. Render methods use CPU Cycles, a deterministic configured seed (default 0), and four worker threads capped by the available cores (`set_threads` overrides). Output directories must already exist. Common limits: meshes/materials <=4096; scene geometry <=10 million vertices and 20 million triangles in total (`GEOMETRY_BUDGET`; instances are free); image inputs <=64 MiB on disk and <=64 megapixels decoded (`IMAGE_LIMIT`); render dimensions <=4096 each and 16 million pixels; samples <=65536; APNG <=240 frames and 64 million total frame-pixels. A whole program may run for 7200 s of wall clock and 100 million VM instructions by default (`LMBLENDER_RUN_TIMEOUT_SECONDS`, `LMBLENDER_MAX_INSTRUCTIONS`; 0 disables), independent of the per-render `render_timeout`.

## Host API signatures

This list is generated from `lmblender --dsl-signatures` by `scripts/update-llm-guide.py`; `test.sh` fails when it drifts:

```text
context scene: Scene
method Material.backface_culling(bool) -> void
method Material.bump(float, float, float) -> void
method Material.keyframe_color(int, float, float, float, string) -> void
method Material.keyframe_emission_strength(int, float, string) -> void
method Material.mix_with(Material, float) -> void
method Material.mix_with_texture(Material, string) -> void
method Material.subsurface(float, float, float, float, float) -> void
method Material.texture_maps(string, string, string, string) -> void
method Material.volume(string, float, float, float, float) -> void
method Mesh.keyframe_transform(int, float, float, float, string) -> void
method Mesh.keyframe_visible(int, bool, string) -> void
method Mesh.material_count() -> int
method Mesh.max_x() -> float
method Mesh.max_y() -> float
method Mesh.max_z() -> float
method Mesh.min_x() -> float
method Mesh.min_y() -> float
method Mesh.min_z() -> float
method Mesh.rotate_x(float) -> void
method Mesh.rotate_y(float) -> void
method Mesh.rotate_z(float) -> void
method Mesh.scale(float, float, float) -> void
method Mesh.set_material(Material) -> void
method Mesh.set_smooth(bool) -> void
method Mesh.translate(float, float, float) -> void
method Mesh.triangle_count() -> int
method Mesh.vertex_count() -> int
method Scene.area_light(float, float, float, float, float, float, float, float, float, float, float) -> void
method Scene.area_light_shape(float, float, float, float, float, float, float, float, float, float, float, float, string) -> void
method Scene.arrow_between(float, float, float, float, float, float, float) -> Mesh
method Scene.camera_dof(float, float) -> void
method Scene.camera_keyframe(int, float, float, float, float, float, float, string) -> void
method Scene.camera_look_at(float, float, float, float, float, float, float) -> void
method Scene.camera_optics_keyframe(int, float, float, string) -> void
method Scene.camera_orthographic(float, float, float, float, float, float, float) -> void
method Scene.capsule(float, float, int, int) -> Mesh
method Scene.checker_ground(float, float) -> void
method Scene.checker_material(float, float, float, float, float, float, float, float) -> Material
method Scene.checkmark(float, float, float, float, float) -> Mesh
method Scene.cone(float, float, int, bool) -> Mesh
method Scene.cube(float) -> Mesh
method Scene.cylinder(float, float, int, bool) -> Mesh
method Scene.denoise_quality(string) -> void
method Scene.environment_texture(string, float, float) -> void
method Scene.expect_int(int, int) -> void
method Scene.fit_camera(float) -> void
method Scene.glow_orb(float, float, float, float, float, float, float, float) -> Mesh
method Scene.ground_plane(float, float, float, float, float) -> void
method Scene.hair_patch(float, float, float, float, int, float, float) -> Mesh
method Scene.hair_sphere(float, float, float, float, int, float, float) -> Mesh
method Scene.image_material(string, float) -> Material
method Scene.import_gltf(string) -> Mesh
method Scene.import_obj(string) -> Mesh
method Scene.import_ply(string) -> Mesh
method Scene.import_stl(string) -> Mesh
method Scene.instance_mesh(Mesh, float, float, float, float, float, float) -> Mesh
method Scene.label(string, float, float, float, float) -> Mesh
method Scene.light_clamp(float, float) -> void
method Scene.load_preset(string) -> void
method Scene.loft(string, string, bool) -> Mesh
method Scene.material(float, float, float, float, float, float, float, float, float, float) -> Material
method Scene.material_from_palette(string) -> Material
method Scene.motion_blur(bool, float) -> void
method Scene.noise_material(float, float, float, float, float, float, float, float) -> Material
method Scene.plane(float) -> Mesh
method Scene.point_light(float, float, float, float, float, float, float) -> void
method Scene.point_light_keyframe(int, int, float, string) -> void
method Scene.preset_param(string, float) -> void
method Scene.ramp_material(float, float, float, float, float, float, float, float) -> Material
method Scene.render_apng(string, int, int, int, int, int, int) -> void
method Scene.render_exr(string, int, int, int) -> void
method Scene.render_passes(string, int, int, int) -> void
method Scene.render_png(string, int, int, int) -> void
method Scene.render_preset(string) -> void
method Scene.render_sequence(string, int, int, int, int, int) -> void
method Scene.render_settings(int, int, bool) -> void
method Scene.render_timeout(int) -> void
method Scene.ring_arc(float, float, float, float, float, float, float, int) -> Mesh
method Scene.set_threads(int) -> void
method Scene.set_view_transform(string) -> void
method Scene.sky_texture(float, float, float) -> void
method Scene.spot_light(float, float, float, float, float, float, float, float, float, float, float, float) -> void
method Scene.strand(string, float, float, int) -> Mesh
method Scene.studio_setup() -> void
method Scene.sun_light(float, float, float, float, float, float, float, float) -> void
method Scene.toon_outline(Mesh, float, float, float, float) -> Mesh
method Scene.torus(float, float, int, int, bool) -> Mesh
method Scene.transparent_background(bool) -> void
method Scene.tube_between(float, float, float, float, float, float, float) -> Mesh
method Scene.use_preset(string) -> void
method Scene.uv_sphere(float, int, int, bool) -> Mesh
method Scene.version() -> string
method Scene.wall_plane_y(float, float, float, float, float) -> void
method Scene.world_color(float, float, float, float) -> void
```

## Parameter guide

| API | Meaning and bounds |
|---|---|
| `cube(size)`, `plane(size)` | Positive extent in scene units, up to 10,000. |
| `uv_sphere(radius, segments, rings, smooth)`, `cylinder(radius,height,segments,smooth)`, `cone(radius,height,segments,smooth)`, `torus(major,minor,segments,rings,smooth)` | Dimensions in scene units; segments 8..256 and rings 4..256. |
| `capsule(radius,length,segments,cap_rings)` | Rounded capsule along Z; total length is at least twice the radius. |
| `glow_orb(x,y,z,radius,r,g,b,strength)` | Smooth emissive sphere; use a second larger low-strength orb for a simple halo effect. |
| `tube_between(x1,y1,z1,x2,y2,z2,radius)`, `arrow_between(...)` | Cylindrical path or tapered arrow between two 3D points. |
| `strand("x,y,z; ...",root_radius,tip_radius,sides)` | A swept polyline of 2..512 points with taper; radii are scene units, sides 3..32. The returned mesh accepts `set_material`. |
| `hair_patch(x,y,z,size,count,height,width)`, `hair_sphere(x,y,z,radius,count,length,width)` | Deterministic curved swept strands (1..2048 per emitter), combined into one material-ready mesh. These are polygonal strands, not simulated/native Cycles hair. `hair_patch`: `(x,y,z)` is the centre of a `size`×`size` square on which blades are rooted; blades grow along +Z to 72–100% of `height`; `width` is the blade root width (≤ height/5). For a lawn, use several patches (2048 blades each) with a green `material` and a brown `ground_plane` just below `z`. |
| `loft("x,y,z;...", "u,v;...", caps)` | Sweep a strictly convex 2D profile (3..64 corners) through 2..512 3D path points. Parallel-transported orientation avoids abrupt section twists. |
| `toon_outline(mesh, radius, r,g,b)` | Add dark cylindrical strokes to boundary and sharp feature edges (dihedral over 30 degrees), sharing the source mesh transform. Intended for hard-surface/toon diagrams; it is not a view-dependent silhouette outline on smooth meshes. |
| `ring_arc(x,y,z,major,minor,start_deg,end_deg,segments)` | Partial torus gauge, angles in degrees, ordered span up to 360. |
| `checkmark(x,y,z,size,stroke)` | Green check silhouette mesh; assign its material explicitly. |
| `label(text,x,y,z,cell_size)` | Extruded 5×7 ASCII uppercase letters and spaces, suitable for flat title plates. |
| `import_obj(path)`, `import_stl(path)`, `import_ply(path)`, `import_gltf(path)` (`.gltf` or `.glb`) | Import mesh from a regular file; formats and size limits are validated. OBJ also loads confined `.mtl` libraries with per-face `usemtl`, corner UVs/normals, `Kd`/`Ks`/`Ns`/`d`/`Tr`/`Ke`, and PNG/JPEG/EXR color/bump/opacity maps; `Mesh.material_count()` counts used slots. |
| `mesh.translate(x,y,z)`, `scale(x,y,z)`, `rotate_x(degrees)`, `rotate_y(degrees)`, `rotate_z(degrees)` | Relative translation/scale and baked axis rotations. Rotations update authored vertices and explicit corner normals; rotate the source before `instance_mesh`. |
| `material(r,g,b,metallic,roughness,transmission,emission_r,g,b,emission_strength)` | Principled BSDF factors; color [0,1], emission strength [0,10000]. |
| `checker_material(c1 RGB,c2 RGB,scale,roughness)`, `noise_material(...)`, `ramp_material(...)` | Two RGB colors, procedural scale, roughness. Ramp uses Generated coordinates and Noise → RGB Ramp. |
| `image_material(path,roughness)` | PNG/JPEG/EXR texture read with the mesh UVs. PNG and JPEG color images (also glTF base color, OBJ `map_Kd`, and `environment_texture` PNGs) are decoded from sRGB; EXR and HDR are read as scene-linear. |
| `Material.bump(strength,distance,scale)` | Procedural noise to bump normal; strength [0,1], distance 0..2 scene units. |
| `Material.texture_maps(normal,roughness,metallic,ao)` | Optional PNG/JPEG/EXR tangent normal, roughness, metallic, and AO maps; empty path leaves a channel unset. Data maps use Cycles' `data` colorspace. AO multiplies base color; it is not a separate ambient-occlusion shader. Each map <=64 MiB, all maps <=256 MiB. |
| `Material.mix_with(other,factor)`, `mix_with_texture(other,path)` | Mixes the receiver shader with a plain Principled second layer by constant [0,1] or texture factor. Complex second layers are rejected. |
| `Material.backface_culling(enabled)` | When enabled, backside hits mix to transparent; default path shading is two-sided. |
| `area_light(px,py,pz,tx,ty,tz,r,g,b,power,size)` | Area-light position/target, linear color, power, square side length. |
| `area_light_shape(...,power,size_x,size_y,shape)` | `rectangle`, `disk`, or `ellipse`; sizes are in scene units. |
| `spot_light(px,py,pz,tx,ty,tz,r,g,b,power,angle_degrees,blend)` | Cone angle 1..170 degrees; softness/blend 0..1. |
| `point_light(x,y,z,r,g,b,power)`; `sun_light(dx,dy,dz,r,g,b,power,angle_degrees)` | World-space position, or for the sun the **direction the light travels** (e.g. `(0.3,0.5,-1.0)` shines down and toward +Y; a low sun has a small negative z); power in renderer units and the sun's angular diameter in **degrees** (0..90). |
| `camera_look_at(px,py,pz,tx,ty,tz,fov_degrees)` | Perspective camera; **vertical** FOV 5..170 degrees, so the horizontal coverage follows render aspect. |
| `camera_orthographic(px,py,pz,tx,ty,tz,vertical_span)` | Orthographic camera; visible vertical span in scene units, with horizontal span adjusted to output aspect. |
| `camera_dof(aperture,focus_distance)` | Aperture 0..10; focus distance in scene units. |
| `motion_blur(enabled,shutter_frames)` | Cycles transform blur for keyed mesh translations and camera keys; shutter is 0.1..2 frames. |
| `world_color(r,g,b,strength)`, `sky_texture(elevation_degrees,rotation_degrees,intensity)` | Linear RGB world or built-in procedural sky. |
| `render_settings(max_bounces,seed,denoise)` | Max bounces 0..64; seed is an integer in 0..2147483647. |
| `set_threads(n)`, `denoise_quality(name)` | 1..64 CPU threads (capped by cores); OIDN quality `fast`, `balanced`, or `high`. |
| `Mesh.keyframe_transform/keyframe_visible`, `Material.keyframe_color/keyframe_emission_strength`, `camera_keyframe`, `camera_optics_keyframe`, `point_light_keyframe` | Frame keys with `linear` or `ease` interpolation; visibility keys hold their value between frames (the interpolation argument is accepted for uniformity but has no effect). See [ANIMATION.md](ANIMATION.md). |
| `render_preset(name)` | Legacy `draft` = 8 samples, 2 bounces, no denoise; legacy `final` = 64 samples, 8 bounces, CPU OIDN high quality. For video diagrams use named `use_preset("diagram-dark-16x9-draft"|"diagram-dark-16x9-final")` profiles below. Pass `0` for samples to `render_png` or `render_sequence` to use the selected preset count. `Scene.set_threads(n)` selects 1–64 CPU threads; default four is capped by available cores. See [throughput measurements](THROUGHPUT.md). |
| `denoise_quality(name)` | Set CPU OIDN quality to `fast`, `balanced`, or `high`. Fast uses Cycles' prefiltered auxiliary albedo/normal passes and is the diagram-final preset default. |
| `use_preset(name)` / `load_preset(path)` | Apply one embedded presentation preset or a version-1 external JSON preset. Later explicit DSL calls override its settings: explicit camera position/target calls disable auto-frame; the first explicit light call removes the preset light rig, and later user lights add to it. `fit_camera` explicitly opts back into framing. `lmblender --presets` lists names; `--preset-show NAME` prints the embedded JSON. |
| `preset_param(name,value)` | Numeric overrides: `samples`, `bounces`, `threads`, `world_strength`, `camera_fov_degrees`, `orthographic_vertical_span`. |
| `material_from_palette(name)` | Create a material from the active preset palette (`color`, `metallic`, `roughness`, optional `transmission` and `emission`). |
| `render_png(path,samples,width,height)`, `render_exr(path,samples,width,height)` | Single frame, samples 1..65536 (`render_png` also accepts 0 = preset count), dimensions and total pixel bounds apply. Set `LMBLENDER_OUTPUT_ROOT` to confine every output path to one directory. |
| `render_sequence(pattern,start,end,samples,width,height)` | Inclusive integer frames; the path must contain `{frame}` (unpadded: `move_1.png`) or `####` (zero-padded to the number of `#`: `move_0001.png`); key interpolation is `linear` or `ease`. A 12-frame sequence easily exceeds 60 s, so use the `system([...], 3600000)` form from the skeleton. |
| `render_apng(path,start,end,samples,width,height,fps)` | Inclusive frames; exact rational frame delay is 1/fps; fps 1..240. |
| `render_passes(prefix,samples,width,height)` | Writes `PREFIX-combined`, `PREFIX-depth`, `PREFIX-normal` and `PREFIX-albedo`, each as linear `.exr` plus a viewable `.png` (depth normalized near=white/far=dark, normals mapped to RGB). Example: `scene.render_passes("out/passes", 32, 640, 480)` writes `out/passes-depth.png` etc. |
| `set_view_transform(name)` | `Standard`, `Filmic`, or `AgX`. |
| `ground_plane(z,half_size,r,g,b)` | Horizontal matte floor at height `z`, a square from -half_size..+half_size in X and Y (half_size up to 1,000,000), linear base color in [0,1]. Example: `scene.ground_plane(-1.0, 20.0, 0.8, 0.8, 0.8)` under a radius-1 sphere at the origin. |
| `checker_ground(half_size,tile_scale)` | Matte dark-navy/light-grey checkerboard floor at z = -0.02; half_size (0,10000], tile_scale 0.1..1000 (larger = more, smaller tiles). For custom colors use `plane` + `checker_material`. |
| `wall_plane_y(y,half_size,r,g,b)` | Vertical backdrop facing -Y at depth `y` (spans X and Z by ±half_size); slightly self-lit (emission 0.3 of its color) so it never renders black. |
| `studio_setup()` | Call **after** creating your meshes: adds a three-light area rig, a neutral floor under the lowest point, a soft grey world, and an auto-framed 42° camera around all meshes. Later explicit camera/light calls override it. |
| `Material.volume(kind,density,r,g,b)` | Turns the material into a volume-only medium: `kind` is `"scatter"` (fog/haze) or `"absorption"` (tinted glass/liquid), density 0..100 per scene unit, color in [0,1]. Assign it to a closed mesh that encloses the region, e.g. `let fog = scene.cube(20.0); fog.set_material(haze);` with `haze.volume("scatter", 0.05, 1.0, 1.0, 1.0)`; a `spot_light` shining through it shows a visible beam. Keep densities low (0.02–0.2) for haze. |
| `Material.subsurface(weight,radius_r,radius_g,radius_b,scale)` | Subsurface scattering for wax/skin/marble: weight and per-channel radius in [0,1], scale 0.001..10 scene units. |
| `Mesh.set_smooth(enabled)` | `true` welds shared vertices and uses smooth vertex normals; `false` keeps faceted shading. |
| `Mesh.min_x/min_y/min_z/max_x/max_y/max_z()`, `vertex_count()`, `triangle_count()` | Bounds of the mesh's own vertices **after** `rotate_*` but **before** `translate`/`scale` (add your own translation); counts of authored vertices/triangles. Useful to center or rest imported meshes on the floor. |
| `light_clamp(direct,indirect)` | Clamp sample brightness to suppress fireflies; 0..10000, 0 disables (default). |
| `transparent_background(enabled)` | Render the world as transparent alpha in PNG/EXR output while still lighting the scene. |
| `render_timeout(seconds)` | Per-render limit 1..3600 s (fails with `RENDER_TIMEOUT`). This is separate from lmscript's own `system(...)` timeout described above. |
| `expect_int(actual,expected)` | Test helper: fails the program with a clear message when the integers differ. |
| `label` orientation | Letters are built flat in the XY plane facing **+Z** (readable from above); `x,y,z` is the bottom-left corner, each character advances `cell_size` in X and the text is 7/6·`cell_size` tall. For text facing a camera on the -Y side, `rotate_x(90.0)` it, then `translate`. |
| `glow_orb` strength | Emission 1–3 keeps the hue under the Standard view transform; ~6 and above clips to near-white. |
| `world_color` strength | Background radiance, not exposure: (0.01,0.01,0.02,1.0) already reads as a visible dark navy; use about 0.002 per channel or strength 0.2 for near-black diagram backgrounds. |
| glTF external files | Buffers and images referenced by a `.gltf`/`.glb` relative URI must sit next to it (no `..`, no absolute paths); a missing one fails with `GLTF_INVALID` naming the URI. |
| Light power scale | Rough starting points for a subject about 5–8 units from the light: `area_light` 300–800, `point_light` 100–800, `spot_light` 300–1500, `sun_light` 2–5 (about 15 is harsh noon). Power falls off with distance squared (sun excepted), so double the distance needs ~4× power. Under- or over-exposure of the whole frame usually means the power is off by 5–10×, not a material problem. |
| `camera_keyframe(frame,px,py,pz,tx,ty,tz,interp)` | Key the camera position and look-at target at an integer frame; `interp` is `"linear"` or `"ease"`. Field of view comes from `camera_look_at` (or `camera_optics_keyframe(frame,vertical_fov_degrees,orthographic_vertical_span,interp)` to animate it). Key at least the first and last frame you render. |
| Primitive placement | `cube`, `uv_sphere`, `cylinder`, `cone`, `capsule` and `torus` are centred on the origin (`cylinder`/`cone`/`capsule` along Z, `torus` lying in the XY plane); `plane(size)` lies in the XY plane at z = 0 facing +Z. To rest a height-`h` cone on a floor at z = 0, `translate(0.0, 0.0, h * 0.5)`. |

## Recipes

A minimal subject render:

```text
let ball = scene.uv_sphere(1.0, 48, 24, true);
let ceramic = scene.material(0.1, 0.3, 0.8, 0.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0);
ball.set_material(ceramic);
scene.area_light(-4.0, -5.0, 6.0, 0.0, 0.0, 0.0, 1.0, 0.9, 0.8, 500.0, 4.0);
scene.camera_look_at(0.0, -6.0, 2.5, 0.0, 0.0, 0.0, 45.0);
scene.render_png("build/ball.png", 64, 640, 480);
```

For an animated loop, add mesh and camera keys at the first, middle, and final frame, keeping the first and final values equal. See `examples/animation-tween.lms`; its numeric gate checks endpoint pixel identity and midpoint change.

## Errors and recovery

The host prints one JSON line with `ok`, `code`, and a human-readable `message` (plus `line`/`column` when a DSL statement is to blame); direct `--version`, `--schema`, and `--dsl-signatures` commands print plain text or JSON to stdout. Codes:

- `LOAD_BAD_BYTECODE`, `LOAD_SCHEMA_MISMATCH`, `LOAD_UNBOUND`: recompile the `.lms` against the current schema (exit 3, phase `load`).
- `RUN_HOST`: a host validation failure (range, non-finite value, missing file, wrong extension, unsupported combination); the message names the method and the accepted bounds. `RUN_ERROR` is a VM fault such as division by zero.
- `GEOMETRY_BUDGET`, `IMAGE_LIMIT`, `IMAGE_INVALID`, `OBJ_INVALID`/`OBJ_LIMIT`, `OBJ_MTL_INVALID`/`OBJ_MTL_LIMIT`/`OBJ_MTL_UNSUPPORTED`, `GLTF_INVALID`/`GLTF_LIMIT`/`GLTF_UNSUPPORTED`, `PRESET_INVALID`/`PRESET_NOT_FOUND`/`PRESET_UNSUPPORTED`, `OUTPUT_PATH`: structured resource or input problems; fix the input rather than retrying.
- `RENDER_TIMEOUT`: one render exceeded `render_timeout(seconds)` (1..3600); lower samples or resolution, or raise the timeout.
- `RUN_TIMEOUT`, `RUN_BUDGET`: the whole program exceeded its wall-clock or VM-instruction budget (see limits above); a running render is cancelled first, a pure loop is terminated after a five-second grace period.

Use the error message as the first diagnostic; do not retry unchanged invalid values.

## Named presets

The versioned embedded catalog is [presets/presets.json](../presets/presets.json). It includes diagram-dark-16x9-draft/final, product-studio, turntable-neutral, glass-showcase, and technical-clean. The diagram profiles target 960x540 at 8 spp without denoising and 1280x720 at 16 spp with fast OIDN respectively; this host measured 0.41 s and 2.00 s per frame on the simple emissive benchmark scene. `tests/denoise_speed.lms` and `tests/denoise_speed_quality.py` compare fast 16 spp to a 256-spp high-quality reference; measured PSNR was 47.14 dB and windowed SSIM 0.99451. Runnable previews are in [examples/presets](../examples/presets). Preset resolution/FPS fields are metadata; render calls still take dimensions and sequence timing explicitly. External JSON uses the same schema and a 128 KiB file cap.
