# Instance geometry sharing investigation

The DSL `scene.instance_mesh` currently shares its source mesh handle, but the
Cycles scene builder creates a separate `ccl::Mesh` node for each object. This
is intentional until the direct-node path has a working geometry ownership and
update sequence.

## Evidence

The attempted prototype cached a `ccl::Mesh *` by source mesh and material,
called `Object::set_geometry` on each object, applied each object's transform,
called `Mesh::compute_bounds()`, and tagged the shared geometry both before
and after attaching all object users. `examples/mesh-instances.lms` still
rendered a uniform background (RGB 67,75,88 everywhere). After the render,
Cycles reported two mesh objects with the same geometry, valid shared geometry
bounds `[0,0,0]..[2,3,4]`, and object bounds `[-1.8,0,0]..[-0.5,1.95,2.6]` and
`[1.8,0,0]..[3.1,1.95,2.6]`. This rules out missing geometry bounds and
instance transforms as the immediate cause, but does not identify the BVH or
shader lifecycle failure. Returning to fresh mesh nodes per object restores
both visible disconnected silhouettes (`tests/png_instances.py`: component
areas 309 and 307 pixels at 256x192). The shared prototype was reverted, so
production rendering remains correct and geometry sharing is still pending.

Blender's Cycles integration has more lifecycle machinery than this host's
direct C++ construction: `intern/cycles/blender/geometry.cpp` builds a
`GeometryKey`, looks up `geometry_map`, avoids duplicate syncs using
`geometry_synced`, and updates the map before returning geometry. Then
`intern/cycles/blender/object.cpp::sync_object` attaches the returned geometry
to an object. The standalone renderer bypasses `BlenderSync`; these source
references suggest the missing behavior is lifecycle/synchronization rather
than the `set_geometry` API alone. This is an inference from the integration
path plus the prototype experiments, not a proven Cycles limitation.

## Static OIDN integration

`BUILD_JOBS=2 ./scripts/build-oidn-static.sh` configures CPU-only OIDN 2.5 with
CUDA, HIP, and SYCL disabled, then links and runs the OIDN RT filter in a fully
static executable. The verified smoke binary is 53,634,528 bytes;
`ldd` reports `not a dynamic executable`, and the filter returned a finite
center pixel (`0.400401`). The linker warns that OIDN's module-loader object
uses `dlopen`; the exercised CPU RT path did not require a shared library.

The static archive was then integrated into standalone Cycles and the host.
`scene.render_settings(..., denoise=true)` now exercises the CPU RT filter.
The end-to-end quality metrics and current ELF sizes are recorded in the
top-level README; the smoke binary above is only the earlier standalone OIDN
feasibility measurement.
