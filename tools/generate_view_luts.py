#!/usr/bin/env python3
"""Bake Blender's OCIO display views into a static C++ RGB LUT header.

Usage: python3 tools/generate_view_luts.py <config.ocio> [output-header]
Run with Blender's bundled Python so its PyOpenColorIO module can load.
"""
import math
import sys
from pathlib import Path

import PyOpenColorIO as ocio

if len(sys.argv) < 2:
    raise SystemExit("usage: generate_view_luts.py <config.ocio> [output-header]")
config_path = Path(sys.argv[1])
output_path = Path(sys.argv[2] if len(sys.argv) > 2 else "cpp/view_luts.h")
config = ocio.Config.CreateFromFile(str(config_path))
size = 33
views = ("Standard", "Filmic", "AgX")
rows = []
for view in views:
    display_view = ocio.DisplayViewTransform()
    display_view.setSrc("Linear Rec.709")
    display_view.setDisplay("sRGB")
    display_view.setView(view)
    processor = config.getProcessor(display_view).getDefaultCPUProcessor()
    values = []
    for ri in range(size):
        for gi in range(size):
            for bi in range(size):
                rgb = []
                for index in (ri, gi, bi):
                    scene_linear = math.expm1(index * math.log1p(16.0) / (size - 1))
                    rgb.append(scene_linear)
                display_rgb = processor.applyRGB(rgb)
                values.extend(max(0.0, min(1.0, float(channel))) for channel in display_rgb)
    rows.append((view, values))

output_path.parent.mkdir(parents=True, exist_ok=True)
with output_path.open("w") as out:
    out.write("// Generated from Blender 5.2 config.ocio; input is scene-linear Rec.709, output sRGB display.\n")
    out.write("#pragma once\n#include <array>\n\nnamespace view_luts {\n")
    out.write(f"inline constexpr int kSize = {size};\n")
    out.write("inline constexpr float kLogRange = 2.833213344056216f; // log(17)\n")
    for view, values in rows:
        name = {"Standard": "kStandard", "Filmic": "kFilmic", "AgX": "kAgX"}[view]
        out.write(f"inline constexpr std::array<float, {len(values)}> {name} = {{\n")
        for begin in range(0, len(values), 12):
            out.write("  " + ", ".join(f"{v:.9g}f" if "." in f"{v:.9g}" or "e" in f"{v:.9g}" else f"{v:.9g}.0f" for v in values[begin:begin+12]) + ",\n")
        out.write("};\n")
    out.write("} // namespace view_luts\n")
print(f"wrote {output_path} ({len(views)} views, {size}^3 samples each)")
