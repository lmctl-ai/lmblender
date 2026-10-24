#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Corresponding-source manifest for a release.

Reads the pinned Blender build_environment versions (the single source of truth
for every third-party dependency version and hash), the Blender commit pin in
scripts/fetch-cycles-source.sh, the OIDN/ISPC pins in
scripts/bootstrap-static-deps.sh, sdk.lock, and the vendored nlohmann/json
header, and writes SOURCES.json and SOURCES.md. With --upstream-dir it also
verifies (and with --fetch, downloads) the upstream archives that the release
source asset ships. --check only resolves everything and exits non-zero on a
missing pin, so test.sh can keep this file in step with the build scripts.
"""
import argparse
import hashlib
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIRROR = "https://projects.blender.org/blender/lib-source/media/branch/blender-v{blender_series}-release/{file}"

# (versions.cmake prefix, display name, role in the lmblender binary)
COMPONENTS = [
    ("ZLIB", "zlib", "linked"),
    ("ZSTD", "zstd", "linked"),
    ("PNG", "libpng", "linked"),
    ("JPEG", "libjpeg-turbo", "linked"),
    ("TIFF", "libtiff", "linked"),
    ("OPENJPH", "OpenJPH", "linked"),
    ("PUGIXML", "pugixml", "linked"),
    ("FMT", "fmt", "linked"),
    ("ROBINMAP", "robin-map", "header-only (OpenImageIO)"),
    ("TBB", "oneTBB", "linked"),
    ("IMATH", "Imath", "linked"),
    ("OPENEXR", "OpenEXR", "linked"),
    ("OPENCOLORIO", "OpenColorIO", "linked"),
    ("EXPAT", "expat", "linked"),
    ("PYSTRING", "pystring", "linked"),
    ("YAMLCPP", "yaml-cpp", "linked"),
    ("MINIZIPNG", "minizip-ng", "linked"),
    ("OPENIMAGEIO", "OpenImageIO", "linked"),
    ("EIGEN", "Eigen", "header-only (Cycles build)"),
    ("SSE2NEON", "sse2neon", "header-only (arm64 Cycles build)"),
    ("OPENJPEG", "OpenJPEG", "build-only (OpenImageIO build dependency)"),
    ("BROTLI", "brotli", "build-only (Blender build_environment)"),
    ("EPOXY", "libepoxy", "build-only (Blender build_environment)"),
    ("FREETYPE", "FreeType", "build-only (macOS build_environment)"),
    ("BZIP2", "bzip2", "build-only (macOS build_environment)"),
    ("EMBREE", "Embree", None),  # role decided by configure-cycles-static.sh
]


def fail(message):
    print(f"source-manifest: {message}", file=sys.stderr)
    sys.exit(2)


def read_versions(blender_tree):
    path = blender_tree / "build_files/build_environment/cmake/versions.cmake"
    if not path.is_file():
        fail(f"{path} is missing; run scripts/fetch-cycles-source.sh")
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^\s*set\((\w+)\s+(.*?)\)\s*(#.*)?$", line)
        if match:
            values[match.group(1)] = match.group(2).strip().strip('"')

    def resolve(text, depth=0):
        if depth > 8:
            fail(f"cannot resolve {text}")
        return re.sub(r"\$\{(\w+)\}", lambda m: resolve(values.get(m.group(1), ""), depth + 1), text)

    return {key: resolve(value) for key, value in values.items()}


def blender_series(blender_tree):
    text = (blender_tree / "build_files/build_environment/cmake/download.cmake").read_text(encoding="utf-8")
    match = re.search(r"^\s*set\(BLENDER_VERSION\s+([\d.]+)\)", text, re.M)
    return match.group(1) if match else "main"


def script_pin(script, pattern, what):
    match = re.search(pattern, (ROOT / script).read_text(encoding="utf-8"), re.M)
    if not match:
        fail(f"{what} pin not found in {script}")
    return match.group(1)


def components(blender_tree):
    versions = read_versions(blender_tree)
    series = blender_series(blender_tree)
    configure = (ROOT / "scripts/configure-cycles-static.sh").read_text(encoding="utf-8")
    embree_on = "WITH_CYCLES_EMBREE=ON" in configure
    out = []
    for key, name, role in COMPONENTS:
        if key == "EMBREE":
            role = "linked" if embree_on else "not linked (WITH_CYCLES_EMBREE=OFF)"
        required = [f"{key}_VERSION", f"{key}_URI", f"{key}_HASH", f"{key}_HASH_TYPE", f"{key}_FILE"]
        missing = [field for field in required if field not in versions]
        if missing:
            fail(f"{name}: versions.cmake lacks {', '.join(missing)}")
        file = versions[f"{key}_FILE"]
        out.append({
            "name": name,
            "component": key,
            "version": versions[f"{key}_VERSION"],
            "upstream_uri": versions[f"{key}_URI"],
            "blender_mirror_uri": MIRROR.format(blender_series=series, file=file),
            "file": file,
            "hash_type": versions[f"{key}_HASH_TYPE"].lower(),
            "hash": versions[f"{key}_HASH"],
            "license": versions.get(f"{key}_LICENSE", "see licenses/upstream"),
            "role": role,
            "shipped": None,
        })
        if key == "EMBREE":
            # The static build verifies SHA-256 rather than Blender's older MD5 pin.
            build_script = (ROOT / "scripts/build-deps.sh").read_text(encoding="utf-8")
            expected_archive = f"embree-v{versions['EMBREE_VERSION']}.zip"
            if expected_archive not in build_script:
                fail(f"Embree archive {expected_archive} is not pinned in scripts/build-deps.sh")
            out[-1]["hash_type"] = "sha256"
            out[-1]["hash"] = script_pin(
                "scripts/build-deps.sh",
                r'\[\[ "\$actual_embree_hash" == ([0-9a-f]{64}) \]\]',
                "Embree SHA-256",
            )

    bootstrap = "scripts/bootstrap-static-deps.sh"
    oidn_uri = script_pin(bootstrap, r"^\s*(https://github\.com/RenderKit/oidn/releases/download/v[\d.]+/oidn-[\d.]+\.src\.tar\.gz)", "OIDN URL")
    oidn_sha = script_pin(bootstrap, r"verify_hash sha256 ([0-9a-f]{64}) \"\$oidn_archive\"", "OIDN sha256")
    out.append({
        "name": "OpenImageDenoise",
        "component": "OIDN",
        "version": re.search(r"/v([\d.]+)/", oidn_uri).group(1),
        "upstream_uri": oidn_uri,
        "blender_mirror_uri": None,
        "file": oidn_uri.rsplit("/", 1)[1],
        "hash_type": "sha256",
        "hash": oidn_sha,
        "license": "SPDX:Apache-2.0",
        "role": "linked (CPU denoiser)",
        "shipped": None,
    })
    ispc_version = script_pin(bootstrap, r"ispc/releases/download/v([\d.]+)/", "ISPC version")
    for archive, sha in re.findall(r"ispc_archive=(ispc-v[\d.]+-[\w.]+\.tar\.gz)\s*\n\s*ispc_sha=([0-9a-f]{64})", (ROOT / bootstrap).read_text(encoding="utf-8")):
        out.append({
            "name": "ISPC (build tool, not linked)",
            "component": "ISPC",
            "version": ispc_version,
            "upstream_uri": f"https://github.com/ispc/ispc/releases/download/v{ispc_version}/{archive}",
            "blender_mirror_uri": None,
            "file": archive,
            "hash_type": "sha256",
            "hash": sha,
            "license": "SPDX:BSD-3-Clause",
            "role": "build tool: compiles OIDN .ispc kernels; its own source is https://github.com/ispc/ispc/tree/v" + ispc_version,
            "shipped": "not shipped (compiler binary, see role)",
        })
    return out


def blender_entry(blender_tree):
    commit = script_pin("scripts/fetch-cycles-source.sh", r"^commit=([0-9a-f]{40})", "Blender commit")
    url = script_pin("scripts/fetch-cycles-source.sh", r"^url=(\S+)", "Blender URL")
    version = script_pin("scripts/fetch-cycles-source.sh", r"Blender (v[\d.]+) source pinned", "Blender version")
    return {
        "name": "Blender source subset (Cycles, intern/sky, intern/guardedalloc, intern/libc_compat, intern/atomic, intern/mikktspace, build_files)",
        "component": "BLENDER",
        "version": version,
        "upstream_uri": f"{url.removesuffix('.git')}/tree/{commit}",
        "blender_mirror_uri": None,
        "file": f"blender-{version}-{commit[:12]}-subset.tar.gz",
        "hash_type": None,
        "hash": None,
        "git_commit": commit,
        "license": "SPDX:GPL-2.0-or-later (intern/sky, guardedalloc, libc_compat, build files); SPDX:Apache-2.0 (intern/cycles)",
        "role": "linked (GPL-covered Blender intern archives and the Cycles renderer); the shipped archive is `git archive` of the sparse paths scripts/fetch-cycles-source.sh checks out",
        "shipped": None,
    }, {
        "name": "Cycles source overlay as compiled (patches/ applied by scripts/make-cycles-source-overlay.py)",
        "component": "CYCLES_OVERLAY",
        "version": version,
        "upstream_uri": None,
        "blender_mirror_uri": None,
        "file": f"cycles-overlay-patched-{commit[:12]}.tar.gz",
        "hash_type": None,
        "hash": None,
        "git_commit": commit,
        "license": "SPDX:Apache-2.0 (Cycles) with lmblender MIT patches",
        "role": "linked: Linux/macOS intern/cycles overlay; Windows additionally applies the shipped cycles-msvc-guarded-allocator.patch and standalone build-system overlay",
        "shipped": None,
    }


def standalone_entry():
    script = "scripts/configure-cycles-portable.sh"
    commit = script_pin(script, r"^cycles_commit=([0-9a-f]{40})", "standalone Cycles commit")
    repo = script_pin(script, r"^cycles_repo=(\S+)", "standalone Cycles repository")
    return {
        "name": "Standalone Cycles build system and third_party sources (Windows)",
        "component": "CYCLES_STANDALONE",
        "version": commit,
        "upstream_uri": f"{repo.removesuffix('.git')}/tree/{commit}",
        "blender_mirror_uri": None,
        "file": f"cycles-standalone-{commit[:12]}.tar.gz",
        "git_commit": commit,
        "hash_type": None,
        "hash": None,
        "license": "SPDX:Apache-2.0; third_party notices included in archive",
        "role": "Windows build system and third_party support; configure-cycles-portable.sh overlays Blender Cycles and applies the shipped MSVC patch",
        "shipped": None,
    }


def vendored_entries():
    lock = dict(line.split("=", 1) for line in (ROOT / "sdk.lock").read_text(encoding="utf-8").split() if "=" in line)
    header = (ROOT / "third_party/nlohmann/json.hpp").read_text(errors="replace")
    json_version = re.search(r"version (\d+\.\d+\.\d+)", header).group(1)
    return [
        {
            "name": "lmcode-cpp (dslvm.hpp, vendored in lmblender/cpp)",
            "component": "LMCODE_CPP",
            "version": lock["commit"],
            "upstream_uri": f"{lock['repo']}/tree/{lock['commit']}",
            "blender_mirror_uri": None,
            "file": None,
            "hash_type": "sha256 (of cpp/dslvm.hpp)",
            "hash": lock["sha256"],
            "license": "SPDX:MIT",
            "role": "linked (header-only DSL VM); source is inside lmblender/",
            "shipped": "lmblender/cpp/dslvm.hpp",
        },
        {
            "name": "nlohmann/json (vendored in lmblender/third_party/nlohmann)",
            "component": "NLOHMANN_JSON",
            "version": json_version,
            "upstream_uri": f"https://github.com/nlohmann/json/tree/v{json_version}",
            "blender_mirror_uri": None,
            "file": None,
            "hash_type": None,
            "hash": None,
            "license": "SPDX:MIT",
            "role": "linked (header-only); source is inside lmblender/",
            "shipped": "lmblender/third_party/nlohmann",
        },
    ]


TOOLCHAIN = {
    "compiler": "GCC 14 (Ubuntu 24.04 gcc-14/g++-14 packages); Apple clang on macOS; MSVC 2022 with /MT on Windows",
    "c_library": "glibc from Ubuntu 24.04 libc6-dev, linked statically with -static -static-libstdc++ -static-libgcc; glibc, libstdc++ and libgcc are System Libraries in the GPL-3.0 sense and are not re-shipped here. docs/RELINKING.md explains how to relink.",
    "dsl_compiler": "@lmctl-ai/lmctl (npm), only used by test.sh to compile the example programs; not linked",
    "exact_versions": "The CI build log prints `g++-14 --version`, `cmake --version`, `cl` on Windows, and the ldd/otool/dumpbin output of the released binary.",
}


def file_hash(path, algorithm):
    digest = hashlib.new(algorithm)
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(entry, destination):
    """Download entry["file"], trying each URL until one yields the pinned hash.

    The Blender mirror is preferred (some upstream hosts are unreachable from CI
    runners) but it has answered with rate-limit pages and stale files, so every
    candidate is verified before it is accepted and the next one is tried otherwise.
    """
    candidates = [uri for uri in (entry["blender_mirror_uri"], entry["upstream_uri"]) if uri]
    attempts = []
    for uri in candidates:
        try:
            print(f"fetching {entry['file']} from {uri}")
            request = urllib.request.Request(uri, headers={"User-Agent": "lmblender-release-source/1.0"})
            with urllib.request.urlopen(request, timeout=120) as response, open(destination, "wb") as out:
                while True:
                    chunk = response.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
            actual = file_hash(destination, entry["hash_type"])
            if actual == entry["hash"]:
                return
            attempts.append(f"{uri}: {entry['hash_type']} {actual} != pinned {entry['hash']} ({destination.stat().st_size} bytes)")
        except Exception as error:  # noqa: BLE001 - report every mirror failure
            attempts.append(f"{uri}: {error}")
        destination.unlink(missing_ok=True)
    fail(f"could not download a verified {entry['file']}:\n  " + "\n  ".join(attempts))


def attach_shipped(entries, upstream_dir, do_fetch):
    for entry in entries:
        if not entry.get("file") or entry["component"] == "ISPC" or entry["role"].startswith("not linked"):
            continue
        path = upstream_dir / entry["file"]
        if not path.is_file() and do_fetch and entry.get("hash"):
            fetch(entry, path)
        if not path.is_file():
            if entry["component"] in ("BLENDER", "CYCLES_OVERLAY", "CYCLES_STANDALONE"):
                fail(f"{entry['file']} must be present in {upstream_dir} (scripts/release-source.sh creates it)")
            entry["shipped"] = None
            continue
        if entry.get("hash"):
            actual = file_hash(path, entry["hash_type"])
            if actual != entry["hash"]:
                fail(f"{path.name}: {entry['hash_type']} {actual} does not match the pinned {entry['hash']}")
        else:
            entry["hash_type"] = "sha256"
            entry["hash"] = file_hash(path, "sha256")
        entry["shipped"] = f"upstream/{entry['file']}"
        entry["bytes"] = path.stat().st_size


def markdown(manifest):
    lines = [
        f"# Corresponding source for lmblender {manifest['lmblender_version']}",
        "",
        "Generated by `scripts/source-manifest.py`. `SOURCES.json` is the machine-readable copy.",
        "`lmblender/` is the repository tree at the release commit; `upstream/` holds the pinned",
        "third-party source archives, each verified against the hash below.",
        "",
        "| Component | Version | Role | License | Archive in this asset | Hash | Upstream |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for entry in manifest["components"]:
        shipped = entry.get("shipped") or "manifest only (download from the URLs, verify the hash)"
        digest = f"{entry['hash_type']}:{entry['hash']}" if entry.get("hash") else "n/a"
        upstream = entry.get("upstream_uri") or ""
        mirror = entry.get("blender_mirror_uri")
        if mirror:
            upstream += f" (mirror: {mirror})"
        lines.append(f"| {entry['name']} | {entry['version']} | {entry['role']} | {entry['license']} | {shipped} | {digest} | {upstream} |")
    lines += ["", "## Toolchain", ""]
    for key, value in manifest["toolchain"].items():
        lines.append(f"- **{key}**: {value}")
    lines += ["", f"lmblender git commit: `{manifest['lmblender_commit']}`", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender", type=Path, required=True, help="pinned Blender source tree")
    parser.add_argument("--version", default=(ROOT / "VERSION").read_text(encoding="utf-8").strip())
    parser.add_argument("--commit", default="unknown", help="lmblender git commit recorded in the manifest")
    parser.add_argument("--upstream-dir", type=Path, help="directory holding the shipped upstream archives")
    parser.add_argument("--fetch", action="store_true", help="download missing archives into --upstream-dir")
    parser.add_argument("--json", type=Path, help="write SOURCES.json here")
    parser.add_argument("--markdown", type=Path, help="write SOURCES.md here")
    parser.add_argument("--check", action="store_true", help="only resolve every pin and report")
    args = parser.parse_args()

    blender, overlay = blender_entry(args.blender)
    entries = [blender, overlay, standalone_entry()] + components(args.blender) + vendored_entries()
    if args.upstream_dir:
        attach_shipped(entries, args.upstream_dir, args.fetch)
    manifest = {
        "lmblender_version": args.version.lstrip("v"),
        "lmblender_commit": args.commit,
        "blender_commit": blender["git_commit"],
        "toolchain": TOOLCHAIN,
        "components": entries,
    }
    if args.json:
        args.json.write_text(json.dumps(manifest, indent=2) + "\n")
    if args.markdown:
        args.markdown.write_text(markdown(manifest))
    shipped = sum(1 for entry in entries if (entry.get("shipped") or "").startswith("upstream/"))
    print(f"source manifest: {len(entries)} components resolved, {shipped} upstream archives shipped"
          + (" (check only)" if args.check else ""))


if __name__ == "__main__":
    main()
