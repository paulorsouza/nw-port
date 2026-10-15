# blender -b -P tools/export_ue.py -- extract/ue/<set>/manifest.json
#
# Exports for Unreal, from the nwbt glb files:
#   fbx/SK_<character>.fbx    mesh + skeleton, no animation
#   fbx/anim/A_<clip>.fbx     skeleton only + one animation (root motion on the Xform root bone)
#   fbx/SM_<weapon>.fbx       weapons, origin at the grip point
# and adds the generated paths to the manifest.
import json
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nwchar  # noqa: E402

mpath = sys.argv[sys.argv.index("--") + 1]
manifest = json.load(open(mpath))
out = os.path.join(os.path.dirname(mpath), "fbx")
os.makedirs(os.path.join(out, "anim"), exist_ok=True)

FBX_COMMON = dict(
    use_selection=True,
    add_leaf_bones=False,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_NONE",
    mesh_smooth_type="FACE",
    use_armature_deform_only=False,
)


def select_only(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = manifest["fps"]
arm = nwchar.import_character(manifest["glb"]["character"])
# Unreal drops the armature object as root bone only when it is named "Armature".
arm.name = "Armature"
if manifest["glb"].get("armor"):
    _, covered = nwchar.attach_armor(arm, manifest["glb"]["armor"])
    for o in [o for o in scene.objects if o.type == "MESH" and nwchar.base(o.name) in covered]:
        bpy.data.objects.remove(o)
skinned = {k: nwchar.attach_armor(arm, glb)[0] for k, glb in manifest["glb"].get("skinned_weapons", {}).items()}
skinned_objs = {o for objs in skinned.values() for o in objs}
meshes = [o for o in scene.objects if o.type == "MESH" and o.parent == arm and o not in skinned_objs]
nwchar.to_centimeters(arm, meshes + list(skinned_objs))
clips = {a.name.lower(): a for a in bpy.data.actions}
missing = [c for c in manifest["clips"] if c not in clips]
if missing:
    raise SystemExit(f"actions missing from the glb: {missing}")


def compose(asset, layers, loop):
    """Copy of the base layer; curves of the upper layers replace those of the same bone.
    In loops, the upper layer is stretched to the base duration (Mannequin syncs by normalized time)."""
    base = clips[layers[0]]
    out_act = base.copy()
    out_act.name = asset
    dst = nwchar.channelbag(out_act)
    b0, b1 = base.frame_range
    for top_name in layers[1:]:
        top = clips[top_name]
        t0, t1 = top.frame_range
        k = (b1 - b0) / (t1 - t0) if loop and t1 > t0 else 1.0
        for fc in nwchar.channelbag(top).fcurves:
            old = dst.fcurves.find(fc.data_path, index=fc.array_index)
            if old:
                dst.fcurves.remove(old)
            new = dst.fcurves.new(fc.data_path, index=fc.array_index, group_name=fc.group.name if fc.group else "")
            new.keyframe_points.add(len(fc.keyframe_points))
            for ks, kd in zip(fc.keyframe_points, new.keyframe_points):
                kd.co = (b0 + (ks.co[0] - t0) * k, ks.co[1])
                kd.interpolation = "LINEAR"
    out_act.use_fake_user = True
    return out_act


ROOT_BONE = next(b.name for b in arm.data.bones if b.parent is None)


def hips_yaw(act):
    """Mean hip angle relative to the character's front (-Y), in degrees; + = turned right."""
    import math
    nwchar.set_action(arm, act)
    vals = []
    for f in range(int(act.frame_range[0]), int(round(act.frame_range[1])) + 1):
        scene.frame_set(f)
        W, P = arm.matrix_world, arm.pose.bones
        side = (W @ P["Thigh_right"].head) - (W @ P["Thigh_left"].head)
        vals.append(math.degrees(math.atan2(-(-side.y), -side.x)))
    return sum(vals) / len(vals)


def straighten(act):
    """Rotates the root bone so the loop's mean hip points forward (test: without the guard pose twist)."""
    import math
    from mathutils import Matrix
    yaw = hips_yaw(act)
    rz = Matrix.Rotation(math.radians(yaw), 4, "Z")
    pb = arm.pose.bones[ROOT_BONE]
    frames = range(int(act.frame_range[0]), int(round(act.frame_range[1])) + 1)
    mats = []
    for f in frames:
        scene.frame_set(f)
        mats.append(rz @ pb.matrix)
    for f, mtx in zip(frames, mats):
        pb.matrix = mtx
        pb.keyframe_insert("rotation_quaternion", frame=f)
    print(f"straightened {act.name}: {yaw:+.1f}° -> {hips_yaw(act):+.1f}°")


def root_motion(act):
    """Root bone displacement between the first and last frame, in meters (object space)."""
    nwchar.set_action(arm, act)
    pb = arm.pose.bones[ROOT_BONE]
    scene.frame_set(int(act.frame_range[0]))
    p0 = pb.head.copy()
    scene.frame_set(int(round(act.frame_range[1])))
    return [round(x, 3) for x in (pb.head - p0)]


materials = nwchar.flatten_materials(os.path.join(out, "textures"))

# Mesh: rest pose, no animation.
arm.animation_data_clear()
sk = os.path.join(out, f"{manifest.get('mesh_name', 'SK_player_male')}.fbx")
select_only([arm] + meshes)
bpy.ops.export_scene.fbx(filepath=sk, object_types={"ARMATURE", "MESH"}, bake_anim=False,
                         path_mode="COPY", embed_textures=True, **FBX_COMMON)
# Skinned weapons (gauntlet glove): one skeletal mesh per weapon, on the same skeleton (SK_<key>).
skinned_fbx = {}
for key, objs in skinned.items():
    skinned_fbx[key] = os.path.join(out, f"SK_{key}.fbx")
    select_only([arm] + objs)
    bpy.ops.export_scene.fbx(filepath=skinned_fbx[key], object_types={"ARMATURE", "MESH"}, bake_anim=False,
                             path_mode="COPY", embed_textures=True, **FBX_COMMON)

anims = {}
for key, comp in manifest["composites"].items():
    act = compose(comp["asset"], comp["layers"], comp["loop"])
    # Clip speed (m/s, on the plane) before removing the root bone translation: the game's blend spaces
    # use it when the sample has no MoveSpeed.
    rm0 = root_motion(act)
    dur = max((act.frame_range[1] - act.frame_range[0]) / scene.render.fps, 1e-6)
    comp["clip_speed"] = round((rm0[0] ** 2 + rm0[1] ** 2) ** 0.5 / 100.0 / dur, 3)
    if comp["loop"]:
        # Loops stay in place (CharacterMovement does the moving): root bone translation zeroed.
        # Zero, do not delete: without a curve the bone keeps the last evaluated value (e.g. the end of the cycle) and
        # the export writes that offset as a constant, moving the body off the capsule.
        cb = nwchar.channelbag(act)
        for fc in [fc for fc in cb.fcurves if fc.data_path == f'pose.bones["{ROOT_BONE}"].location']:
            for kp in fc.keyframe_points:
                kp.co[1] = 0.0
                kp.handle_left[1] = 0.0
                kp.handle_right[1] = 0.0
            fc.update()
        arm.pose.bones[ROOT_BONE].location = (0.0, 0.0, 0.0)
        # Only idle or forward loops: rotating a sideways clip would also rotate the step direction.
        import math
        travel = math.degrees(math.atan2(-rm0[0], -rm0[1])) if comp["clip_speed"] > 0.2 else 0.0
        # Clips already in place in the game (no translation) do not tell the direction: the name suffix decides.
        import re
        lateral = re.search(r"_(l|r|fl|fr|b|bl|br)$", comp["layers"][0]) is not None
        if manifest.get("straighten_loops") and abs(travel) < 25.0 and not lateral:
            straighten(act)
    bones = {nwchar.bone_of(fc) for fc in nwchar.channelbag(act).fcurves}
    rm = root_motion(act)
    nwchar.set_action(arm, act)
    start, end = act.frame_range
    scene.frame_start, scene.frame_end = int(start), int(round(end))
    path = os.path.join(out, "anim", f"{comp['asset']}.fbx")
    select_only([arm])
    # Scene already in centimeters (nwchar.to_centimeters): no scale factor on object or bone.
    anim_opts = dict(FBX_COMMON)
    bpy.ops.export_scene.fbx(
        filepath=path, object_types={"ARMATURE"}, bake_anim=True,
        bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, **anim_opts)
    comp.update(fbx=path, frames=int(round(end - start)) + 1, animated_bones=len(bones), root_motion=rm)
    anims[comp["asset"]] = path
    flag = "  <- partial" if len(bones) < 60 else ""
    print(f"anim {comp['asset']:42} frames={comp['frames']:4} bones={len(bones):3} root={rm}{flag}")

# .blend to open in Blender: character, weapons on the sockets and every action (Action Editor).
for key, glb in manifest["glb"]["weapons"].items():
    before = set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=glb)
    bone = manifest["weapons"][key]["socket"]
    for o in set(scene.objects) - before:
        if o.parent is None:
            o.parent, o.parent_type, o.parent_bone = arm, "BONE", bone
            o.matrix_parent_inverse.identity()
            o.location = (0, -arm.data.bones[bone].length, 0)
            # The glTF importer already converts to the scene unit (cm): scale 1.
            # Rotation: the Blender bone is -90° on local X relative to the game joint (tools/ue/grip_offsets.md).
            o.rotation_mode = "XYZ"
            o.rotation_euler = (-1.5707963, 0.0, 0.0)
            o.name = f"SM_{key}"
