# Corresponding source offer

The `lmblender` executable distributed with a release is the combined GPL-3.0-or-later work described in `LICENSE` and `docs/LICENSE_AUDIT.md`.

For a release tagged `vX.Y.Z`, the corresponding lmblender source revision is the repository tree at that exact tag. The source tree contains the host source, schema, pinned DSL VM, Cycles overlay patches, and build scripts. The exact upstream Blender source used by this build is tag v5.2.2 at commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`; `scripts/fetch-cycles-source.sh` verifies the commit. `sdk.lock` identifies the vendored DSL SDK revision. `BUILD.md` and `docs/RELINKING.md` describe the build inputs and relinking path.

The static third-party dependency sources and source archives are not all vendored in the lmblender repository. Their upstream versions and licenses must be pinned in the release build manifest, and the corresponding sources and applicable static-link relinking materials must be supplied with a public binary distribution. The current host-built release package does not yet contain that complete dependency source bundle. Until the clean-source CI bootstrap and source bundle are verified, treat public binary publication as blocked.
