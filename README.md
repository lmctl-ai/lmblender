# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The host embeds the pinned DSL v5 VM and schema. Blender's standalone Cycles source is built separately, without Blender, Python, GUI, or GPU backends.

## Current milestone status

- The static DSL host exposes `scene.version()`, `scene.import_obj(path)`, and `scene.import_stl(path)`; imported meshes expose vertex/triangle counts and bounds. `scene.render_png(path, samples, width, height)` builds Cycles `Scene`, `Mesh`, `Object`, `ShaderGraph`, camera, and light nodes directly through the Cycles C++ API and renders them with an in-process CPU session. The output path is published from a temporary file, and the current renderer uses a simple default material/light/camera setup.
- `scene.material(r, g, b, metallic, roughness, transmission, emission_r, emission_g, emission_b, emission_strength)` creates a Principled BSDF material. Attach it with `mesh.set_material(material)`. Channels and metallic/roughness/transmission are bounded to `[0,1]`; emission strength is `[0,10000]`. The first binding deliberately exposes scalar channels rather than DSL arrays; procedural and image textures are still planned.
- Lighting bindings include `scene.world_color(r, g, b, strength)` and `scene.point_light(x, y, z, r, g, b, strength)`. Point lights use Cycles power units; the default light is used when no explicit point lights were added. Sun/spot/area lights and camera controls remain planned.
- `scene.camera_look_at(px, py, pz, tx, ty, tz, fov_degrees)` sets a perspective camera pose and field of view. Its regression example checks that the rendered foreground centroid stays within 12% of the image center. Orthographic/panoramic projection, DOF, and camera instancing are still planned.
- Render inputs are bounded to 65,536 samples, 4096x4096 dimensions and 16 million pixels, 10 million vertices per mesh, 20 million triangles total, 4096 imported meshes/materials, and 1024 point lights. `scene.render_timeout(seconds)` sets the Cycles session wall-clock limit (1–3600 seconds; default 3600); timeout cancels the render, removes its staged PNG, and returns structured `RENDER_TIMEOUT` JSON. The timeout regression deliberately cancels a 1024-square 65,536-sample render after one second.
- A standalone static Cycles executable has been built at `build/cycles-static/bin/cycles` on the development host. It renders XML scenes to PNG with no `LD_LIBRARY_PATH` or `OCIO` environment and passes the `ldd: not a dynamic executable` check.
- A Cornell-box-style XML smoke scene rendered identically with static and dynamic Cycles at 128x128 and 16 samples (RGB MAD 0/255). The fixture uses emissive room panels and a blue box to check geometry/XML/image output; it is not a lighting parity fixture.
- OBJ/STL mesh handles can now reach a PNG render without launching another process. The current vertical slice writes a bounded Cycles XML scene and loads it through Cycles' native in-process XML reader before rendering with the C++ session API. Direct per-object scene construction through Cycles C++ node APIs remains future work; it needs follow-up on the initial API path, which built but rendered only the background.

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
