# Relinking and rebuild notes

This release is a static Linux aarch64 binary built with GNU GCC and glibc. The archive includes the schema, DSL VM source pin, notices, and required license texts, but not the full compiler/dependency source trees or object files. To produce a relinkable build, check out the source and follow `BUILD.md` and `scripts/build-deps.sh`, `scripts/configure-cycles-static.sh`, `scripts/build-oidn-static.sh`, then configure/build lmblender with `LMBLENDER_FULL_STATIC=ON` and `-j2`. The static build depends on the exact Blender source overlay and dependency archives recorded in the scripts and CMake cache options. Do not use the release executable as a substitute for corresponding source and relink materials required by applicable licenses.

Verify a release binary with:

```sh
./lmblender --version
ldd ./lmblender
```

The expected `ldd` result is `not a dynamic executable`. The executable still uses glibc interfaces such as `getpwuid`; static glibc binaries should run on compatible Linux kernels and CPU architectures, and should be rebuilt with the target toolchain for other environments.
