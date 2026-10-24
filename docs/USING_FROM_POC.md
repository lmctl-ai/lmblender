# Use lmblender from another project

Install Node.js and lmscript with `npm install -g @lmctl-ai/lmctl@latest`.
`lmblender` can then be installed by lmscript when it is not already on `PATH`.
Use `@latest` to follow the newest non-draft release; use an exact tag such as
`@v0.3.4` for reproducibility or a fixed offline cache. A matching binary on
`PATH` takes precedence. Run the exact host selected by lmscript with `dsl_cli(p)`:

```lmscript
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let mug = scene.import_stl("assets/mug.stl");
mug.set_material(scene.material(0.68, 0.31, 0.12, 0.0, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0));
scene.studio_setup();
scene.render_png("mug.png", 0, 1280, 720);
end _dsl_

file = "mug.dslb";
savedsl(p, file);
argv = dsl_cli(p);
push(argv, "--dsl");
push(argv, file);
r = system(argv, 3600000);
println(trim(r.stdout));
delete_file(file);
if (r.code != 0) { println(trim(r.stderr)); exit(1); }
```

The install API selects the release binary for the host OS and architecture,
verifies its SHA-256 against the GitHub release digest, and caches it for later
runs. lmblender publishes Linux arm64 and x64, macOS arm64 and x64, and
Windows x64 binaries (since v0.3.4).

For local development, put the built `lmblender` executable first on `PATH`;
the same script then uses that binary and does not download a release. The DSL
schema remains in `schema/lmblender.dsl.json` for host development and direct
compiler use.

On the original POC host, `scripts/release.sh` refreshes the consumer layout
from a verified local build. Preset timing and quality measurements are in
[THROUGHPUT.md](THROUGHPUT.md); they depend on scene complexity and hardware.
