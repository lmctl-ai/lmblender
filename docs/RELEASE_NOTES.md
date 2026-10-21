## lmblender 0.3.0 — Linux arm64

Adds measured homepage-video presets: 960x540 draft at 8 spp/no denoise (0.41 s/frame), and 1280x720 final at 16 spp/fast CPU OIDN (2.00 s/frame on the simple emissive benchmark). Adds a selectable OIDN quality binding and a regression against a 256-spp high-quality reference (47.14 dB PSNR, 0.99451 windowed SSIM at 640x360). The render profiler now reports scene build, BVH/session reset, path tracing, OIDN, PNG transform/encoding, and total time. Full-check passed: 14 gallery dimensions, 4 video sequences, 18/18 visual QA, schema, render/import tests, and static linkage.

Sequences currently create a new Cycles session per frame. A transform-update reuse prototype failed image parity (stale transforms; forcing a full reset removed the world background), so it is documented but not shipped. Embree remains disabled because the available aarch64 build is shared and current BVH/session setup is below 1 ms for the profiled scenes.

## lmblender 0.2.2 — Linux arm64

Fixes Cycles camera pixel-aspect handling. Perspective `fov_degrees` is now
explicitly vertical FOV, and orthographic scale is the visible vertical span.
Sphere/cube silhouette regressions cover 16:9, 4:3, 1:1, and 9:16, including
the `render_sequence` path. Camera geometry now remains isotropic within 2%.

## lmblender 0.2.1 — Linux arm64

Static CPU Cycles host with direct C++ scene construction, OIDN denoising, bounded OBJ/STL/PLY/glTF mesh import, material image maps and layered shaders, area-light shapes, spot softness, display transforms, transform motion blur, and automatic imported-model studio lighting/framing.

Verified on aarch64 Linux: `ldd` reports `not a dynamic executable`; `test.sh` passes the complete render, import, schema-signature, image-quality, and Blender comparison suite. The draft archive includes the DSL v5 schema, LLM API guide, animation guide, third-party notices, and license texts.
