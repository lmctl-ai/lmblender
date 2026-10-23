# lmblender tutorial

Goal: go from an empty directory to rendered images of your own 3D scenes with lmblender, one idea per page.

lmblender is a standalone CPU Cycles renderer. You describe a scene in a short lmscript `_dsl_` block; lmscript compiles it to bytecode and runs the lmblender executable, which builds the scene directly in Cycles and writes PNG, EXR, or image sequences. It does not launch Blender and does not use Python.

## Prerequisites

- Node.js 24.15 or newer.
- lmscript, installed with one command (the npm package is named `@lmctl-ai/lmctl`; it provides the `lmscript` command):

```sh
npm install -g @lmctl-ai/lmctl@latest
```

Check it with `lmscript --version`, which prints a line such as `lmscript 0.2.388`.

- A Linux (x64 or arm64), macOS (x64 or arm64), or Windows x64 machine. The lmblender executable itself is downloaded on first use by the `_dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest"` line at the top of every script; you do not install it by hand.

Every page has one script. Save it into an empty directory (or download it with the `curl` line on the page) and run `lmscript FILE.lms`. Each script prints one line when it succeeds and writes its images next to it.

## Pages

Pages 2 to 10 each start from the previous page's script and add one thing, so the scene grows as you go.

1. [First render](01-first-render.md): install check and one sphere.
2. [Place objects](02-place-objects.md): add a cube and a torus and move them.
3. [Camera](03-camera.md): choose the viewpoint.
4. [Materials](04-materials.md): color, metal, roughness.
5. [Lights](05-lights.md): ground, sky, and an area light instead of the studio rig.
6. [Textures](06-textures.md): procedural checker and noise with bump.
7. [Import a mesh](07-import-a-mesh.md): add an OBJ file you wrote.
8. [Animation](08-animation.md): keyframes and an image sequence.
9. [View transforms](09-view-transforms.md): Standard versus AgX for bright light.
10. [When it fails](10-when-it-fails.md): read the error and fix the script.
11. [Compared with Blender](11-compare-with-blender.md): the same scenes rendered by Blender 5.2 from Python, with timings and known gaps.

The scoring of this tutorial against a written checklist is in [CHECKLIST.md](CHECKLIST.md).

## Reference

- [LLM_GUIDE.md](../LLM_GUIDE.md): every DSL method, its units and bounds, and error codes.
- [ANIMATION.md](../ANIMATION.md): sequences and APNG output.
- [PRESETS.md](../PRESETS.md): named looks such as product-studio.
- [examples/](../../examples/): the regression examples the comparison pages use.
- LMScript language: https://lmctl.com/tools/lmscript/skill.html
- Releases: https://github.com/lmctl-ai/lmblender/releases

Every script on these pages was run from an empty directory with lmscript 0.2.388 and the lmblender v0.3.4 release on Linux arm64; the images are the files those runs wrote. The `tutorial` GitHub Actions workflow runs the same scripts on fresh Linux x64, Linux arm64, macOS, and Windows runners (see [CHECKLIST.md](CHECKLIST.md) for the latest results).
