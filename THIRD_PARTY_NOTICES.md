# Third-party notices

## cpp-dsl-sdk

`cpp/dslvm.hpp` is vendored from [lmctl-ai/cpp-dsl-sdk](https://github.com/lmctl-ai/cpp-dsl-sdk), pinned by `sdk.lock` and verified by `scripts/sync-sdk.sh --verify`. The SDK is distributed under the MIT License. The upstream license is reproduced in `licenses/cpp-dsl-sdk-MIT.txt`.

## GNU toolchain and static runtime

Static Linux binaries may include GNU libc, libstdc++, and libgcc objects from the build host. Their licenses and relinking obligations apply to distributed binaries. Build/release packaging will include exact package versions and required notices; no Cycles or image-I/O third-party library is linked by this initial scaffold.
