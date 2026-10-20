# Roadmap

| Milestone | Status | Evidence / remaining work |
|---|---|---|
| lmscript DSL v5 host and embedded schema | Scaffold complete | `scene.version()` example and `./test.sh`; no renderer binding yet. |
| Minimal static CPU Cycles executable | Spike complete | `scripts/configure-cycles-static.sh`; static ELF gate and XML-to-PNG parity smoke test pass. Current 28,145,984-byte executable is host build output under ignored `build/`. |
| Direct Cycles C++ scene bindings | Not started | Add primitives/meshes, materials, lights, camera, world, render settings, PNG/EXR and passes. |
| OBJ/STL mesh import as first binding | Not started | Required for lmcad-to-lmblender composition. |
| Golden DSL rendering examples | Not started | Add after the direct Cycles API surface exists. |
