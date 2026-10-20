# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The host embeds the pinned DSL v5 VM and schema. Blender's standalone Cycles source is built separately, without Blender, Python, GUI, or GPU backends.

## Current milestone status

- The static DSL host exposes `scene.version()`, `scene.import_obj(path)`, and `scene.import_stl(path)`; imported meshes expose vertex/triangle counts and bounds. `scene.render_png(path, samples, width, height)` builds Cycles `Scene`, `Mesh`, `Object`, `ShaderGraph`, camera, and light nodes directly through the Cycles C++ API and renders them with an in-process CPU session. The renderer uses a fitted Z-up perspective camera by default, and assigns Cycles emission shaders to direct-light nodes so they participate in the light manager.
- `scene.material(r, g, b, metallic, roughness, transmission, emission_r, emission_g, emission_b, emission_strength)` creates a Principled BSDF material. `scene.checker_material(...)` and `scene.noise_material(...)` connect Cycles procedural nodes into Principled base color. `scene.image_material(path, roughness)` loads a PNG/JPEG/EXR via the static OIIO path, uses per-corner cube-projected UV coordinates, and caps the input at 64 MiB; PNG has a real-render regression fixture. Attach any with `mesh.set_material(material)`. Authored UV maps and texture color-space controls remain planned.
- Lighting bindings include `scene.world_color(r, g, b, strength)`, point, area, sun, and spot lights. `scene.area_light(px, py, pz, tx, ty, tz, r, g, b, strength, size)`, `scene.sun_light(dx, dy, dz, r, g, b, strength, angle_degrees)`, and `scene.spot_light(px, py, pz, tx, ty, tz, r, g, b, strength, angle_degrees, blend)` map to Cycles light nodes. `scene.ground_plane(z, size, r, g, b)` adds a diffuse XY floor; `scene.wall_plane_y(y, size, r, g, b)` adds a lightly self-lit backdrop; `scene.fit_camera(fov_degrees)` defers its aspect-aware fit until render dimensions are known, uses transformed non-studio bounds, and compensates the horizontal sensor basis. The automatic point light is omitted when explicit point lights are configured. Environment textures remain planned.
- `scene.camera_look_at(px, py, pz, tx, ty, tz, fov_degrees)` sets perspective. `scene.camera_orthographic(px, py, pz, tx, ty, tz, width)` sets an orthographic camera; the projection example numerically compares its framing with perspective. Panoramic projection, DOF, and camera instancing are still planned.
- Render inputs are bounded to 65,536 samples, 4096x4096 dimensions and 16 million pixels, 10 million vertices per mesh, 20 million triangles total, 4096 imported meshes/materials, and 1024 point lights. `scene.render_timeout(seconds)` sets the Cycles session wall-clock limit (1–3600 seconds; default 3600); timeout cancels the render, removes its staged PNG, and returns structured `RENDER_TIMEOUT` JSON. The timeout regression deliberately cancels a 1024-square 65,536-sample render after one second.
- `scene.render_settings(max_bounces, seed, denoise)` exposes Cycles bounce depth and sampling seed; denoise must be `false` because this standalone build has no denoiser enabled. Bounds are 0–64 bounces and a nonnegative 31-bit seed.
- Run `./examples/compose/run.sh` for lmcad-to-lmblender STL composition examples. It exports a mug, plate with holes, and U-channel bracket from lmcad, then renders the bracket from three static camera views. STL carries tessellated geometry only; the renderer applies materials after import and assumes source coordinates use Cycles world units. QA results are recorded in `docs/MILESTONE3-QA.md`.
- `scene.render_exr(path, samples, width, height)` writes a linear 32-bit-float OpenEXR using the same bounded, atomic render path as PNG. A header regression verifies OpenEXR magic and the stored 128x96 data window. Separate depth/normal/albedo passes, Standard/Filmic/AgX display transforms, and CPU denoising are still pending.
- `mesh.translate(dx, dy, dz)` and `mesh.scale(sx, sy, sz)` set per-object transforms; `scene.instance_mesh(mesh, x, y, z, sx, sy, sz)` creates another object from the same DSL mesh. The current Cycles scene builder copies source geometry for each object rather than sharing one Cycles geometry node. The image regression segments the render and requires two visible disconnected silhouettes.
- `scene.image_material(path, roughness)` loads a bounded PNG/JPEG/EXR through Cycles' image-texture node. The current fixture verifies PNG decoding and cube-projected UVs; JPEG and EXR input fixtures and explicit colorspace controls remain unverified. Input files are limited to 64 MiB.
- A standalone static Cycles executable has been built at `build/cycles-static/bin/cycles` on the development host. It renders XML scenes to PNG with no `LD_LIBRARY_PATH` or `OCIO` environment and passes the `ldd: not a dynamic executable` check.
- A Cornell-box-style XML smoke scene rendered identically with static and dynamic Cycles at 128x128 and 16 samples (RGB MAD 0/255). The fixture uses emissive room panels and a blue box to check geometry/XML/image output; it is not a lighting parity fixture.
- OBJ/STL mesh handles can now reach a PNG render without launching another process. Production rendering constructs scenes through the Cycles C++ node API. The XML reader is retained only as a test oracle. The direct node API supports checker/noise/image materials and orthographic projection. Separate depth/normal/albedo passes and Standard/Filmic/AgX display transforms are pending. CPU denoising is disabled: the host has only shared OIDN libraries under `/home/ubuntu/blender-deps/install/openimagedenoise`, not a static OIDN archive in the static prefix, so it cannot be linked into the current fully static host without rebuilding that dependency.
- `scene.instance_mesh` currently creates separate Cycles mesh nodes; sharing geometry across instances remains pending. A prototype that reused the same geometry node produced a blank frame, so it was reverted rather than claiming a working optimization. The composition gallery currently covers mug and plate-with-holes; bracket and multi-angle turntable scenes remain pending.

