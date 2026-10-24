# lmblender: start here

lmblender is a static, Python-free host for lmscript `_dsl_` bytecode that drives Blender's Cycles path tracer (CPU, Embree, OIDN) and writes
PNG/EXR/APNG. Repo: https://github.com/lmctl-ai/lmblender. Combined binary is GPL-3.0-or-later; lmblender-authored files are MIT.

## 60-second takeover

A from-source build is heavy (Blender `build_environment` + Cycles + OIDN, hours; `scripts/build-deps.sh`, `scripts/configure-cycles-static.sh`),
so day to day work runs against the released host: `mkdir /tmp/w && cd /tmp/w && lmscript /path/to/lmblender/examples/cornell-box.lms`
(lmscript fetches `github:lmctl-ai/lmblender@latest` into `~/.cache/lmscript/tools/github/lmctl-ai/lmblender/`). The full `test.sh` and releases
run in CI. Change flow: host/schema/example edit, check the examples with lmscript, read the README gate in `test.sh`.

## Doc map

- `01-purpose-and-place.md`: scope, install story, license.
- `04-gotchas.md`: miscompiles, colour management, camera aspect, SVE/arm64 lessons.
- `05-open-items.md`, `05-platform-status.md` (macOS/Windows build facts and history; the `platforms` jobs are `continue-on-error`).
- `06-examples-and-images.md`: how examples and README images work, the story examples, shared scenes, homepage clips.
- `20-sibling-3d-hosts.md`: pointer to the cross-repo reference in lmcad (host comparison, DSL cheat sheet, porting rules, shared examples).
- `docs/` (not durable-memory): the large reference set (PLATFORMS, RELINKING, ROADMAP, QA reports, tutorial, API/LLM guides). Start with `docs/LLM_GUIDE.md` for the scene API.
- Also see `tool-book-feedback/` for notes from users of the tool book.

## State (2026-10-24)

v0.3.x released on linux/macos/windows; 73 examples; automatic releases on every main push. Open items: 05-open-items.md.
