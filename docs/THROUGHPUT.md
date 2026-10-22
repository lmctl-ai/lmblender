# CPU render throughput

Measurements on the shared aarch64 host (Cycles CPU, 4 threads, UV sphere + area light, static OIDN). Results are scene- and host-load-dependent. `LMBLENDER_PROFILE=1` enables the detailed Cycles debug timing and emits one `LMBLENDER_PROFILE` summary line per render to stderr. The output driver measures its own view-transform/PNG encode callback; Cycles logs path tracing and OIDN separately to stdout. Scene build and session reset/BVH are wall-clock brackets around C++ scene construction and `Session::reset` respectively.

The 6.31 s OIDN entry below is a historical measurement and is not a stable baseline: a controlled rerun of the same resolution with the current binary reported 0.91 s for OIDN. The CPU device reports ARM NEON, four TBB threads, and OIDN 2.5; an unset `OIDN_NUM_THREADS` and an explicit value of four measured 0.9089 s and 0.9097 s. See [OIDN audit](OIDN_AUDIT.md) for the build flags, filter lifecycle, standalone quality timings, and reproduction command.

## Phase profile, 16 spp + OIDN

Measured with `tests/throughput-bench.lms`-style scene on 2026-10-20. Values are seconds per frame; path tracing and OIDN are Cycles' own logged wall times, and PNG is the output-driver transform/encode callback.

| Resolution | Scene build | BVH/session reset | Path tracing | OIDN | PNG transform + encode | Total |
|---|---:|---:|---:|---:|---:|---:|
| 960x540 | 0.001 | <0.001 | 0.48 | 6.31 | 0.08 | 6.90 |
| 1280x720 | <0.001 | <0.001 | 0.85 | 11.07 | 0.12 | 12.07 |

The component values can differ slightly from total wall time because Cycles phases overlap and each timer has separate boundaries. OIDN consumes most of the time for this small scene. Session reuse or BVH optimization cannot materially improve this case without changing the denoiser or image quality settings.

## Sequence session experiment

`render_sequence` was profiled over three animated 480x270 frames (8 spp + OIDN): scene construction/session setup was 1 ms on frame 1 and below 1 ms on frames 2–3; `Session::reset`/BVH was below 1 ms on all three. Render plus denoise was 1.76, 1.75, and 1.70 seconds. A prototype that retained one C++ `Session` and updated object transforms failed its real-image parity check (the transform stayed stale); forcing `Scene::reset()` then removed the world background. That prototype was discarded. The sequence currently creates a fresh Cycles session per frame. Session reuse remains unimplemented; a safe solution needs Cycles scene graph updates that preserve world/material state. Measured setup/BVH are below timer resolution, so this is not the current throughput bottleneck. The successful optimization was fast OIDN quality, measured against a high-sample reference below.

For comparison, the earlier 64 spp + OIDN end-to-end measurements were 19.93 s at 960x540 and 35.16 s at 1280x720. The prior 8 spp, no-denoise draft values were 1.90 s and 3.36 s respectively. Those older runs were not phase-instrumented and are not directly comparable to the present 16 spp profile.

## Reproducing the phase profile

Build with `cmake --build build --parallel 2`. Compile `tests/throughput-bench.lms` with `lmscript`, then run the resulting bytecode as `LMBLENDER_PROFILE=1 build/lmblender --dsl <bytecode>`. Cycles emits `Average path tracing time` and `Average denoising time` in its debug log; lmblender reports scene build, BVH/session reset, output-driver encode time, and render wall time to stderr.

`scene.set_threads(n)` selects 1–64 CPU threads. Adaptive sampling is controlled by Cycles and currently starts at its automatic minimum; the requested sample count remains the maximum. See the preset profiles in `presets/presets.json` for output size and quality settings.

## Speed and image-quality tradeoff

Cycles exposes OIDN `high`, `balanced`, and `fast` quality choices through `scene.denoise_quality(name)` and preset `render.denoise_quality`. The diagram final preset uses `fast`; auxiliary albedo/normal guides remain enabled, with Cycles' fast prefilter. A repeatable test compares a 16-spp fast render to a 256-spp high-quality OIDN reference of a glossy sphere and lit floor at 640x360. The measured result was **47.14 dB PSNR, 0.99451 windowed SSIM** (8x8 windows, stride 4). The test gates at 30 dB and 0.95 SSIM.

