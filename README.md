# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The host embeds the pinned DSL v5 VM and schema. Blender's standalone Cycles source is built separately, without Blender, Python, GUI, or GPU backends.

## Current milestone status

| Area | Status | Current support / gap |
| --- | --- | --- |
| CPU Cycles render | Done | Static executable, direct C++ scene construction, PNG and EXR, CPU OIDN denoise |
| Materials and lights | Partial | Principled, checker/noise/image textures, point/area/sun/spot and emissive surfaces; glass transmission binding is present; subsurface/volume nodes are experimental (see numeric QA failures) |
| World | Partial | Color background, Cycles multiple-scattering sky, and equirectangular PNG/EXR/HDR environment node with rotation/strength |
| Camera and motion | Partial | Perspective/orthographic, look-at/fitting, and Cycles aperture/focus DOF; animated-transform motion blur is not implemented (direct CCL motion prototype rendered blank) |
| Mesh input | Partial | OBJ, STL, ASCII PLY; binary PLY and glTF remain |
| Gallery QA | Partial | 640×480 sky image with numeric QA and thumbnail; broader gallery resolution/QA pass remains |
| Alpha output | Partial | Transparent world film emits RGBA PNG; shadow-catcher composition remains |

- The static DSL host exposes `scene.version()`, `scene.import_obj(path)`, `scene.import_stl(path)`, and `scene.import_ply(path)`; PLY currently accepts bounded ASCII 1.0 meshes with x/y/z vertices and polygon face indices. Imported meshes expose vertex/triangle counts and bounds. `scene.render_png(path, samples, width, height)` builds Cycles `Scene`, `Mesh`, `Object`, `ShaderGraph`, camera, and light nodes directly through the Cycles C++ API and renders them with an in-process CPU session. The renderer uses a fitted Z-up perspective camera by default, and assigns Cycles emission shaders to direct-light nodes so they participate in the light manager. Binary PLY and glTF import remain unsupported.
- `scene.material(r, g, b, metallic, roughness, transmission, emission_r, emission_g, emission_b, emission_strength)` creates a Principled BSDF material. `scene.checker_material(...)` and `scene.noise_material(...)` connect Cycles procedural nodes into Principled base color. `scene.image_material(path, roughness)` loads a PNG/JPEG/EXR via the static OIIO path, uses per-corner cube-projected UV coordinates, and caps the input at 64 MiB; PNG has a real-render regression fixture. Attach any with `mesh.set_material(material)`. Authored UV maps and texture color-space controls remain planned.
- Lighting bindings include `scene.world_color(r, g, b, strength)`, point, area, sun, and spot lights. `scene.sky_texture(elevation_degrees, rotation_degrees, intensity)` uses Cycles' multiple-scattering sky node as a world background; `examples/sky-texture.lms` checks a 640x480 vertical luminance gradient (top 188.76, horizon 14.55; clipped pixels below 5%). `scene.area_light(px, py, pz, tx, ty, tz, r, g, b, strength, size)`, `scene.sun_light(dx, dy, dz, r, g, b, strength, angle_degrees)`, and `scene.spot_light(px, py, pz, tx, ty, tz, r, g, b, strength, angle_degrees, blend)` map to Cycles light nodes. `scene.ground_plane(z, size, r, g, b)` adds a diffuse XY floor; `scene.wall_plane_y(y, size, r, g, b)` adds a lightly self-lit backdrop; `scene.fit_camera(fov_degrees)` defers its aspect-aware fit until render dimensions are known, uses transformed non-studio bounds, and compensates the horizontal sensor basis. The automatic point light is omitted when explicit point lights are configured. HDRI environments remain planned.
- `scene.camera_look_at(px, py, pz, tx, ty, tz, fov_degrees)` sets perspective. `scene.camera_orthographic(px, py, pz, tx, ty, tz, width)` sets an orthographic camera; the projection example numerically compares its framing with perspective. Panoramic projection, DOF, and camera instancing are still planned.
- Render inputs are bounded to 65,536 samples, 4096x4096 dimensions and 16 million pixels, 10 million vertices per mesh, 20 million triangles total, 4096 imported meshes/materials, and 1024 point lights. `scene.render_timeout(seconds)` sets the Cycles session wall-clock limit (1–3600 seconds; default 3600); timeout cancels the render, removes its staged PNG, and returns structured `RENDER_TIMEOUT` JSON. The timeout regression deliberately cancels a 1024-square 65,536-sample render after one second.
- `scene.render_settings(max_bounces, seed, denoise)` exposes Cycles bounce depth, sampling seed, and CPU OIDN denoising. Denoising is available when Cycles is configured with `WITH_OPENIMAGEDENOISE=ON`; no GPU backend is built. Bounds are 0–64 bounces and a nonnegative 31-bit seed.
- Run `./examples/compose/run.sh` for lmcad-to-lmblender STL composition examples. It exports a mug, plate with holes, and U-channel bracket from lmcad, then renders the bracket from three static camera views and eight-angle mug/bracket turntable sets. STL carries tessellated geometry only; the renderer applies materials after import and assumes source coordinates use Cycles world units. Numeric turntable QA is in `tests/turntable_quality.py`; composition review notes are in `docs/MILESTONE3-QA.md`.
- `scene.render_exr(path, samples, width, height)` writes a linear 32-bit-float OpenEXR. `scene.render_passes(prefix, samples, width, height)` enables Cycles Combined, Z, Normal, and Diffuse Color (reported as albedo), writing each as a separate EXR plus a normalized PNG (`prefix-{combined,depth,normal,albedo}.{exr,png}`). Depth PNG maps near/far to white/black, normal PNG maps [-1,1] to [0,1], and combined PNG uses AgX. A Blender-backed regression checks monotonic depth across a ramp, unit normals, channel counts, and all eight outputs.
- `scene.set_view_transform(name)` selects `Standard`, `Filmic`, or `AgX` for PNG output. The host embeds 33³ RGB display LUTs baked from Blender 5.2's OCIO config and applies trilinear interpolation to scene-linear Cycles pixels; no OCIO runtime library or config is loaded. Values above scene-linear 16 are clamped. `test.sh` renders a neutral emission panel with both lmblender and Blender, then checks each transform's center-pixel RGB RMSE is at most 3/255 (currently 0.80, 0.73, and 0.80/255). EXR remains scene-linear.
- `mesh.translate(dx, dy, dz)` and `mesh.scale(sx, sy, sz)` set per-object transforms; `scene.instance_mesh(mesh, x, y, z, sx, sy, sz)` creates another object from the same DSL mesh. Source geometry is currently copied into separate Cycles mesh nodes: a prototype that shared a raw `Geometry *` using `Object::set_geometry` rendered a blank frame, while independent nodes pass the two-silhouette image regression. BlenderSync's datablock geometry map/sync tracking is not yet replicated; see [the sharing investigation](docs/INSTANCE-SHARING.md).
- `scene.image_material(path, roughness)` loads a bounded PNG/JPEG/EXR through Cycles' image-texture node. The current fixture verifies PNG decoding and cube-projected UVs; JPEG and EXR input fixtures and explicit colorspace controls remain unverified. Input files are limited to 64 MiB.
- A standalone static Cycles executable has been built at `build/cycles-static/bin/cycles` on the development host. It renders XML scenes to PNG with no `LD_LIBRARY_PATH` or `OCIO` environment and passes the `ldd: not a dynamic executable` check.
- A Cornell-box-style XML smoke scene rendered identically with static and dynamic Cycles at 128x128 and 16 samples (RGB MAD 0/255). The fixture uses emissive room panels and a blue box to check geometry/XML/image output; it is not a lighting parity fixture.
- OBJ/STL mesh handles render without launching another process. Production rendering uses the Cycles C++ node API; the XML reader is only a test oracle. Separate depth/normal/albedo passes are implemented. CPU denoising is linked through static OIDN archives and covered by the quality regression described below.

