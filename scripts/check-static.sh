#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
#
# Assert that each executable only depends on operating-system libraries.
#   Linux:   fully static ("not a dynamic executable").
#   macOS:   only libSystem, libc++, libobjc and /System/Library frameworks.
#   Windows: only OS DLLs; no MSVC/MinGW runtime or third-party DLLs.
set -euo pipefail
status=0
for bin in "$@"; do
  case "$(uname -s)" in
    Linux)
      out=$(ldd "$bin" 2>&1 || true)
      printf '%s: %s\n' "$bin" "$out"
      grep -q 'not a dynamic executable' <<<"$out" || { echo "error: $bin is not fully static" >&2; status=1; }
      ;;
    Darwin)
      deps=$(otool -L "$bin" | tail -n +2 | awk '{print $1}')
      printf '%s links:\n%s\n' "$bin" "$deps"
      bad=$(grep -vE '^(/usr/lib/libSystem\.B\.dylib|/usr/lib/libc\+\+\.1\.dylib|/usr/lib/libobjc\.A\.dylib|/System/Library/Frameworks/[^ ]+)$' <<<"$deps" || true)
      [[ -z "$bad" ]] || { echo "error: $bin links non-system libraries:" >&2; echo "$bad" >&2; status=1; }
      ;;
    MINGW*|MSYS*|CYGWIN*)
      deps=$(objdump -p "$bin" | awk '/DLL Name:/ {print toupper($3)}')
      printf '%s imports:\n%s\n' "$bin" "$deps"
      bad=$(grep -vE '^(KERNEL32|KERNELBASE|ADVAPI32|USER32|GDI32|SHELL32|OLE32|OLEAUT32|WS2_32|BCRYPT|CRYPT32|SHLWAPI|VERSION|DBGHELP|PSAPI|WINMM|NTDLL|RPCRT4|SECUR32|IPHLPAPI|API-MS-WIN-[A-Z0-9-]+)\.DLL$' <<<"$deps" || true)
      [[ -z "$bad" ]] || { echo "error: $bin imports non-OS DLLs:" >&2; echo "$bad" >&2; status=1; }
      ;;
    *) echo "error: unsupported OS $(uname -s)" >&2; status=1 ;;
  esac
done
exit $status
