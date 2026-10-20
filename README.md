# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The host embeds the pinned DSL v5 VM and schema. Blender's standalone Cycles source is built separately, without Blender, Python, GUI, or GPU backends.

## Current milestone status

| Area | Status | Current support / gap |
| --- | --- | --- |
| CPU Cycles render | Done | Static executable, direct C++ scene construction, PNG and EXR, CPU OIDN denoise |
| Materials and lights | Partial | Principled, checker/noise/ramp/bump/image textures, emission, point/area/sun/spot; glass, subsurface, and volume have paired numeric render regressions; roughness/metallic maps, layered shaders, and arbitrary node graphs remain unsupported |
| World | Partial | Color background, Cycles multiple-scattering sky, and equirectangular PNG/EXR/HDR environment node with rotation/strength |
| Camera and motion | Partial | Perspective/orthographic, look-at/fitting, and Cycles aperture/focus DOF; animated-transform motion blur is not implemented (direct CCL motion prototype crashed in BVH motion interpolation) |
| Animation | Partial | Keyed mesh translation, camera position/target, and material base color with linear/ease interpolation; deterministic PNG sequences and atomic APNG (rational 1/fps delay); mesh rotation/scale, other material parameters, and motion blur remain unsupported |
| Mesh input | Partial | OBJ, STL, ASCII PLY, and glTF 2.0 JSON/GLB triangle meshes with node transforms, PBR base-color factor, and external PNG/JPEG base-color textures; binary PLY and embedded glTF images remain unsupported |
| Gallery QA | Partial | 12 curated renders pass numeric checks and agy/Gemini visual checklists at 640×480; lmcad compositions are checked separately |
| Alpha output | Partial | Transparent world film emits RGBA PNG; shadow-catcher composition remains |

- The static DSL host exposes `scene.version()`, `scene.import_obj(path)`, `scene.import_stl(path)`, `scene.import_ply(path)`, and `scene.import_gltf(path)`. PLY currently accepts bounded ASCII 1.0 meshes with x/y/z vertices and polygon face indices. glTF 2.0 `.gltf` and `.glb` import traverses the selected scene hierarchy, bakes node transforms into render meshes, preserves TEXCOORD_0, reads metallic/roughness and baseColorFactor, and supports external PNG/JPEG base-color images confined to the glTF directory. Imported meshes expose vertex/triangle counts and bounds. Sparse accessors, non-triangle modes, required extensions, skinning, morph targets, and embedded/data-URI images are rejected with structured glTF errors. Inputs are bounded to 256 MiB glTF containers, 512 MiB aggregate buffers, 64 MiB per texture, 10 million decoded vertices, 20 million triangles, 4096 generated meshes/materials, and 200,000 hierarchy visits. `scene.render_png(path, samples, width, height)` builds Cycles `Scene`, `Mesh`, `Object`, `ShaderGraph`, camera, and light nodes directly through the Cycles C++ API and renders them with an in-process CPU session. The renderer uses a fitted Z-up perspective camera by default, and assigns Cycles emission shaders to direct-light nodes so they participate in the light manager.
- `scene.material(r, g, b, metallic, roughness, transmission, emission_r, emission_g, emission_b, emission_strength)` creates a Principled BSDF material. `scene.checker_material(...)`, `scene.noise_material(...)`, and `scene.ramp_material(...)` connect Cycles procedural nodes into Principled base color; `material.bump(strength, distance, scale)` adds a Noise → Bump normal network. `scene.image_material(path, roughness)` loads a PNG/JPEG/EXR via the static OIIO path, uses per-corner cube-projected UV coordinates, and caps the input at 64 MiB; PNG has a real-render regression fixture. Attach any with `mesh.set_material(material)`. Authored UV maps, texture color-space controls, roughness/metallic maps, and layered/mix shaders remain planned.
- Lighting bindings include `scene.world_color(r, g, b, strength)`, point, area, sun, and spot lights. `scene.sky_texture(elevation_degrees, rotation_degrees, intensity)` uses Cycles' multiple-scattering sky node as a world background; `examples/sky-texture.lms` checks the vertical luminance gradient and clipped-pixel ratio. `scene.environment_texture(path, strength, rotation)` loads bounded equirectangular PNG/EXR/HDR maps into a Cycles environment node. `scene.area_light`, `scene.sun_light`, and `scene.spot_light` map to Cycles light nodes. Ground/backdrop helpers and aspect-aware fitted camera are also provided.
- `scene.camera_look_at(px, py, pz, tx, ty, tz, fov_degrees)` sets perspective. `scene.camera_orthographic(px, py, pz, tx, ty, tz, width)` sets an orthographic camera; `scene.camera_dof(aperture, focus_distance)` enables aperture DOF and is covered by a per-object edge-sharpness regression. Panoramic projections and camera instancing remain unsupported.
- Render inputs are bounded to 65,536 samples, 4096x4096 dimensions and 16 million pixels, 10 million vertices per mesh, 20 million triangles total, 4096 imported meshes/materials, and 1024 point lights. `scene.render_timeout(seconds)` sets the Cycles session wall-clock limit (1–3600 seconds; default 3600); timeout cancels the render, removes its staged PNG, and returns structured `RENDER_TIMEOUT` JSON. The timeout regression deliberately cancels a 1024-square 65,536-sample render after one second.
- `scene.render_settings(max_bounces, seed, denoise)` exposes Cycles bounce depth, sampling seed, and CPU OIDN denoising. Denoising is available when Cycles is configured with `WITH_OPENIMAGEDENOISE=ON`; no GPU backend is built. Bounds are 0–64 bounces and a nonnegative 31-bit seed.
- Run `./examples/compose/run.sh` for lmcad-to-lmblender STL composition examples. It exports a mug, plate with holes, and U-channel bracket from lmcad, then renders the bracket from three static camera views and eight-angle mug/bracket turntable sets. STL carries tessellated geometry only; the renderer applies materials after import and assumes source coordinates use Cycles world units. Numeric turntable QA is in `tests/turntable_quality.py`; composition review notes are in `docs/MILESTONE3-QA.md`.
- `scene.render_exr(path, samples, width, height)` writes a linear 32-bit-float OpenEXR. `scene.render_passes(prefix, samples, width, height)` enables Cycles Combined, Z, Normal, and Diffuse Color (reported as albedo), writing each as a separate EXR plus a normalized PNG (`prefix-{combined,depth,normal,albedo}.{exr,png}`). Depth PNG maps near/far to white/black, normal PNG maps [-1,1] to [0,1], and combined PNG uses AgX. A Blender-backed regression checks monotonic depth across a ramp, unit normals, channel counts, and all eight outputs.
- `scene.set_view_transform(name)` selects `Standard`, `Filmic`, or `AgX` for PNG output. The host embeds 33³ RGB display LUTs baked from Blender 5.2's OCIO config and applies trilinear interpolation to scene-linear Cycles pixels; no OCIO runtime library or config is loaded. Values above scene-linear 16 are clamped. `test.sh` renders a neutral emission panel with both lmblender and Blender, then checks each transform's center-pixel RGB RMSE is at most 3/255 (currently 0.80, 0.73, and 0.80/255). EXR remains scene-linear.
- `mesh.translate(dx, dy, dz)` and `mesh.scale(sx, sy, sz)` set per-object transforms; `scene.instance_mesh(mesh, x, y, z, sx, sy, sz)` shares the same Cycles geometry between scene objects and passes a nonblank two-instance pixel test. glTF scene-node transforms are currently baked into each generated render mesh; general DSL object instances use shared geometry.
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

