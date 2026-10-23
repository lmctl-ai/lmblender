# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "tutorial", "blender", "py"))
import lmb_bpy

scene = lmb_bpy.Scene()
first = scene.import_gltf("tests/fixtures/bridge/bridge.glb")
scene.expect_int(first.triangle_count(), 12)
scene.world_color(0.24, 0.27, 0.31, 0.42)
scene.area_light(-4.0, -5.0, 8.0, 0.0, 0.0, 1.0, 1.0, 0.87, 0.72, 1050.0, 5.0)
scene.area_light(5.0, -1.0, 5.0, 0.0, 0.0, 1.2, 0.65, 0.79, 1.0, 560.0, 4.0)
scene.camera_look_at(7.35889, -6.92579, 4.95831, 0.0, 0.0, 0.0, 30.2)
scene.render_settings(32, 31, True)
scene.render_png(sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "blender/bridge-reference.png", 32, 640, 480)
lmb_bpy.done()
