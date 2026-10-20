# Roadmap

| Milestone | Status | Evidence / remaining work |
|---|---|---|
| lmscript DSL v5 host and embedded schema | Scaffold complete | `scene.version()` and render examples plus `./test.sh`; schema embedded in the statically linked host. |
| Minimal static CPU Cycles executable | Spike complete | `scripts/configure-cycles-static.sh`; static ELF gate and XML-to-PNG parity smoke test pass. Current 28,145,984-byte executable is host build output under ignored `build/`. |
| Direct Cycles C++ scene bindings | In progress | `scene.render_png(path, samples, width, height)` renders imported meshes with an in-process CPU `Session`. The first slice writes a bounded XML scene and loads it through Cycles' native C++ XML reader; direct node-by-node scene assembly remains. Add materials, explicit camera/light/world/render settings, EXR and passes. |
| OBJ/STL mesh import as first binding | Usable vertical slice | `scene.import_obj` and `scene.import_stl` support triangulated OBJ and ASCII/binary STL. OBJ handles can be passed to `scene.render_png`; tests check mesh counts and decoded image variation. |
| Golden DSL rendering examples | In progress | `examples/render-mesh.lms` renders a tetra mesh; current test checks PNG dimensions and pixel variation. Add reference comparisons and more complex scenes. |