idle = next(c["asset"] for k, c in manifest["composites"].items() if k == manifest["loops"].get("Idle"))
nwchar.set_action(arm, bpy.data.actions[idle])
scene.frame_start, scene.frame_end = 0, int(bpy.data.actions[idle].frame_range[1])
guide = bpy.data.texts.new("LEIA")
guide.write("Actions (Action Editor, Armature object):\n")
for name, comp_key in manifest["loops"].items():
    guide.write(f"  loop {name}: {manifest['composites'][comp_key]['asset']}\n")
for name, keys in manifest["montages"].items():
    guide.write(f"  {name}: {', '.join(manifest['composites'][k]['asset'] for k in keys)}\n")
for name, bs in manifest["blendspaces"].items():
    guide.write(f"  {name}: {len(bs['samples'])} samples (see manifest.json)\n")
blend_path = os.path.join(os.path.dirname(mpath), f"NW_{manifest['name']}.blend")
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=blend_path, compress=True)
manifest["blend"] = blend_path

# Attacks with no damage window in the .adb (common for NPCs): window = fast stretch of the weapon hand around the peak.
ATTACK_HINTS = ("attack", "heavy", "leap", "spin", "swipe", "combo", "sprintattack")  # shield (bash/rush) hits by area
HAND = next((b for b in ("right_hand_attach", "Hand_right") if b in arm.pose.bones), None)


