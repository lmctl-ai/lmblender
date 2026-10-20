## lmblender 0.2.2 — Linux arm64

Fixes Cycles camera pixel-aspect handling. Perspective `fov_degrees` is now
explicitly vertical FOV, and orthographic scale is the visible vertical span.
Sphere/cube silhouette regressions cover 16:9, 4:3, 1:1, and 9:16, including
the `render_sequence` path. Camera geometry now remains isotropic within 2%.

## lmblender 0.2.1 — Linux arm64

Static CPU Cycles host with direct C++ scene construction, OIDN denoising, bounded OBJ/STL/PLY/glTF mesh import, material image maps and layered shaders, area-light shapes, spot softness, display transforms, transform motion blur, and automatic imported-model studio lighting/framing.

Verified on aarch64 Linux: `ldd` reports `not a dynamic executable`; `test.sh` passes the complete render, import, schema-signature, image-quality, and Blender comparison suite. The draft archive includes the DSL v5 schema, LLM API guide, animation guide, third-party notices, and license texts.
