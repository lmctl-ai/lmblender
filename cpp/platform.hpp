// SPDX-FileCopyrightText: 2025-2026 lmblender contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Small OS portability helpers so the host builds on Linux, macOS and Windows.
#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#include <process.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace lmblender::platform {

// Keep schema/protocol bytes identical across platforms (no CRT CRLF rewrite).
inline void binary_stdout()
{
#if defined(_WIN32)
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

inline long process_id()
{
#if defined(_WIN32)
  return long(_getpid());
#else
  return long(::getpid());
#endif
}

// Portable replacement for mkdtemp("/tmp/<prefix>-XXXXXX"): a fresh, private
// directory under the OS temp directory.
inline std::filesystem::path make_temp_dir(const std::string &prefix)
{
  std::random_device rd;
  std::mt19937_64 gen((uint64_t(rd()) << 32) ^ rd() ^ uint64_t(process_id()));
  const auto base = std::filesystem::temp_directory_path();
  for (int attempt = 0; attempt < 64; ++attempt) {
    char name[32];
    snprintf(name, sizeof(name), "-%012llx", (unsigned long long)(gen() & 0xffffffffffffULL));
    const auto dir = base / (prefix + name);
    std::error_code ec;
    if (std::filesystem::create_directory(dir, ec)) {
      std::filesystem::permissions(dir, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
      return dir;
    }
  }
  throw std::runtime_error("could not create a temporary directory");
}

}  // namespace lmblender::platform
