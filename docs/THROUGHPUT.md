# CPU render throughput

Measured on the shared aarch64 host (4 worker threads, simple UV sphere + ground + area light, Cycles CPU, 8-bit PNG). `scene_setup_ms`, `reset_bvh_ms`, and `render_denoise_output_ms` are emitted to stderr when `LMBLENDER_PROFILE=1`. The final field combines path tracing, OIDN where enabled, and image output; this profiler does not split those phases yet.

| Preset | Size | Samples | Denoise | Seconds/frame | Frames/minute |
|---|---:|---:|---|---:|---:|
| draft | 480x270 | 8 | off | 0.50 | 119.8 |
| draft | 960x540 | 8 | off | 1.90 | 31.5 |
| draft | 1280x720 | 8 | off | 3.36 | 17.9 |
| final | 480x270 | 64 | CPU OIDN | 5.02 | 12.0 |
| final | 960x540 | 64 | CPU OIDN | 19.93 | 3.0 |
| final | 1280x720 | 64 | CPU OIDN | 35.16 | 1.7 |

The simple 960x540 draft scene is below the 3 second/frame target. Final rendering is substantially slower. Timing is scene-dependent; materials, geometry, volume, and denoising change the result. The current profiler groups tracing, denoising, and PNG writing together. This run measured scene setup and session reset below one millisecond for the small fixture. Each sequence frame still creates a new Cycles session; on this fixture session/BVH setup is not the dominant cost, while the render stage scales with samples and pixels.

`scene.set_threads(n)` selects 1–64 CPU threads; the default is four and is limited by available cores. `scene.render_preset("draft")` uses 8 samples and 2 bounces without denoising; `final` uses 64 samples, 8 bounces, and static CPU OIDN. `tests/throughput-bench.lms` reproduces this table with `LMBLENDER_PROFILE=1`.