Measured preset timings on this host (4 threads):

| Preset | Output | Samples | Denoise | Time/frame |
|---|---:|---:|---|---:|
| diagram-dark-16x9-draft | 960x540 | 8 | off | 0.41 s |
| diagram-dark-16x9-final | 1280x720 | 16 | OIDN fast | 2.00 s |

The final timing used a simple emissive diagram sphere and preset lighting, not a complex glossy/volume scene. For the glossy sphere+floor scene at 640x360, fast OIDN took 1.95 s at 16 spp; the 256-spp high-quality reference took 19.03 s. Use `scene.denoise_quality("balanced")` or `"high"` when image quality is more important than turnaround. Results vary significantly with scene complexity and host load.

### No-denoise sample-count comparison

On the same 640x360 glossy gold sphere/floor scene, fixed-seed no-denoise renders took 6.80 s at 64 spp and 11.14 s at 128 spp. Against the 256-spp high-OIDN reference, the measured 64-spp image was 37.75 dB PSNR / 0.93456 SSIM; 128 spp was 39.34 dB / 0.94856 SSIM. Both miss the existing 0.95 SSIM gate, and both are slower than 16 spp + fast OIDN (2.00 s for the same scene in the existing quality run). Adaptive sampling was enabled, but this glossy scene did not converge early enough to make the higher-sample no-denoise options competitive. The 8-spp no-denoise draft remains appropriate for previews; it is not the final preset.

The diagram-dark final preset already renders 1280x720 in about 2.00 s on the simple emissive benchmark, below the 8 s target. Keep its 16 spp + fast OIDN settings. A more complex scene will cost more; the target is not a guarantee for glossy, volumetric, or texture-heavy content.

Static Embree 4.4.1 is enabled for Cycles CPU BVH construction and ray traversal. The dependency build uses TBB, NEON on arm64 or SSE4.2/AVX2 dispatch on x64, and disables ISPC, SYCL and tutorials. The resulting arm64 executable is 90,415,296 bytes versus 81,233,664 bytes without Embree (+9,181,632 bytes); `ldd` still reports `not a dynamic executable`. The Cycles log confirms `Using EMBREE layout`, including on linked mesh instances. With a fixed seed, a repeated instance render was byte identical, and the dense mesh image was 79.71 dB PSNR against the BVH2 build (the test gate is 35 dB).

### Embree versus BVH2 on arm64

Sequential wall-clock comparisons on this shared four-core host, with the same static dependency stack and fixed-seed scene; lower is faster. Other active jobs caused load variation, so these are directional measurements rather than a stable capacity claim.

| Scene | Embree | BVH2 | Image PSNR |
|---|---:|---:|---:|
| Four dense spheres, about 520k triangles, 640x480, 16 spp | 12.95 s | 14.25 s | 79.61 dB |
| Glass sphere, 128 spp | 128.59 s | 135.40 s | 77.89 dB |
| Hair/strand scene, 64 spp | 52.60 s | 51.88 s | 77.75 dB |

On the standard `tests/throughput-bench.lms` sequence (4 threads, sequential runs, same shared host), draft 960x540 8 spp took 10.66 s with Embree versus 11.14 s with BVH2; draft 1280x720 8 spp took 19.54 versus 19.43 s. Final 960x540 64 spp took 79.39 versus 94.56 s, and final 1280x720 64 spp took 133.05 versus 149.28 s. The absolute times are much higher than the earlier unloaded-host numbers above because another build ran concurrently on this shared machine; these runs do not redefine preset throughput targets.

Embree improves the dense and glass scenes by about 9% and 5%; the hair difference is within shared-host noise. The earlier small-scene phase profile above predates this build and remains useful for identifying the OIDN bottleneck, but is not an Embree comparison. Adaptive sampling is already on in Cycles, though low-sample renders naturally reach their requested maximum. Clamp and bounce choices remain scene-dependent; the diagram preset uses four bounces and no clamp to preserve light/shadow contrast. Async denoising overlap and tile tuning were not enabled: the direct Cycles CPU session uses its own scheduler and the measured OIDN phase is already the dominant serialized tail.

The x64 CI runner builds and tests the static Embree path, including fixed-seed image parity (79.63 dB PSNR against BVH2 in run 39791643545). Its logs contain no timed Embree-versus-BVH2 run, so an x64 speedup has not yet been measured.
