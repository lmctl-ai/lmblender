# BlenderCLI interchange

The paired `examples/bridge/build-and-export.lms` script in the BlenderCLI
checkout creates a three-object `.blend` scene through worker operations and
exports `bridge.glb`. Run that script first (set `BLENDERCLI_REPO` when using
`run.sh` with a checkout in another location). Then run this directory's
`render-export.lms` from the lmblender checkout. It imports
all triangle meshes, node transforms, base-color/metallic/roughness factors,
UVs/normals, and `KHR_materials_emissive_strength`, then renders at 640x480.

The bridge currently recreates camera, lights, and world in the lmblender
script. The BlenderCLI GLB exporter bakes object transforms into world-space
vertices and does not yet preserve parent hierarchy, Blender cameras/lights,
or world settings. The examples therefore demonstrate usable geometry and PBR
transfer, not pixel parity. Cross-render numeric comparisons should use a
matched rig; the native Blender render and standalone Cycles output have
different lighting/color-management paths.
