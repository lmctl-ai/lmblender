# lmblender

`lmblender` is a standalone CPU Cycles renderer controlled by `lmscript` DSL bytecode. It is a separate executable from [lmcad](https://github.com/lmctlhq/lmcad); composition happens through files and lmscript programs. The initial scaffold embeds DSL schema bytes and hosts the pinned v5 VM. Cycles API bindings and mesh import arrive in the next milestone.

## Build and test

```sh
./scripts/sync-sdk.sh --verify
./test.sh
ldd build/lmblender  # expected: not a dynamic executable
```

Requires CMake, Ninja, a C++17 compiler, Python 3 for test assertions, and `lmscript` from `@lmctl-ai/lmctl` for compiling examples.

## Current surface

The initial host only exposes `scene.version()` to validate the embedded schema, VM, static-link setup, and bytecode v5 path. It does not render yet. The target renderer is standalone CPU Cycles; it will not load Blender UI, Python, or GL.
