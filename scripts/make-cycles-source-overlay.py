#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT

"""Create a symlink view of Blender sources with only the unused startup LFS gate satisfied."""
import argparse
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.source.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)

for entry in source.iterdir():
    target = output / entry.name
    if entry.name == "intern":
        # Cycles needs one patched source file. Keep every other intern
        # directory as an upstream symlink and copy only the small Cycles tree
        # into this disposable build overlay.
        if target.is_symlink():
            target.unlink()
        elif target.exists():
            shutil.rmtree(target)
        target.mkdir()
        for intern_entry in entry.iterdir():
            intern_target = target / intern_entry.name
            if intern_entry.name == "cycles":
                shutil.copytree(intern_entry, intern_target, symlinks=True)
            else:
                intern_target.symlink_to(intern_entry, target_is_directory=intern_entry.is_dir())
        patch_file = Path(__file__).resolve().parent.parent / "patches/cycles-bvh2-instance-sharing.patch"
        subprocess.run(["patch", "--batch", "-p1", "-i", str(patch_file)], cwd=output, check=True)
        motion_patch = Path(__file__).resolve().parent.parent / "patches/cycles-motion-vectorizer.patch"
        subprocess.run(["patch", "--batch", "-p1", "-i", str(motion_patch)], cwd=output, check=True)
        transform_header = output / "intern/cycles/util/transform.h"
        transform_text = transform_header.read_text()
        interpolation_start = """ccl_device void transform_motion_array_interpolate(ccl_private Transform *tfm,
                                                   const ccl_global DecomposedTransform *motion,
                                                   const int numsteps,
                                                   const float time)
{
  /* Figure out which steps we need to interpolate. */"""
        interpolation_guard = """ccl_device void transform_motion_array_interpolate(ccl_private Transform *tfm,
                                                   const ccl_global DecomposedTransform *motion,
                                                   const int numsteps,
                                                   const float time)
{
  /* A single sample is a constant transform. The interpolation below needs a
   * pair and otherwise computes motion - 1 as an array index. */
  if (numsteps <= 1) {
    transform_compose(tfm, motion);
    return;
  }
  /* Figure out which steps we need to interpolate. */"""
        if interpolation_start not in transform_text:
            raise RuntimeError("Cycles motion interpolation guard insertion point is missing")
        transform_header.write_text(transform_text.replace(interpolation_start, interpolation_guard, 1))
        cpu_cmake = output / "intern/cycles/kernel/device/cpu/CMakeLists.txt"
        cpu_text = cpu_cmake.read_text()
        cpu_marker = "cycles_set_solution_folder(cycles_kernel_cpu)"
        cpu_workaround = (
            "# Keep this target's inline motion interpolation off the GCC14 tree vectorizer.\n"
            "if(CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\" AND CMAKE_SYSTEM_PROCESSOR MATCHES \"aarch64|arm64\")\n"
            "  target_compile_options(cycles_kernel_cpu PRIVATE -fno-tree-vectorize)\n"
            "endif()\n"
        )
        if cpu_marker not in cpu_text:
            raise RuntimeError("Cycles CPU kernel CMake insertion point is missing")
        if cpu_workaround not in cpu_text:
            cpu_cmake.write_text(cpu_text.replace(cpu_marker, cpu_workaround + cpu_marker, 1))
        continue
    if entry.name != "release":
        if not target.exists() and not target.is_symlink():
            target.symlink_to(entry, target_is_directory=entry.is_dir())
        continue

    target.mkdir(exist_ok=True)
    for release_entry in entry.iterdir():
        release_target = target / release_entry.name
        if release_entry.name != "datafiles":
            if not release_target.exists() and not release_target.is_symlink():
                release_target.symlink_to(release_entry, target_is_directory=release_entry.is_dir())
            continue

        release_target.mkdir(exist_ok=True)
        for data_entry in release_entry.iterdir():
            data_target = release_target / data_entry.name
            if data_entry.name != "startup.blend":
                if not data_target.exists() and not data_target.is_symlink():
                    data_target.symlink_to(data_entry, target_is_directory=data_entry.is_dir())
            elif not data_target.exists():
                # CMake checks only size. Cycles standalone does not load this file.
                data_target.write_bytes(b"Build-only placeholder; Cycles standalone never loads startup.blend.\n" + bytes(2048))

print(output)
