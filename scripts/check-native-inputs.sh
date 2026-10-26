#!/usr/bin/env bash
# Check the concrete inputs required by the full native gate without building.
set -euo pipefail
source_tree=${1:?Blender source tree path required}
cycles_build=${2:?Cycles build path required}
static_prefix=${3:?Static dependency prefix required}
missing=()
[ -f "$source_tree/intern/atomic/atomic_ops.h" ] || missing+=("$source_tree/intern/atomic/atomic_ops.h")
cycles_archive=0
for archive in "$cycles_build/lib/libcycles_session.a" "$cycles_build/lib/cycles_session.lib" "$cycles_build/lib/libcycles_session.lib"; do
  if [ -f "$archive" ]; then cycles_archive=1; break; fi
done
[ "$cycles_archive" = 1 ] || missing+=("$cycles_build/lib/libcycles_session.a (or cycles_session.lib on Windows)")
[ -f "$static_prefix/include/OpenImageIO/paramlist.h" ] || missing+=("$static_prefix/include/OpenImageIO/paramlist.h")
if [ "${#missing[@]}" -ne 0 ]; then
  printf 'missing lmblender native build inputs:\n' >&2
  printf '  %s\n' "${missing[@]}" >&2
  printf 'run scripts/bootstrap-static-deps.sh, scripts/build-deps.sh, scripts/build-oidn-static.sh, and scripts/configure-cycles-static.sh first (or set their documented path overrides).\n' >&2
  exit 2
fi
