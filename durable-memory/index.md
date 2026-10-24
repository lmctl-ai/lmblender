# lmblender: start here

lmblender is a static, Python-free host for lmscript `_dsl_` bytecode that drives Blender's Cycles path tracer (CPU, Embree, OIDN) and writes PNG/EXR/APNG.
Repository: https://github.com/lmctl-ai/lmblender. Combined binary is GPL-3.0-or-later; lmblender-authored files are MIT.

## 60-second takeover

A from-source build is heavy (Blender `build_environment` + Cycles + OIDN, hours; `scripts/build-deps.sh`, `scripts/configure-cycles-static.sh`), so day-to-day work runs against the released host:
```sh
mkdir -p /tmp/w && cd /tmp/w
lmscript /path/to/lmblender/examples/cornell-box.lms
```
(lmscript fetches `github:lmctl-ai/lmblender@latest` into `~/.cache/lmscript/tools/github/lmctl-ai/lmblender/`).

## Document map

| File | Contents |
| --- | --- |
| [00-index.md](00-index.md) | Original index and state summary |
| [01-purpose-and-place.md](01-purpose-and-place.md) | Scope, installation story, licensing split |
| [04-gotchas.md](04-gotchas.md) | Miscompiles, colour management, camera aspect, SVE/arm64 lessons |
| [05-open-items.md](05-open-items.md) | Current backlog and planned features |
| [05-platform-status.md](05-platform-status.md) | macOS/Windows build facts, verification runs, and toolchain details |
| [06-examples-and-images.md](06-examples-and-images.md) | How examples and README images work, shared scenes, homepage clips |
| [20-sibling-3d-hosts.md](20-sibling-3d-hosts.md) | Comparison across 3D sibling hosts (lmcad, lmmagnum, lmfilament) |

## Adding an example or function

### Adding an example
1. Create `examples/<name>.lms` using `_dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest"`.
2. Generate its rendered output: `lmscript examples/<name>.lms` (writes `images/<name>.png`).
3. Add the example to `test.sh` test list.
4. Add the image to `README.md` gallery with `<img src="images/<name>.png" ...>` and `lmctl autorun <raw .lms URL>` caption.

### Adding a host function/method
1. Declare the method/function in the DSL schema (`schema/lmblender.dsl.json`).
2. Implement and register the method in `src/registry.cpp` using the typed `dslvm::Registry` API.
3. Verify that `./test.sh` passes and signatures match `lmscript --dsl-signatures`.

## Releases
Releases are built and published automatically on every push to `main` when CI workflows succeed.
Platform binaries: `lmblender-linux-x64`, `lmblender-linux-arm64`, `lmblender-macos-arm64`, `lmblender-macos-x64`, `lmblender-windows-x64.exe`.