## Build and test

The scripts below build host-specific dependencies and Cycles from Blender's source checkout without changing that checkout. GCC 14 is used to match the existing Blender build.

```sh
./scripts/sync-sdk.sh --verify
./test.sh
BUILD_JOBS=2 LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" ./scripts/build-deps.sh
BUILD_JOBS=2 ./scripts/build-oidn-static.sh
BUILD_JOBS=2 LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" ./scripts/configure-cycles-static.sh
./tests/static-cycles-smoke.sh
ldd build/lmblender
ldd build/cycles-static/bin/cycles
```

`build-deps.sh` builds static TBB, Imath, OpenEXR, TIFF, OpenColorIO, and OpenImageIO. `configure-cycles-static.sh` configures the standalone Cycles target with Blender/Python/GUI/GPU, OpenVDB, OpenSubdiv, OIDN, OSL, USD, and Hydra disabled. It uses a build-only source overlay with a placeholder for the unavailable Git LFS `startup.blend`; the Blender source checkout remains clean.

The final Cycles ELF is fully static according to `file` and `ldd` (28,145,984 bytes in the current build). GNU ld warns that static glibc `getpwuid` and OIIO's plugin loader `dlopen` need shared libraries if those code paths are used. The verified PNG render works with `LD_LIBRARY_PATH` and `OCIO` unset; plugin loading beyond that path has not been tested.

The lmblender host and Cycles executable are statically linked (`ldd` reports “not a dynamic executable”). With static CPU OIDN enabled, the current `build/cycles-static/bin/cycles` is 79,072,920 bytes and `build/lmblender` is 80,538,088 bytes. The denoise regression renders a four-sample noisy image, the same scene with denoising, and a 64-sample reference: high-frequency residual falls from 4.265 to 0.815, silhouette IoU against reference is 0.9731, and median edge gradient is 22.34 versus 19.19 (86% retained). `./test.sh` also checks schema embedding, rendering examples, display transforms, and passes. The four-face tetra renders at 256x192 with 32 Cycles samples; direct construction has luminance standard deviation 36.05/255. The XML oracle uses a different emissive material and world, so that regression compares silhouette IoU (>=0.95) rather than color values.

`scripts/build-oidn-static.sh` builds OIDN 2.5 CPU archives with CUDA/HIP/SYCL disabled. `scripts/configure-cycles-static.sh` adapts OIDN 2.x archive names for Blender's legacy CMake finder, configures CPU OIDN and rebuilds Cycles. The host links the same static archives. Static glibc emits a warning about OIDN's module-loader `dlopen` path; the exercised CPU filter works without shared libraries. `tests/denoise_quality.py` uses an auditable residual, silhouette, and edge-gradient comparison.

## Dependencies

The VM and schema are pinned in `sdk.lock`; `scripts/sync-sdk.sh --verify` checks the vendored header against that pin. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for notices.
