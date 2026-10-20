# CPU render throughput

Measurements on the shared aarch64 host (Cycles CPU, 4 threads, UV sphere + area light, static OIDN). Results are scene- and host-load-dependent. `LMBLENDER_PROFILE=1` enables the detailed Cycles debug timing and emits one `LMBLENDER_PROFILE` summary line per render to stderr. The output driver measures its own view-transform/PNG encode callback; Cycles logs path tracing and OIDN separately to stdout. Scene build and session reset/BVH are wall-clock brackets around C++ scene construction and `Session::reset` respectively.

## Phase profile, 16 spp + OIDN

Measured with `tests/throughput-bench.lms`-style scene on 2026-10-20. Values are seconds per frame; path tracing and OIDN are Cycles' own logged wall times, and PNG is the output-driver transform/encode callback.

| Resolution | Scene build | BVH/session reset | Path tracing | OIDN | PNG transform + encode | Total |
|---|---:|---:|---:|---:|---:|---:|
| 960x540 | 0.001 | <0.001 | 0.48 | 6.31 | 0.08 | 6.90 |
| 1280x720 | <0.001 | <0.001 | 0.85 | 11.07 | 0.12 | 12.07 |

The component values can differ slightly from total wall time because Cycles phases overlap and each timer has separate boundaries. OIDN consumes most of the time for this small scene. Session reuse or BVH optimization cannot materially improve this case without changing the denoiser or image quality settings.

For comparison, the earlier 64 spp + OIDN end-to-end measurements were 19.93 s at 960x540 and 35.16 s at 1280x720. The prior 8 spp, no-denoise draft values were 1.90 s and 3.36 s respectively. Those older runs were not phase-instrumented and are not directly comparable to the present 16 spp profile.

## Reproducing the phase profile

Build with `cmake --build build --parallel 2`. Compile `tests/throughput-bench.lms` with `lmscript`, then run the resulting bytecode as `LMBLENDER_PROFILE=1 build/lmblender --dsl <bytecode>`. Cycles emits `Average path tracing time` and `Average denoising time` in its debug log; lmblender reports scene build, BVH/session reset, output-driver encode time, and render wall time to stderr.

`scene.set_threads(n)` selects 1–64 CPU threads. Adaptive sampling is controlled by Cycles and currently starts at its automatic minimum; the requested sample count remains the maximum. See the preset profiles in `presets/presets.json` for output size and quality settings.
