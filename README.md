# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The host embeds the pinned DSL v5 VM and schema. Blender's standalone Cycles source is built separately, without Blender, Python, GUI, or GPU backends.

## Current milestone status

- The `lmblender` DSL host currently exposes `scene.version()` and is a static executable.
- A standalone static Cycles executable has been built at `build/cycles-static/bin/cycles` on the development host. It renders XML scenes to PNG with no `LD_LIBRARY_PATH` or `OCIO` environment and passes the `ldd: not a dynamic executable` check.
- A Cornell-box-style XML smoke scene rendered identically with static and dynamic Cycles at 128x128 and 16 samples (RGB MAD 0/255). The fixture uses emissive room panels and a blue box to check geometry/XML/image output; it is not a lighting parity fixture.
- DSL bindings that construct and render scenes directly through the Cycles C++ API, including the first OBJ/STL import binding, remain to be implemented.

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

## Dependencies

The VM and schema are pinned in `sdk.lock`; `scripts/sync-sdk.sh --verify` checks the vendored header against that pin. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for notices.
