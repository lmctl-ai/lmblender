# lmblender guide for DSL authors

`lmblender` executes lmscript DSL v5 bytecode and constructs a Cycles CPU scene directly in C++. It does not launch Blender or execute Python. Create a `.lms` file, import `../schema/lmblender.dsl.json`, then run `lmscript scene.lms scene.dslb` and `lmblender --dsl scene.dslb`. `lmblender --version` prints the host, Cycles, and OpenImageDenoise versions. `lmblender --dsl-signatures` prints the host API; `lmscript --dsl-signatures schema/lmblender.dsl.json` prints the schema API. `test.sh` diffs these outputs so the schema and executable must agree.

## Coordinates, units, defaults

Coordinates are right-handed, Z-up, with +X right and +Y away from the default viewing side. Distances are scene units; there is no implicit conversion from millimeters or meters. Color channels are linear floats in [0,1] unless noted. All APIs are positional and have no optional arguments; where an API needs a default, set it explicitly using the listed method. Render methods use CPU Cycles, a deterministic configured seed (default 0), and two worker threads. Output directories must already exist. Common limits: meshes/materials <=4096; image inputs <=64 MiB; render dimensions <=4096 each and 16 million pixels; samples <=65536; APNG <=240 frames and 64 million total frame-pixels.

## Host API signatures

This list is generated from `lmblender --dsl-signatures` and should match the DSL schema:

