# Instance geometry sharing investigation

The DSL `scene.instance_mesh` currently shares its source mesh handle, but the
Cycles scene builder creates a separate `ccl::Mesh` node for each object. This
is intentional until the direct-node path has a working geometry ownership and
update sequence.

## Evidence

The attempted prototype cached a `ccl::Mesh *` by source mesh, called
`Object::set_geometry` on each object, and tried `Geometry::tag_update(scene,
false)` both before and after object attachment. In every shared-node variant,
`examples/mesh-instances.lms` rendered a blank image. Returning to a fresh mesh
node per object restored both visible disconnected silhouettes; the existing
`test.sh` / `tests/png_instances.py` regression checks that result. The shared
prototype was reverted, so the production behavior remains correct and the
optimization is not claimed as complete.

Blender's Cycles integration has more lifecycle machinery than this host's
direct C++ construction: `intern/cycles/blender/geometry.cpp` builds a
`GeometryKey`, looks up `geometry_map`, avoids duplicate syncs using
`geometry_synced`, and updates the map before returning geometry. Then
`intern/cycles/blender/object.cpp::sync_object` attaches the returned geometry
to an object. The standalone renderer bypasses `BlenderSync`; these source
references suggest the missing behavior is lifecycle/synchronization rather
than the `set_geometry` API alone. This is an inference from the integration
path plus the prototype experiments, not a proven Cycles limitation.

## Static OIDN feasibility

`BUILD_JOBS=2 ./scripts/build-oidn-static.sh` configures CPU-only OIDN 2.5 with
CUDA, HIP, and SYCL disabled, then links and runs the OIDN RT filter in a fully
static executable. The verified smoke binary is 53,634,528 bytes;
`ldd` reports `not a dynamic executable`, and the filter returned a finite
center pixel (`0.400401`). The linker warns that OIDN's module-loader object
uses `dlopen`; the exercised CPU RT path did not require a shared library.

This establishes that OIDN can be linked statically on this host. It does not
enable denoising in lmblender: current standalone Cycles was configured with
OIDN disabled, so Cycles must be rebuilt against these static archives and its
denoising API wired to `scene.render_settings` before the DSL option can be
enabled.