def swing_window(act):
    nwchar.set_action(arm, act)
    f0, f1 = int(act.frame_range[0]), int(round(act.frame_range[1]))
    pos = []
    for f in range(f0, f1 + 1):
        scene.frame_set(f)
        pos.append((arm.matrix_world @ arm.pose.bones[HAND].head).copy())
    # speed relative to the hip, so the whole body's displacement is not counted
    speed = [0.0] + [(pos[i] - pos[i - 1]).length for i in range(1, len(pos))]
    peak = max(range(len(speed)), key=lambda i: speed[i])
    lim = speed[peak] * 0.45
    a = b = peak
    while a > 0 and speed[a - 1] >= lim:
        a -= 1
    while b < len(speed) - 1 and speed[b + 1] >= lim:
        b += 1
    fps = scene.render.fps
    return round(a / fps, 4), round((b + 1) / fps, 4)


if HAND:
    acts = {a.name: a for a in bpy.data.actions}
    windows = manifest.setdefault("windows", {})
    for montage, keys in manifest["montages"].items():
        if not any(h in montage.lower() for h in ATTACK_HINTS):
            continue
        secs = windows.setdefault(montage, [{"fragment": "", "windows": []} for _ in keys])
        while len(secs) < len(keys):
            secs.append({"fragment": "", "windows": []})
        for i, key in enumerate(keys):
            act = acts.get(manifest["composites"][key]["asset"])
            if act is None:
                continue
            t0, t1 = swing_window(act)
            # The game window counts if it covers the swing peak; otherwise (e.g. it came from a sequence with another clip) add the computed one.
            mid = (t0 + t1) / 2
            covered = any(w["type"] in ("CAGE-Damage", "WeaponEffects") and w["start"] <= mid <= (w["end"] if w["end"] is not None else 1e9)
                          for w in secs[i]["windows"])
            if covered:
                continue
            secs[i]["windows"].append({"type": "CAGE-Damage", "start": t0, "end": t1, "name": "swing", "params": {}})
            print(f"computed damage window: {montage}[{i}] {t0:.2f}–{t1:.2f}s")

weapons = {}
for key, glb in manifest["glb"]["weapons"].items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=glb)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    materials.update(nwchar.flatten_materials(os.path.join(out, "textures")))
    path = os.path.join(out, f"SM_{key}.fbx")
    select_only(objs)
    bpy.ops.export_scene.fbx(filepath=path, object_types={"MESH"}, bake_anim=False,
                             path_mode="COPY", embed_textures=True, **FBX_COMMON)
    weapons[key] = path

manifest["materials"] = materials
manifest["fbx"] = {"skeletal_mesh": sk, "anims": anims, "weapons": weapons, "skinned_weapons": skinned_fbx,
                   "root_bone": ROOT_BONE}
json.dump(manifest, open(mpath, "w"), indent=1)
print(f"export_ue: SK + {len(anims)} animations + {len(weapons)} weapons")