```text
context scene: Scene
method Material.bump(float, float, float) -> void
method Material.keyframe_color(int, float, float, float, string) -> void
method Material.texture_maps(string, string, string, string) -> void
method Material.mix_with(Material, float) -> void
method Material.mix_with_texture(Material, string) -> void
method Material.backface_culling(bool) -> void
method Material.subsurface(float, float, float, float, float) -> void
method Material.volume(string, float, float, float, float) -> void
method Mesh.keyframe_transform(int, float, float, float, string) -> void
method Mesh.max_x() -> float
method Mesh.max_y() -> float
method Mesh.max_z() -> float
method Mesh.min_x() -> float
method Mesh.min_y() -> float
method Mesh.min_z() -> float
method Mesh.rotate_x(float) -> void
method Mesh.scale(float, float, float) -> void
method Mesh.set_material(Material) -> void
method Mesh.set_smooth(bool) -> void
method Mesh.translate(float, float, float) -> void
method Mesh.triangle_count() -> int
method Mesh.vertex_count() -> int
method Scene.area_light(float, float, float, float, float, float, float, float, float, float, float) -> void
method Scene.area_light_shape(float, float, float, float, float, float, float, float, float, float, float, float, string) -> void
method Scene.camera_dof(float, float) -> void
method Scene.camera_keyframe(int, float, float, float, float, float, float, string) -> void
method Scene.camera_look_at(float, float, float, float, float, float, float) -> void
method Scene.camera_orthographic(float, float, float, float, float, float, float) -> void
method Scene.motion_blur(bool, float) -> void
method Scene.checker_ground(float, float) -> void
method Scene.checker_material(float, float, float, float, float, float, float, float) -> Material
method Scene.cone(float, float, int, bool) -> Mesh
method Scene.cube(float) -> Mesh
method Scene.cylinder(float, float, int, bool) -> Mesh
method Scene.environment_texture(string, float, float) -> void
method Scene.expect_int(int, int) -> void
method Scene.fit_camera(float) -> void
method Scene.ground_plane(float, float, float, float, float) -> void
method Scene.image_material(string, float) -> Material
method Scene.import_gltf(string) -> Mesh
method Scene.import_obj(string) -> Mesh
method Scene.import_ply(string) -> Mesh
method Scene.import_stl(string) -> Mesh
method Scene.instance_mesh(Mesh, float, float, float, float, float, float) -> Mesh
method Scene.light_clamp(float, float) -> void
method Scene.material(float, float, float, float, float, float, float, float, float, float) -> Material
method Scene.noise_material(float, float, float, float, float, float, float, float) -> Material
method Scene.plane(float) -> Mesh
method Scene.point_light(float, float, float, float, float, float, float) -> void
method Scene.ramp_material(float, float, float, float, float, float, float, float) -> Material
method Scene.render_apng(string, int, int, int, int, int, int) -> void
method Scene.render_exr(string, int, int, int) -> void
method Scene.render_passes(string, int, int, int) -> void
method Scene.render_png(string, int, int, int) -> void
method Scene.render_sequence(string, int, int, int, int, int) -> void
method Scene.render_settings(int, int, bool) -> void
method Scene.render_timeout(int) -> void
method Scene.set_view_transform(string) -> void
method Scene.sky_texture(float, float, float) -> void
method Scene.spot_light(float, float, float, float, float, float, float, float, float, float, float, float) -> void
method Scene.sun_light(float, float, float, float, float, float, float, float) -> void
method Scene.torus(float, float, int, int, bool) -> Mesh
method Scene.transparent_background(bool) -> void
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
| `import_obj/stl/ply/gltf(path)` | Import mesh from a regular file; formats and size limits are validated. |
| `mesh.translate(x,y,z)`, `scale(x,y,z)`, `rotate_x(degrees)` | Relative translation/scale and baked X-axis rotation. |
| `material(r,g,b,metallic,roughness,transmission,emission_r,g,b,emission_strength)` | Principled BSDF factors; color [0,1], emission strength [0,10000]. |
| `checker_material(c1 RGB,c2 RGB,scale,roughness)`, `noise_material(...)`, `ramp_material(...)` | Two RGB colors, procedural scale, roughness. Ramp uses Generated coordinates and Noise → RGB Ramp. |
| `image_material(path,roughness)` | PNG/JPEG/EXR texture read with the mesh UVs. |
| `Material.bump(strength,distance,scale)` | Procedural noise to bump normal; strength [0,1], distance 0..2 scene units. |
| `Material.texture_maps(normal,roughness,metallic,ao)` | Optional PNG/JPEG/EXR tangent normal, roughness, metallic, and AO maps; empty path leaves a channel unset. Data maps use Cycles' `data` colorspace. AO multiplies base color; it is not a separate ambient-occlusion shader. Each map <=64 MiB, all maps <=256 MiB. |
| `Material.mix_with(other,factor)`, `mix_with_texture(other,path)` | Mixes the receiver shader with a plain Principled second layer by constant [0,1] or texture factor. Complex second layers are rejected. |
| `Material.backface_culling(enabled)` | When enabled, backside hits mix to transparent; default path shading is two-sided. |
| `area_light(px,py,pz,tx,ty,tz,r,g,b,power,size)` | Area-light position/target, linear color, power, square side length. |
| `area_light_shape(...,power,size_x,size_y,shape)` | `rectangle`, `disk`, or `ellipse`; sizes are in scene units. |
| `spot_light(px,py,pz,tx,ty,tz,r,g,b,power,angle_degrees,blend)` | Cone angle 1..170 degrees; softness/blend 0..1. |
| `point_light(x,y,z,r,g,b,power)`; `sun_light(dx,dy,dz,r,g,b,power,angle)` | World-space position or direction; power in renderer units and sun angle in radians. |
| `camera_look_at(px,py,pz,tx,ty,tz,fov_degrees)` | Perspective camera, FOV 5..170 degrees. |
| `camera_orthographic(px,py,pz,tx,ty,tz,width)` | Orthographic camera width in scene units. |
| `camera_dof(aperture,focus_distance)` | Aperture 0..10; focus distance in scene units. |
| `motion_blur(enabled,shutter_frames)` | Cycles transform blur for keyed mesh translations and camera keys; shutter is 0.1..2 frames. |
| `world_color(r,g,b,strength)`, `sky_texture(elevation_degrees,rotation_degrees,intensity)` | Linear RGB world or built-in procedural sky. |
| `render_settings(max_bounces,seed,denoise)` | Max bounces 0..64; seed is a nonnegative 32-bit integer. |
| `render_png/exr(path,samples,width,height)` | Single frame, samples 1..65536, dimensions and total pixel bounds apply. |
| `render_sequence(pattern,start,end,samples,width,height)` | Inclusive integer frames; path includes `{frame}` or `####`; key interpolation is `linear` or `ease`. |
| `render_apng(path,start,end,samples,width,height,fps)` | Inclusive frames; exact rational frame delay is 1/fps; fps 1..240. |
| `render_passes(prefix,samples,width,height)` | Writes combined/depth/normal/albedo EXR and normalized PNGs. |
| `set_view_transform(name)` | `Standard`, `Filmic`, or `AgX`. |

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

The host returns a JSON result with `ok`, `code`, and a human-readable `message`; direct `--version`, `--schema`, and `--dsl-signatures` commands print plain text or JSON to stdout. `LOAD_BAD_BYTECODE` means compile the `.lms` against the current schema again. `RUN_ERROR`/`VALIDATION_ERROR` means check method signatures, finite values, ranges, input paths, or output extensions. `RENDER_TIMEOUT` means increase `render_timeout(seconds)` within 1..3600 or lower samples/resolution. Import errors identify the expected format, malformed header, resource budget, or unreadable regular-file condition. Use the error message as the first diagnostic; do not retry unchanged invalid values.
