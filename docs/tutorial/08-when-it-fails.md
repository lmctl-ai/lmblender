# 8. When it fails

Goal: read an lmblender error and fix the script.

This script has a deliberate mistake: a sphere of radius 0.

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/08-when-it-fails.lms
```

<!-- code: code/08-when-it-fails.lms -->
```lmscript
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(0.0, 48, 24, true);
scene.studio_setup();
scene.render_png("when-it-fails.png", 16, 640, 480);
end _dsl_
file = "when-it-fails.dslb";
savedsl(p, file);
r = system ${dsl_cli(p)} --dsl ${file};
delete_file(file);
if (r.code != 0) { println(trim(r.stdout)); if (trim(r.stderr) != "") { println(trim(r.stderr)); } exit(1); }
println("wrote when-it-fails.png");
```

```sh
lmscript 08-when-it-fails.lms
```

Expected output (the script exits with status 1 and writes no image):

```text
{"ok": false, "phase": "run", "code": "RUN_HOST", "message": "dsl line 2:18: sphere radius must be in (0,10000]", "line": 2, "column": 18}
```

The host reports every failure as one JSON line on standard output:

- `code` names the kind of problem.
- `message` names the call and its accepted range.
- `line` and `column` point into the script. Line 2 here is the `uv_sphere` call.

Fix: change `0.0` to a positive radius such as `1.0` and run the script again. It then prints:

```text
wrote when-it-fails.png
```

![The fixed script renders the sphere from page 1](images/when-it-fails.png)

This is also why every script on these pages prints `r.stdout` on failure: the JSON error is on standard output, and standard error is usually empty. The scripts in the repository's `examples/` directory print only standard error, so on the same failure they show `lmblender exit 3:` with nothing after it.

## Common failures

| What you see | Cause | Fix |
|---|---|---|
| `RUN_HOST` with a range in the message | A value is outside its allowed range | Use a value inside the range in the message |
| `LOAD_SCHEMA_MISMATCH` or `LOAD_BAD_BYTECODE`, exit 3 | The bytecode was compiled for a different lmblender version | Run the `.lms` again so lmscript recompiles it; do not reuse old `.dslb` files |
| `OUTPUT_PATH` | The output directory does not exist, or is a symlink | Create the directory first, for example `mkdir("renders");` in lmscript before the block |
| `OBJ_INVALID`, `GLTF_INVALID`, `IMAGE_INVALID` | The input file is broken, or a path is wrong | Check that the path is relative to where you run lmscript, and open the file in another tool |
| `RENDER_TIMEOUT` | One render ran longer than `render_timeout(seconds)` | Lower the samples or resolution, or raise the timeout |
| `GEOMETRY_BUDGET` | More than 10 million vertices or 20 million triangles | Use `instance_mesh` for copies, or fewer segments |
| `Illegal instruction` (exit 132), or `r.code` is null, on Linux arm64 when denoising is on | Known issue in the v0.3.3 and v0.3.4 linux-arm64 releases: their OIDN denoiser uses SVE instructions, which CPUs such as AWS Graviton2 (Neoverse-N1) do not have | Leave denoising off (the default) or build lmblender from source on that machine ([BUILD.md](../../BUILD.md)); renders without the denoiser are not affected |

The full list of error codes is in [LLM_GUIDE.md](../LLM_GUIDE.md#errors-and-recovery).

Next: [9. Compared with Blender](09-compare-with-blender.md).