## Build and test

The scripts below build host-specific dependencies and Cycles from Blender's source checkout without changing that checkout. GCC 14 is used to match the existing Blender build.

```sh
./scripts/sync-sdk.sh --verify
./test.sh
BUILD_JOBS=2 LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" ./scripts/build-deps.sh
BUILD_JOBS=2 LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" ./scripts/configure-cycles-static.sh
./tests/static-cycles-smoke.sh
ldd build/lmblender
ldd build/cycles-static/bin/cycles
```

`build-deps.sh` builds static TBB, Imath, OpenEXR, TIFF, OpenColorIO, and OpenImageIO. `configure-cycles-static.sh` configures the standalone Cycles target with Blender/Python/GUI/GPU, OpenVDB, OpenSubdiv, OIDN, OSL, USD, and Hydra disabled. It uses a build-only source overlay with a placeholder for the unavailable Git LFS `startup.blend`; the Blender source checkout remains clean.

The final Cycles ELF is fully static according to `file` and `ldd` (28,145,984 bytes in the current build). GNU ld warns that static glibc `getpwuid` and OIIO's plugin loader `dlopen` need shared libraries if those code paths are used. The verified PNG render works with `LD_LIBRARY_PATH` and `OCIO` unset; plugin loading beyond that path has not been tested.

The lmblender host is statically linked too (`ldd build/lmblender` reports “not a dynamic executable”; current host size is recorded by `stat -c %s build/lmblender`). `./test.sh` compiles the host, runs the DSL examples and validates decoded PNGs have visible pixel variation. The four-face tetra renders at 256x192 with 32 Cycles samples; direct construction has luminance standard deviation 36.05/255. For the integration oracle, the XML and direct renders have object-pixel mean absolute RGB error below 2/255; their full-frame MAD is higher because the XML fixture's unlinked background differs from the explicit direct background shader. XML parsing is retained only behind `LMBLENDER_TEST_XML_ORACLE` for this regression test. Scene/material authoring and render settings will continue moving to direct Cycles API bindings incrementally.

## Dependencies

The VM and schema are pinned in `sdk.lock`; `scripts/sync-sdk.sh --verify` checks the vendored header against that pin. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for notices.
