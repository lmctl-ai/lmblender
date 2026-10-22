# Import and bytecode hostile-input corpus

`tests/fuzz_hostile_inputs.py` mutates bounded fixture corpora for OBJ, MTL,
ASCII PLY, binary STL, glTF JSON, GLB, PNG, JPEG, DSL v5 bytecode, and `.lms`
source. It tests truncation, byte flips, null blocks, large digit runs, MTL
path traversal, and oversize numeric fields. Every host response must be
structured JSON; signals, aborts, sanitizer diagnostics, and hangs fail the
test. `test.sh` runs the corpus on the fully static release build.

To repeat under ASan and UBSan, configure a **separate dynamic sanitizer
build** against the same already-built static Cycles archives and dependency
prefix. Set the four source/build/prefix paths to the values used for the
regular build:

```sh
cmake -S . -B build/asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLMBLENDER_FULL_STATIC=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DBLENDER_SOURCE_TREE="$BLENDER_SOURCE_TREE" \
  -DBLENDER_DEPS_INSTALL="$BLENDER_DEPS_INSTALL" \
  -DBLENDER_LIBDIR="$BLENDER_LIBDIR" \
  -DCYCLES_BUILD_DIR="$CYCLES_BUILD_DIR" \
  -DLMBLENDER_STATIC_PREFIX="$LMBLENDER_STATIC_PREFIX"
cmake --build build/asan --parallel 1
LMBLENDER_FUZZ_BINARY=build/asan/lmblender python3 tests/fuzz_hostile_inputs.py
```

On 2026-10-22, 287 deterministic cases passed under combined ASan/UBSan on
arm64, with no crash or sanitizer report. This instruments the host and its
import/DSL code. The prebuilt Cycles and image dependency archives are not
sanitized. `.lms` source is checked by the external `lmscript` compiler;
the sanitized host receives and fuzzes its compiled bytecode.
