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
- ISPC tunes for the build host CPU by default. GitHub's arm64 runners have SVE, so OIDN kernels in the v0.3.3/v0.3.4
  linux-arm64 releases SIGILL on Graviton2/Neoverse-N1 when denoising. build-oidn-static.sh now pins `--cpu`
  on Linux and CI runs scripts/check-no-sve.sh on the arm64 binary.
- The host runs Cycles without an OCIO config: images tagged plain sRGB are NOT linearized. PNG/JPEG color
  textures and environment maps must be tagged `scene_linear_srgb` (Cycles decodes them in the kernel).
- View LUTs: a log1p(v) shaper put the first LUT step at 0.093 linear and crushed dark tones by up to 20/255.
  The shaper is now log1p(64 v); Standard is evaluated analytically. Re-bake with tools/generate_view_luts.py.
- The fallback camera (no camera call) needs the same pixel-aspect correction as explicit cameras.

Compiler pins/minimum are 0.2.420; the complete pinned SDK registers std by default, the embedded renderer schema stays host-only, and API-guide checks filter canonical std signatures from the effective registry.
