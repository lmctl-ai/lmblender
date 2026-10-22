# Gotchas

- GCC 14 -O2 on aarch64 miscompiles Cycles `BVH2::pack_instances` (leaf merge zeroes z/w): shared-geometry
  instances render blank. Fixed by an overlay patch (source-level `make_int4` rewrite); keep it.
- Camera pixel aspect: vertical FOV convention; the 0.2.2 fix removed the 1.78x stretch at 16:9.
- Presets own the camera and lights only until the scene calls a camera/light explicitly.
- OIDN denoise is ~0.9 s at 960x540 on 4 aarch64 cores; the one-off 6.3 s figure was host load.
- Budget errors name the limit (64M total pixels per render_sequence call).
- `*.png` in .gitignore hides fixtures; add explicit exceptions.
- Per-arch SHA256SUMS used to overwrite each other in the release job; one merged SHA256SUMS now covers all assets.
- Never judge CI by workflow-level success when jobs are `continue-on-error`.
