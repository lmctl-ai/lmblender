# OIDN CPU performance audit

The previously recorded 6.31 s OIDN time at 960x540 was not reproducible in a controlled rerun, so it does not indicate a scalar-kernel fallback or a broken filter lifecycle.

## Build and execution findings

- `scripts/build-oidn-static.sh` configures OIDN 2.5 with `OIDN_STATIC_LIB=ON`, `OIDN_DEVICE_CPU=ON`, and CUDA/HIP/SYCL off. The generated ISPC commands use `--arch=aarch64 --target=neon-i32x8 -O3`; the archive is therefore using optimized ARM NEON kernels. `OIDN_FILTER_RT=ON` includes the ray-tracing denoiser.
- The Cycles render log reports `Intel(R) Open Image Denoise 2.5.0`, `ISA: NEON`, and `Tasking: TBB2022.3`. With four render threads, OIDN reports four non-affinitized threads. Running once with `OIDN_NUM_THREADS=4` and once with the variable unset produced 0.9097 s and 0.9089 s respectively, so an explicit OIDN thread override does not improve this host's result.
- At 960x540 with a 16-spp sphere/floor scene, a fresh Cycles process measured 0.91 s denoising. The full render was 4.45 s; 3.38 s was path tracing and 0.14 s was PNG conversion/encoding. The denoiser is not the dominant phase for this more complex scene.
- A separate warmed standalone OIDN RT-filter benchmark (three executions, 960x540, four threads, HDR RGB plus albedo and normal) measured 0.46 s at fast quality, 0.88 s at balanced, and 2.12 s at high. This uses synthetic gradient input and is a microbenchmark, not a render-quality comparison. The previous reported 6.31 s is slower than this high-quality standalone number, but the rerun shows Cycles at 0.91 s for the documented scene.

## Lifecycle and options

Cycles' `OIDNDenoiser` creates a CPU device once per denoiser/session. It reuses its RT filter unless quality or the requested guiding-pass set changes. Per render it binds that frame's color/albedo/normal/output buffers and commits/executes the filter. OIDN's RT filter retains its model weights; it does not upload a new model per frame. The filter commit is still required after rebinding image pointers.

The measured lmblender path uses HDR input, denoising albedo and normal guides, and the fast prefilter (`cleanAux=false`). Balanced and high are supported when a caller chooses image quality over throughput. The standalone benchmark found the expected quality cost, especially for high; it did not test LDR because Cycles' render buffer is scene-linear HDR. OIDN tiling is not an exposed Cycles control here: the device log shows one 960x544 aligned tile at 960x540. These settings were not changed because the measured Cycles run already takes under one second for denoising.

## Reproduction

Build the project as documented in `BUILD.md`, compile a DSL scene with `lmscript`, then run:

```sh
OIDN_VERBOSE=2 LMBLENDER_PROFILE=1 build/lmblender --lmcode build/oidn-profile.dslb
```

Check for the OIDN version, `ISA: NEON`, thread count, Cycles' `Average denoising time`, and the final `LMBLENDER_PROFILE` line. Set `OIDN_NUM_THREADS=4` to compare with an explicit thread count; on this host it gives no measurable gain over OIDN's four-thread default.

## Conclusion

No OIDN rebuild or Cycles source patch is justified by the reproduced measurements: optimized NEON kernels and four-thread TBB tasking are active, device/filter reuse is already present, and the original 6.31 s denoise time was not reproduced. Keep the fast-quality diagram final preset, which is already well below its target; use high quality only where its measured extra cost is acceptable. The historical phase table in `THROUGHPUT.md` is retained as the earlier measurement, with this audit explaining why it should not be treated as a current expected value.
