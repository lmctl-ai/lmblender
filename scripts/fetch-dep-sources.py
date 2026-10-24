#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Download and unpack pinned dependency sources without configuring build_environment.

Versions, file names and hashes come from the pinned Blender checkout's
build_files/build_environment/cmake/versions.cmake. Archives are fetched from
Blender's lib-source mirror (falling back to the upstream URI), verified, and
extracted to DEST/<name>/ with the top-level directory stripped.

usage: fetch-dep-sources.py BLENDER_SOURCE_TREE DEST NAME [NAME...]
  NAME is the versions.cmake prefix, e.g. ZLIB, OPENIMAGEIO, OIDN.
"""
import hashlib
import pathlib
import re
import shutil
import sys
import tarfile
import time
import urllib.request
import zipfile

MIRROR = "https://projects.blender.org/blender/lib-source/media/branch/blender-v5.2-release/"


def parse_versions(path: pathlib.Path) -> dict:
    values = {}
    for m in re.finditer(r'^set\((\w+)\s+(.*?)\)\s*(?:#.*)?$', path.read_text(encoding="utf-8"), re.M):
        values[m.group(1)] = m.group(2).strip().strip('"')

    def expand(text, depth=0):
        if depth > 10:
            return text
        out = re.sub(r"\$\{(\w+)\}", lambda m: values.get(m.group(1), m.group(0)), text)
        return out if out == text else expand(out, depth + 1)

    return {k: expand(v) for k, v in values.items()}


def download(urls, target: pathlib.Path):
    last = None
    for url in urls:
        for attempt in range(3):
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "lmblender-ci/1.0"})
                with urllib.request.urlopen(req, timeout=300) as resp, open(target, "wb") as out:
                    shutil.copyfileobj(resp, out)
                if target.stat().st_size > 0:
                    return url
            except Exception as exc:  # noqa: BLE001 - retried, then reported
                last = exc
                time.sleep(5 * (attempt + 1))
    raise SystemExit(f"download failed for {target.name}: {last}")


def extract(archive: pathlib.Path, dest: pathlib.Path):
    tmp = dest.with_name(dest.name + ".tmp")
    shutil.rmtree(tmp, ignore_errors=True)
    tmp.mkdir(parents=True)
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(tmp)
    else:
        with tarfile.open(archive) as t:
            t.extractall(tmp, filter="tar") if hasattr(tarfile, "data_filter") else t.extractall(tmp)
    entries = list(tmp.iterdir())
    root = entries[0] if len(entries) == 1 and entries[0].is_dir() else tmp
    shutil.rmtree(dest, ignore_errors=True)
    shutil.move(str(root), str(dest))
    shutil.rmtree(tmp, ignore_errors=True)


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    source_tree = pathlib.Path(sys.argv[1])
    dest_root = pathlib.Path(sys.argv[2])
    versions = parse_versions(source_tree / "build_files/build_environment/cmake/versions.cmake")
    downloads = dest_root / "_archives"
    downloads.mkdir(parents=True, exist_ok=True)
    for name in sys.argv[3:]:
        file_name = versions[f"{name}_FILE"]
        expected = versions[f"{name}_HASH"].lower()
        algo = versions.get(f"{name}_HASH_TYPE", "MD5").lower()
        archive = downloads / file_name
        def ok():
            return archive.exists() and hashlib.new(algo, archive.read_bytes()).hexdigest() == expected
        if not ok():
            url = download([MIRROR + file_name, versions.get(f"{name}_URI", "")], archive)
            if not ok():
                raise SystemExit(f"{file_name}: {algo} mismatch from {url}")
        target = dest_root / name.lower()
        stamp = target / ".lmblender-source-hash"
        if stamp.exists() and stamp.read_text(encoding="utf-8") == expected:
            print(f"{name}: up to date ({target})")
            continue
        extract(archive, target)
        stamp.write_text(expected)
        print(f"{name}: {file_name} -> {target}")


if __name__ == "__main__":
    main()
