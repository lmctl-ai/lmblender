## Unreleased

Primitive meshes now carry box-projected UVs instead of a single top-down XY projection, so an
`image_material` on `cube` (and on cylinder/cone sides) shows the image on every face rather than a
smeared average; planes keep their previous mapping. `tests/primitive_uv_quality.py` covers it.

`instance_mesh` copies of a smooth-shaded source (for example `uv_sphere(..., true)`) now render
smooth like the source instead of flat-shaded; `tests/instance_smooth_quality.py` covers it. Examples
write `images/<name>.png` and run themselves, and twelve new story examples form the README gallery.

## lmblender 0.3.6 — Linux, macOS and Windows (arm64/x64)

Make generated OBJ/MTL regression fixtures byte-stable on Windows by writing
explicit LF line endings. The shared fixture comparison now passes consistently
across platforms and includes a direct newline stability assertion. Axis rotation
also updates imported corner normals and rejects direct rotation of shared instances.

## lmblender 0.3.5 — Linux, macOS and Windows (arm64/x64)

Adds bounded baked Y-axis and Z-axis mesh rotations alongside the existing X-axis
rotation. This makes rigid-body and scientific assembly scenes orientable without
pre-transforming source geometry; a render regression covers all three axes.

## lmblender 0.3.4 — Linux, macOS and Windows (arm64/x64)

First release with `lmblender-macos-arm64`, `lmblender-macos-x64` and
`lmblender-windows-x64.exe` beside the Linux binaries. macOS binaries link only
system frameworks, `libSystem` and `libc++`; the Windows executable imports only
`KERNEL32.DLL` and `ADVAPI32.DLL`. Also: hair strands (`strand`, `hair_sphere`,
`hair_patch`), convex-profile `loft`, `toon_outline`, a 287-case hostile-input
corpus, and one merged `SHA256SUMS` over every asset. A failing optional
platform never blocks or corrupts the Linux assets.

## lmblender 0.3.3 — Linux arm64 and x64

Static Embree CPU traversal; OBJ `.mtl` materials and texture maps; authored glTF
`TEXCOORD_0` for textured meshes; output-path policy (`LMBLENDER_OUTPUT_ROOT`,
symlink refusal); scene geometry budget, decoded-image dimension checks and a
wall-clock/instruction ceiling per program; fallback key light only for scenes
without lights; `auto_frame:false` presets require an explicit camera; one
`SHA256SUMS` over every release asset and verified corresponding-source
archives. macOS and Windows builds are experimental in CI and not yet released.

## lmblender 0.3.2 — Linux arm64 and x64

Adopts the lmscript host protocol: `--dsl-schema` prints a human summary and
`--dsl-schema --json` prints the byte-exact embedded schema; the legacy
`--schema` output remains available. All lmblender `.lms` examples use a pinned
`cli=`/`install=` declaration and run the selected executable through
`dsl_cli(p)`. Release staging uses the public `lmctl-ai/lmblender` repository,
Linux host/architecture bare binary names, checksums, notices, and a source
offer. Presets now yield to later camera calls and replace their generated
area-light rig when the first explicit light is added. Also includes GLB
emissive-strength import and the BlenderCLI bridge, strand, spot, and light
falloff examples.

## lmblender 0.3.1 — Linux arm64

Audits OIDN CPU throughput after a 6.31 s denoise measurement. The static OIDN 2.5 build uses optimized AArch64 NEON ISPC kernels and TBB; a controlled Cycles run measured 0.91 s at 960x540, and the OIDN device/filter are reused for the session. The earlier slow result was not reproducible. Adds standalone quality-level timings and 64/128-spp no-denoise comparisons: on a glossy test scene, 16-spp fast OIDN is both faster and passes the SSIM gate where the no-denoise choices do not. Retains the measured 1280x720 diagram final preset at approximately 2.00 s/frame on the simple emissive benchmark.

See `docs/OIDN_AUDIT.md` and `docs/THROUGHPUT.md` for build flags, reproduction steps, quality metrics, and workload limits. This is a static Linux arm64 draft; render duration varies with scene complexity and host load.

## lmblender 0.3.0 — Linux arm64

Adds measured homepage-video presets: 960x540 draft at 8 spp/no denoise (0.41 s/frame), and 1280x720 final at 16 spp/fast CPU OIDN (2.00 s/frame on the simple emissive benchmark). Adds a selectable OIDN quality binding and a regression against a 256-spp high-quality reference (47.14 dB PSNR, 0.99451 windowed SSIM at 640x360). The render profiler now reports scene build, BVH/session reset, path tracing, OIDN, PNG transform/encoding, and total time. Full-check passed: 14 gallery dimensions, 4 video sequences, 18/18 visual QA, schema, render/import tests, and static linkage.

Sequences currently create a new Cycles session per frame. A transform-update reuse prototype failed image parity (stale transforms; forcing a full reset removed the world background), so it is documented but not shipped. Static Embree 4.4.1 is now enabled for CPU BVH and ray traversal on arm64 and x64; see [THROUGHPUT.md](THROUGHPUT.md) for the measured dense-mesh, glass, and hair results.

## lmblender 0.2.2 — Linux arm64

Fixes Cycles camera pixel-aspect handling. Perspective `fov_degrees` is now
explicitly vertical FOV, and orthographic scale is the visible vertical span.
Sphere/cube silhouette regressions cover 16:9, 4:3, 1:1, and 9:16, including
the `render_sequence` path. Camera geometry now remains isotropic within 2%.

## lmblender 0.2.1 — Linux arm64

Static CPU Cycles host with direct C++ scene construction, OIDN denoising, bounded OBJ/STL/PLY/glTF mesh import, material image maps and layered shaders, area-light shapes, spot softness, display transforms, transform motion blur, and automatic imported-model studio lighting/framing.

Verified on aarch64 Linux: `ldd` reports `not a dynamic executable`; `test.sh` passes the complete render, import, schema-signature, image-quality, and Blender comparison suite. The draft archive includes the DSL v5 schema, LLM API guide, animation guide, third-party notices, and license texts.