The lmblender host and Cycles executable are statically linked (`ldd` reports “not a dynamic executable”). The current host binary is 80,861,224 bytes (77.1 MiB); `lmblender --version` reports Cycles 4.5.0 and OpenImageDenoise 2.5.0. The latest full check recorded in [docs/STATUS.md](docs/STATUS.md) gives exact binary sizes. With static CPU OIDN enabled, the denoise regression renders a four-sample noisy image, the same scene with denoising, and a 64-sample reference: high-frequency residual falls from 4.040 to 0.832, silhouette IoU against reference is 0.9797, and median edge gradient is 21.57 versus 16.43 (76% retained). `./test.sh` checks schema embedding, mesh/glTF imports, numeric rendering examples, display transforms, passes, and denoising. `./scripts/full-check.sh` adds the static-linkage, 640×480 image, and per-image QA gates and records its latest result to `docs/STATUS.md`.

`scripts/build-oidn-static.sh` builds OIDN 2.5 CPU archives with CUDA/HIP/SYCL disabled. `scripts/configure-cycles-static.sh` adapts OIDN 2.x archive names for Blender's legacy CMake finder, configures CPU OIDN and rebuilds Cycles. The host links the same static archives. Static glibc emits a warning about OIDN's module-loader `dlopen` path; the exercised CPU filter works without shared libraries. `tests/denoise_quality.py` uses an auditable residual, silhouette, and edge-gradient comparison.

See [docs/LLM_GUIDE.md](docs/LLM_GUIDE.md) for the complete DSL signature list, parameter units, limits, recipes, and error recovery; [docs/ANIMATION.md](docs/ANIMATION.md) documents frame sequences and APNG limits.

## Dependencies

The VM and schema are pinned in `sdk.lock`; `scripts/sync-sdk.sh --verify` checks the vendored header against that pin. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for notices.
