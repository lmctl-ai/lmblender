"""Create a uniform emission-panel .blend and render Blender OCIO goldens.

Run only in the test/reference path with Blender's bundled Python. The lmblender
binary itself does not invoke Python or use OCIO at runtime.
"""
import bpy
import math
import os
import sys

base = os.path.abspath(sys.argv[-1])
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new("emission panel")
mesh.from_pydata([(-10,-10,0),(10,-10,0),(10,10,0),(-10,10,0)], [], [(0,1,2,3)])
obj = bpy.data.objects.new("emission panel", mesh)
bpy.context.collection.objects.link(obj)
mat = bpy.data.materials.new("18 percent linear emission")
mat.use_nodes = True
principled = mat.node_tree.nodes.get("Principled BSDF")
principled.inputs["Base Color"].default_value = (0,0,0,1)
principled.inputs["Emission Color"].default_value = (0.18,0.18,0.18,1)
principled.inputs["Emission Strength"].default_value = 1.0
obj.data.materials.append(mat)
camera_data = bpy.data.cameras.new("camera")
camera = bpy.data.objects.new("camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location=(0,0,4)
camera.data.type='ORTHO'
camera.data.ortho_scale=4
bpy.context.scene.camera=camera
scene=bpy.context.scene
scene.render.engine='CYCLES'
scene.cycles.samples=8
scene.render.resolution_x=64
scene.render.resolution_y=64
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.world.color=(0,0,0)
if scene.world.use_nodes:
    bg=scene.world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value=(0,0,0,1)
    bg.inputs["Strength"].default_value=0
for view in ('Standard','Filmic','AgX'):
    scene.view_settings.look='None'
    scene.view_settings.view_transform=view
    scene.render.filepath=os.path.join(base, f"blender-{view.lower()}.png")
    bpy.ops.render.render(write_still=True)
