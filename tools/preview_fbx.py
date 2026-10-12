# blender -b -P tools/preview_fbx.py -- extract/ue/<conjunto>/manifest.json <pasta_saida> <A_nome>...
# Confere os FBX exportados: SK + armas nos sockets + cada animação, folha de 5 quadros por animação.
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
manifest = json.load(open(args[0]))
out_dir, wanted = args[1], args[2:]
os.makedirs(out_dir, exist_ok=True)
fbx = manifest["fbx"]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = manifest["fps"]
bpy.ops.import_scene.fbx(filepath=fbx["skeletal_mesh"])
arm = next(o for o in scene.objects if o.type == "ARMATURE")

for key, path in fbx["weapons"].items():
    before = set(scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    bone = manifest["weapons"][key]["socket"]
    for o in set(scene.objects) - before:
        if o.parent is None:
            o.parent, o.parent_type, o.parent_bone = arm, "BONE", bone
            o.matrix_parent_inverse.identity()
            o.location = (0, -arm.data.bones[bone].length, 0)

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
for rot, energy in (((50, 0, 150), 5.0), ((60, 0, -30), 3.0), ((70, 0, -120), 2.0)):
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("w")
world.color = (0.08, 0.08, 0.1)
scene.world = world
scene.render.resolution_x, scene.render.resolution_y = 300, 400

anim_by_asset = {c["asset"]: c for c in manifest["composites"].values()}
for asset in wanted:
    before = set(scene.objects)
    bpy.ops.import_scene.fbx(filepath=anim_by_asset[asset]["fbx"])
    src = next(o for o in set(scene.objects) - before if o.type == "ARMATURE")
    src.hide_render = True
    act = src.animation_data.action
    # O FBX só de animação volta com outro repouso (sem bind pose); copiar a pose em espaço
    # de mundo, osso a osso, mostra o que os valores absolutos do arquivo descrevem.
    for pb in arm.pose.bones:
        for c in list(pb.constraints):
            pb.constraints.remove(c)
        if pb.name in src.pose.bones:
            c = pb.constraints.new("COPY_TRANSFORMS")
            c.target, c.subtarget = src, pb.name
    start, end = act.frame_range
    tiles = []
    for i in range(5):
        scene.frame_set(int(round(start + (end - start) * i / 4)))
        hips = arm.matrix_world @ arm.pose.bones["Hips"].head
        focus = Vector((hips.x, hips.y, hips.z))
        cam.location = focus + Vector((2.0, -3.6, 0.5))
        cam.rotation_euler = (focus - cam.location).to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(out_dir, "_tmp.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        w, h = img.size
        tiles.append(np.array(img.pixels[:]).reshape(h, w, 4))
        bpy.data.images.remove(img)
    os.remove(path)
    bpy.data.objects.remove(src)
    sheet = np.concatenate(tiles, axis=1)
    im = bpy.data.images.new(asset, sheet.shape[1], sheet.shape[0], alpha=True)
    im.pixels = sheet.ravel()
    im.filepath_raw = os.path.join(out_dir, f"{asset}.png")
    im.file_format = "PNG"
    im.save()
    print(f"preview {asset}")
