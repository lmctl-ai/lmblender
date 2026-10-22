# Cycles mesh-instance sharing

## What failed

The standalone Cycles build uses GCC 14 `-O2` and its CPU device uses BVH2. On this aarch64 build, GCC's tree vectorizer miscompiled `BVH2::pack_instances`: adding the primitive offset to `int4.x` and `.y` also zeroed `.z` (visibility) and `.w` (primitive type). Instanced leaf nodes then had no valid primitive type, so all rays missed. Separate geometry rendered normally.

## Fix carried by this repo

`scripts/make-cycles-source-overlay.py` creates a disposable copy of `intern/cycles` under `build/cycles-static/blender-source-overlay` and applies `patches/cycles-bvh2-instance-sharing.patch` there. Other Blender `intern` directories remain symlinks. The patch builds the leaf with `make_int4(offset_x, offset_y, original_visibility, original_type)`. The script never edits the pinned Blender checkout (`BLENDER_SOURCE_TREE`); `git status --short` there stayed empty before and after the build.

`render_direct` shares a Cycles `Mesh` by `(source geometry, material, smooth flag)`, caches the material shader, keeps vertices in local space, and places translation/scale on each Cycles `Object`. Image-textured meshes remain per-object because the current cube-projection UVs depend on instance scale.

## Regression

`examples/mesh-instances.lms` renders two translated copies of one imported OBJ. `tests/png_instances.py` checks that the output contains exactly two connected foreground components of at least 10 pixels; the current measured components are `[309, 307]`. The unpatched BVH2 build returns a blank frame for this fixture. `test.sh` runs the same image and component checks.

## Other host builds checked

A regular integrated Blender build has `WITH_CYCLES_EMBREE=ON`, so its CPU path does not use this BVH2 instance merge. A standalone Cycles build with GCC 14 `-O2` and Embree off, but its XML reader's `xml_read_object` creates a fresh `Mesh` for each `<object>`; the standalone XML path does not share mesh geometry and therefore does not execute `pack_instances`. The patch is needed by this direct-node host when it shares a `ccl::Mesh`; it was not applied to either external build tree.

To rebuild the repaired Cycles BVH and host:

```sh
python3 scripts/make-cycles-source-overlay.py "${BLENDER_SOURCE_TREE:-build/blender-src-upstream-v5.2.2}" build/cycles-static/blender-source-overlay
cmake --build build/cycles-static --target cycles_bvh --parallel 2
cmake --build build --parallel 2
```
