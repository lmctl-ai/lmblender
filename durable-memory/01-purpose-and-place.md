# Purpose and place

lmblender is a static, Python-free, GUI-free, OpenGL-free host for lmscript `_dsl_` bytecode (cpp-dsl-sdk
dslvm v5). It drives the Cycles C++ API directly (CPU, Embree, OIDN denoise) and writes PNG/EXR/APNG.
It is the Cycles-render sibling of lmcad (CAD + raster); the two stay separate binaries, usable alone or
combined (lmcad exports a model, lmblender renders it). The private proving ground (.blend editing, baking,
sculpt-like ops) is lmctlhq/blendercli; its `export_lmblender` op bridges to this host through GLB.

Install story: `_dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest"`. Release assets are bare
executables named `lmblender-OS-ARCH[.exe]` (OS linux/macos/windows, ARCH x64/arm64). `--version` first line
must contain exactly one MAJOR.MINOR.PATCH token equal to the release tag. VERSION is the single source; CI stamps `MAJOR.MINOR.<run number>` into it for every automatic release (the committed file keeps the MAJOR.MINOR base).
License: combined binary GPL-3.0-or-later (Blender/Cycles-derived); lmblender-authored files MIT (SPDX headers).
