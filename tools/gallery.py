# blender -b -P tools/gallery.py -- <character.glb> <pose> <output.png> <armor.glb>...
# Dresses each armor on the base character (head + body) and builds a grid with each one's name.
# The armor pieces replace the body pieces of the same slot (shirt1, pants1...).
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nwchar  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
char_glb, pose, out_png, armors = args[0], args[1], args[2], args[3:]
COLS = 6
# Armor piece suffix -> bare body mesh it covers.
SLOT_OF = {"_chest": "shirt1", "_thighs": "pants1", "_calves": "boots1", "_forearms": "gloves1"}

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 30
arm = nwchar.import_character(char_glb)
nwchar.set_action(arm, bpy.data.actions[pose])
scene.frame_set(8)
base_meshes = {nwchar.base(o.name): o for o in scene.objects if o.type == "MESH"}

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.lens = 60
focus = Vector((0, 0, 0.92))
cam.location = focus + Vector((1.3, -3.6, 0.35))
cam.rotation_euler = (focus - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam
for rot, energy in (((50, 0, 150), 4.0), ((55, 0, -35), 3.0), ((70, 0, -120), 2.5)):
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("w")
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs["Color"].default_value = (0.55, 0.6, 0.7, 1)
bg.inputs["Strength"].default_value = 0.9
scene.world = world
# Dark background in the image, but the bright sky still lights and reflects on the metal.
scene.render.film_transparent = True
scene.render.resolution_x, scene.render.resolution_y = 300, 440

label = bpy.data.objects.new("label", bpy.data.curves.new("label", "FONT"))
label.data.align_x = "CENTER"
label.data.size = 0.11
label.location = (0, -0.6, -0.05)
label.rotation_euler = cam.rotation_euler
mat = bpy.data.materials.new("label")
mat.use_nodes = True
mat.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = (1, 1, 1, 1)
mat.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 1.5
label.data.materials.append(mat)
scene.collection.objects.link(label)

tiles = []
for glb in armors:
    before = set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=glb)
    new = set(scene.objects) - before
    pieces = [o for o in new if o.type == "MESH" and nwchar.base(o.name) != "Icosphere"]
    for o in [o for o in new if o not in pieces]:
        bpy.data.objects.remove(o)
    slots = {nwchar.base(o.name) for o in pieces}
    covered = {SLOT_OF[k] for k in SLOT_OF for sl in slots if sl.lower().endswith(k)}
    for o in pieces:
        for g in o.vertex_groups:
            g.name = nwchar.base(g.name)
        for m in o.modifiers:
            if m.type == "ARMATURE":
                m.object = arm
        o.parent = arm
        o.matrix_parent_inverse.identity()
    for name, o in base_meshes.items():
        o.hide_render = name in slots or name in covered
    name = os.path.basename(glb).replace(".cdf.glb", "").replace("male_", "")
    label.data.body = name
    path = out_png + "_tmp.png"
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    w, h = img.size
    tiles.append(np.array(img.pixels[:]).reshape(h, w, 4))
    bpy.data.images.remove(img)
    for o in pieces:
        bpy.data.objects.remove(o)
    print(f"gallery {name} slots={sorted(slots)}")
    os.remove(path)


while len(tiles) % COLS:
    tiles.append(np.zeros_like(tiles[0]))
rows = [np.concatenate(tiles[i:i + COLS], axis=1) for i in range(0, len(tiles), COLS)]
sheet = np.concatenate(rows[::-1], axis=0)
# Composites over a dark background (the render is transparent).
bgcol = np.array([0.09, 0.09, 0.11, 1.0])
a = sheet[..., 3:4]
sheet = sheet * a + bgcol * (1 - a)
sheet[..., 3] = 1
im = bpy.data.images.new("gallery", sheet.shape[1], sheet.shape[0], alpha=True)
im.pixels = sheet.ravel()
im.filepath_raw = out_png
im.file_format = "PNG"
im.save()
