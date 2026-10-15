# blender -b -P tools/assemble_character.py -- <character.glb> <weapon.glb> <shield.glb> <out.blend> <preview_dir>
#
# Assembles the character for checking and export:
#  1. a single copy of the skeleton (the glb brings one copy per mesh, with suffix .001, .002...);
#  2. layered actions like the game's Mannequin: unarmed base + weapon layer on top;
#  3. sword on right_hand_attach and shield on left_hand_attach;
#  4. contact sheet per action in <preview_dir>.
import sys
import os
import re
import math
import bpy
import numpy as np
from mathutils import Vector
from bpy_extras import anim_utils

args = sys.argv[sys.argv.index("--") + 1:]
char_glb, sword_glb, shield_glb, out_blend, preview_dir = args
os.makedirs(preview_dir, exist_ok=True)

FPS = 30
# Final action -> layers, from the base up (same composition as player_1h_melee.adb).
LAYERED = {
    "sword_idle": ["unarmed_idle", "rswrd_idle"],
    "sword_run": ["unarmed_run", "rswrd_run"],
    "sword_sprint": ["unarmed_sprint", "rswrd_sprint"],
}
SINGLE = ["rswrd_1h_lt_atk_01", "rswrd_1h_lt_atk_02", "rswrd_1h_lt_atk_03", "rswrd_1h_hvy_atk_01"]
DROP_MESHES = {"xray", "Icosphere", "shirt2"}
SUFFIX = re.compile(r"\.\d{3}$")


def base(name):
    return SUFFIX.sub("", name)


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = FPS
bpy.ops.import_scene.gltf(filepath=char_glb)
arm = next(o for o in scene.objects if o.type == "ARMATURE")
arm.name = "player_male"

for o in list(scene.objects):
    if o.type == "MESH" and base(o.name) in DROP_MESHES:
        bpy.data.objects.remove(o)


def channelbag(action):
    return anim_utils.action_get_channelbag_for_slot(action, action.slots[0])


def bone_of(fc):
    m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
    return m.group(1) if m else None


# 1. Single skeleton. The copy kept is the one the animations animate.
used = {}
for act in bpy.data.actions:
    for fc in channelbag(act).fcurves:
        b = bone_of(fc)
        if b:
            used[b] = used.get(b, 0) + 1
keep_suffix = {}
for b in used:
    keep_suffix.setdefault(base(b), b)
bones_by_base = {}
for b in arm.data.bones:
    bones_by_base.setdefault(base(b.name), []).append(b.name)
keep = {bb: keep_suffix.get(bb, names[0]) for bb, names in bones_by_base.items()}

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
for bb, names in bones_by_base.items():
    for n in names:
        if n != keep[bb]:
            for child in eb[n].children:
                child.parent = eb[keep[bb]] if base(child.name) != bb else None
for bb, names in bones_by_base.items():
    for n in names:
        if n != keep[bb]:
            eb.remove(eb[n])
bpy.ops.object.mode_set(mode="OBJECT")
for bb, n in keep.items():
    arm.data.bones[n].name = bb
for o in scene.objects:
    if o.type == "MESH":
        for g in o.vertex_groups:
            g.name = base(g.name)
        for m in o.modifiers:
            if m.type == "ARMATURE":
                m.object = arm
        o.parent = arm
for act in bpy.data.actions:
    for fc in channelbag(act).fcurves:
        b = bone_of(fc)
        if b:
            fc.data_path = fc.data_path.replace(f'["{b}"]', f'["{base(b)}"]')
print(f"bones: {len(arm.data.bones)}")


# 2. Layered actions: curves of the top layer replace those of the base.
def compose(name, layers):
    out = bpy.data.actions[layers[0]].copy()
    out.name = name
    dst = channelbag(out)
    for top in layers[1:]:
        for fc in channelbag(bpy.data.actions[top]).fcurves:
            old = dst.fcurves.find(fc.data_path, index=fc.array_index)
            if old:
                dst.fcurves.remove(old)
            new = dst.fcurves.new(fc.data_path, index=fc.array_index, group_name=fc.group.name if fc.group else "")
            new.keyframe_points.add(len(fc.keyframe_points))
            for k_src, k_dst in zip(fc.keyframe_points, new.keyframe_points):
                k_dst.co = k_src.co
                k_dst.interpolation = "LINEAR"
    out.use_fake_user = True
    return out


final = [compose(n, l) for n, l in LAYERED.items()]
for n in SINGLE:
    a = bpy.data.actions[n]
    a.use_fake_user = True
    final.append(a)
arm.animation_data_create()
arm.animation_data.action = final[0]


# 3. Weapons on the hand sockets.
def attach(glb, bone):
    before = set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=glb)
    roots = [o for o in set(scene.objects) - before if o.parent is None]
    for o in roots:
        o.parent = arm
        o.parent_type = "BONE"
        o.parent_bone = bone
        # parent_type BONE attaches to the bone tail; the socket is the head.
        o.matrix_parent_inverse.identity()
        o.location = (0, -arm.data.bones[bone].length, 0)
    return roots


attach(sword_glb, "right_hand_attach")
attach(shield_glb, "left_hand_attach")

# Light, camera and world for the previews.
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.lens = 50
target = Vector((0, 0, 1.0))
cam.location = Vector((2.2, -4.2, 1.6))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam
for rot, energy in (((math.radians(50), 0, math.radians(150)), 5.0), ((math.radians(60), 0, math.radians(-30)), 3.0), ((math.radians(70), 0, math.radians(-120)), 2.0)):
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = rot
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("w")
world.color = (0.08, 0.08, 0.1)
scene.world = world
scene.render.resolution_x, scene.render.resolution_y = 360, 480

bpy.ops.wm.save_as_mainfile(filepath=out_blend)


# 4. Contact sheet: 5 frames per action, side by side.
def contact_sheet(action, n=5):
    arm.animation_data.action = action
    arm.animation_data.action_slot = action.slots[0]
    start, end = action.frame_range
    tiles = []
    for i in range(n):
        scene.frame_set(int(round(start + (end - start) * i / (n - 1))))
        # The animation moves the character (root motion); the camera follows the hips.
        hips = arm.matrix_world @ arm.pose.bones["Hips"].head
        focus = Vector((hips.x, hips.y, 1.0))
        cam.location = focus + Vector((2.2, -4.2, 0.6))
        cam.rotation_euler = (focus - cam.location).to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(preview_dir, f"_tmp_{i}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        w, h = img.size
        tiles.append(np.array(img.pixels[:]).reshape(h, w, 4))
        bpy.data.images.remove(img)
        os.remove(path)
    sheet = np.concatenate(tiles, axis=1)
    out = bpy.data.images.new(action.name, sheet.shape[1], sheet.shape[0], alpha=True)
    out.pixels = sheet.ravel()
    out.filepath_raw = os.path.join(preview_dir, f"{action.name}.png")
    out.file_format = "PNG"
    out.save()


for act in final:
    contact_sheet(act)
    print(f"preview: {act.name} {act.frame_range[:]}")
