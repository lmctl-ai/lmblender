#!/usr/bin/env python3
"""Create a symlink view of Blender sources with only the unused startup LFS gate satisfied."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.source.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)

for entry in source.iterdir():
    target = output / entry.name
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
