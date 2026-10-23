# lmblender tutorial

Goal: go from an empty directory to rendered images of your own 3D scenes with lmblender, one idea per page.

lmblender is a standalone CPU Cycles renderer. You describe a scene in a short lmscript `_dsl_` block; lmscript compiles it to bytecode and runs the lmblender executable, which builds the scene directly in Cycles and writes PNG, EXR, or image sequences. It does not launch Blender and does not use Python.

## Prerequisites

- Node.js 24.15 or newer.
- lmscript, installed with one command:

```sh
npm install -g @lmctl-ai/lmctl@latest
```

- A Linux (x64 or arm64), macOS (x64 or arm64), or Windows x64 machine. The lmblender executable itself is downloaded on first use by the `_dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest"` line at the top of every script; you do not install it by hand.

Every page has one script. Save it into an empty directory (or download it with the `curl` line on the page) and run `lmscript FILE.lms`. Each script prints one line when it succeeds and writes its images next to it.

## Pages

1. [First render](01-first-render.md): install check and one sphere.
2. [Camera](02-camera.md): place the camera yourself.
3. [Materials and lights](03-materials-and-lights.md): color, metal, ground, light.
4. [Textures](04-textures.md): procedural checker and noise with bump.
5. [Import a mesh](05-import-a-mesh.md): render an OBJ file you wrote.
6. [Animation](06-animation.md): keyframes and an image sequence.
7. [View transforms](07-view-transforms.md): Standard versus AgX for bright light.
8. [When it fails](08-when-it-fails.md): read the error and fix it.
9. [Compared with Blender](09-compare-with-blender.md): the same scenes rendered by Blender 5.2 from Python, with timings and known gaps.

The scoring of this tutorial against a written checklist is in [CHECKLIST.md](CHECKLIST.md).

## Reference

- [LLM_GUIDE.md](../LLM_GUIDE.md): every DSL method, its units and bounds, and error codes.
- [ANIMATION.md](../ANIMATION.md): sequences and APNG output.
- [PRESETS.md](../PRESETS.md): named looks such as product-studio.
- [examples/](../../examples/): the regression examples the comparison pages use.
- LMScript language: https://lmctl.com/tools/lmscript/skill.html
- Releases: https://github.com/lmctl-ai/lmblender/releases

Every script on these pages was run from an empty directory with lmscript 0.2 and the v0.3.4 release on Linux arm64; the images are the files those runs wrote.
