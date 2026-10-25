# Build notes

This is the Linux aarch64/x86-64 static build recipe prepared for the public CI workflow. It assumes Ubuntu 24.04, GCC 14, Ninja, CMake, Python 3, npm/lmscript, curl, and the normal Blender dependency build tools. Use `-j2` on constrained hosts.

## Host dependency locations

Install the native tools on Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build gcc-14 g++-14 git curl ca-certificates xz-utils patch flex bison python3 npm pkg-config libtinfo-dev libncurses-dev libssl-dev
```

Dependency sources live under `build/blender-deps-environment/build` and harvested libraries under `build/blender-deps-harvest` (override with `BLENDER_DEPS_SOURCE_ROOT` and `BLENDER_DEPS_INSTALL`). A fresh build first fetches the pinned upstream Blender checkout and runs `scripts/bootstrap-static-deps.sh`, which populates the same dependency source layout with Blender's pinned dependency versions, downloads the SHA-256-verified OIDN and ISPC inputs, and harvests the support libraries. `scripts/build-deps.sh` then builds the static OIIO/OCIO/OpenEXR/TBB prefix. The host's Blender checkout is not modified by the Cycles overlay.

## Build Cycles and lmblender

```sh
npm install --global @lmctl-ai/lmctl@0.2.420
scripts/sync-sdk.sh --verify
BLENDER_SOURCE_TREE="$PWD/build/blender-src-upstream-v5.2.2" scripts/fetch-cycles-source.sh
BLENDER_SOURCE_TREE="$PWD/build/blender-src-upstream-v5.2.2" \
  BLENDER_DEPS_ENV_BUILD="$PWD/build/blender-deps-environment" \
  BLENDER_DEPS_INSTALL="$PWD/build/blender-deps-harvest" \
  BLENDER_LIBDIR="$PWD/build/blender-deps-harvest" \
  LMBLENDER_CXX=/usr/bin/g++-14 LMBLENDER_CC=/usr/bin/gcc-14 BUILD_JOBS=2 \
  scripts/bootstrap-static-deps.sh
BLENDER_DEPS_SOURCE_ROOT="$PWD/build/blender-deps-environment/build" \
  BLENDER_DEPS_INSTALL="$PWD/build/blender-deps-harvest" \
  LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" BUILD_JOBS=2 scripts/build-deps.sh
OIDN_SOURCE="$PWD/build/blender-deps-environment/build/openimagedenoise/src/external_openimagedenoise" \
  ISPC_EXECUTABLE="$(cat build/ispc-executable.txt)" \
  BUILD_JOBS=2 scripts/build-oidn-static.sh
BLENDER_SOURCE_TREE="$PWD/build/blender-src-upstream-v5.2.2" \
  BLENDER_DEPS_INSTALL="$PWD/build/blender-deps-harvest" \
  BLENDER_LIBDIR="$PWD/build/blender-deps-harvest" \
  LMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  LMBLENDER_DEPS_BUILD_DIR="$PWD/build/deps-gcc14" \
  LMBLENDER_CYCLES_BUILD_DIR="$PWD/build/cycles-static" BUILD_JOBS=2 \
  scripts/configure-cycles-static.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLMBLENDER_FULL_STATIC=ON \
  -DBLENDER_SOURCE_TREE="$PWD/build/blender-src-upstream-v5.2.2" \
  -DCYCLES_BUILD_DIR="$PWD/build/cycles-static" \
  -DLMBLENDER_STATIC_PREFIX="$PWD/build/static-prefix-gcc14" \
  -DBLENDER_DEPS_INSTALL="$PWD/build/blender-deps-harvest" \
  -DBLENDER_LIBDIR="$PWD/build/blender-deps-harvest"
cmake --build build --parallel 2
./test.sh
ldd build/lmblender
```

The `ldd` result should say `not a dynamic executable`. The host links CPU Cycles, OIIO, OCIO, OpenEXR/Imath, TBB, CPU OIDN, image codecs, pugixml, and the selected Blender intern support archives. Optional features and components are listed in `docs/LICENSE_AUDIT.md`.

## Reproducing dependency sources elsewhere

`scripts/fetch-cycles-source.sh` fetches upstream `blender/blender` v5.2.2 commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c` and sparsely checks out the CMake/Cycles/allocator/sky/dependency build files used by this build. It skips `release/datafiles`: that tree contains LFS assets that are not needed for the standalone renderer and the pinned repository has a missing LFS object. It does not use any other Blender checkout; set `BLENDER_SOURCE_TREE` to reuse an existing clone of the same commit.

The GitHub Actions workflow is at `.github/workflows/ci.yml`. It uses pinned action SHAs, an architecture/compiler/flags-keyed cache, two CPU threads, tests the static link with `ldd`, and attaches bare binaries and tarballs to draft tag releases. The workflow and bootstrap are syntax-checked here but have not yet been run on GitHub Actions; cold-cache and both runner architectures remain unverified. Do not treat a local cache hit as proof of a clean-room build.

## Motion workaround overlay

`scripts/make-cycles-source-overlay.py` applies the reviewed patches in `patches/` only inside the disposable build overlay. The upstream source checkout stays untouched. The current overlay includes the GCC 14/aarch64 BVH and motion interpolation workarounds; see `docs/RELINKING.md` and `docs/LICENSE_AUDIT.md`.
