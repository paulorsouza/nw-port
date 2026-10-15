# Functions shared by the Blender scripts that assemble the game character.
import re
import bpy
from bpy_extras import anim_utils

SUFFIX = re.compile(r"\.\d{3}$")
# Base cdf meshes that are not part of the visual: shadow proxy, stray sphere and the sleeveless variant.
DROP_MESHES = {"xray", "Icosphere", "shirt2"}


def base(name):
    return SUFFIX.sub("", name)


def channelbag(action):
    return anim_utils.action_get_channelbag_for_slot(action, action.slots[0])


def bone_of(fc):
    m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
    return m.group(1) if m else None


def import_character(glb, name="player_male", drop=None):
    """Imports the nwbt glb and leaves a single skeleton.

    drop: meshes to remove by name (default DROP_MESHES, for the player; NPCs use shirt2 as clothing).
    The glb carries one skeleton copy per mesh (suffixes .001, .002...). The copy the animations
    animate is kept; vertex groups and curves switch to the names without suffix.
    """
    scene = bpy.context.scene
    bpy.ops.import_scene.gltf(filepath=glb)
    arm = next(o for o in scene.objects if o.type == "ARMATURE")
    arm.name = name

    for o in list(scene.objects):
        if o.type == "MESH" and base(o.name) in (DROP_MESHES if drop is None else drop):
            bpy.data.objects.remove(o)

    used = set()
    for act in bpy.data.actions:
        for fc in channelbag(act).fcurves:
            b = bone_of(fc)
            if b:
                used.add(b)
    keep_suffix = {}
    for b in sorted(used):
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
    return arm


def reset_pose(arm):
    """Returns every bone to rest. A bone without a curve in the action keeps the last evaluated value
    (from another animation): without this, the export writes the previous action's pose on those bones."""
    for pb in arm.pose.bones:
        pb.location = (0.0, 0.0, 0.0)
        pb.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pb.rotation_euler = (0.0, 0.0, 0.0)
        pb.scale = (1.0, 1.0, 1.0)


def set_action(arm, action):
    reset_pose(arm)
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = action
    arm.animation_data.action_slot = action.slots[0]


def _source_image(socket):
    """First image upstream of a shader input (search through mix/math/normal map nodes)."""
    if not socket.is_linked:
        return None
    stack, seen = [socket.links[0].from_node], set()
    while stack:
        node = stack.pop(0)
        if node.name in seen:
            continue
        seen.add(node.name)
        if node.type == "TEX_IMAGE" and node.image:
            return node
        stack += [i.links[0].from_node for i in node.inputs if i.is_linked]
    return None


def flatten_materials(tex_dir):
    """Puts each material in the form FBX carries (image straight into base color, normal and alpha)
    and writes the packed images as PNG in tex_dir. Returns material -> {channel: png}."""
    import os
    os.makedirs(tex_dir, exist_ok=True)
    saved, table = {}, {}
    for img in bpy.data.images:
        if img.packed_file is None and not img.filepath:
            continue
        name = os.path.basename(img.name).split(".dds")[0].replace("/", "_") + ".png"
        path = os.path.join(tex_dir, name)
        if path not in saved.values():
            img.filepath_raw = path
            img.file_format = "PNG"
            img.save()
        saved[img.name] = path
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            continue
        nt = mat.node_tree
        bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        entry = {}
        for channel in ("Base Color", "Normal", "Alpha", "Specular IOR Level", "Emission Color"):
            tex = _source_image(bsdf.inputs[channel])
            if tex and tex.image:
                entry[channel] = saved.get(tex.image.name)
        base = _source_image(bsdf.inputs["Base Color"])
        if base:
            for l in list(bsdf.inputs["Base Color"].links):
                nt.links.remove(l)
            nt.links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
        alpha = _source_image(bsdf.inputs["Alpha"])
        if alpha:
            for l in list(bsdf.inputs["Alpha"].links):
                nt.links.remove(l)
            nt.links.new(alpha.outputs["Alpha"], bsdf.inputs["Alpha"])
        # The game's _emis comes with intensity ~30 (.mtl Emittance) and blows the color out to white.
        for l in list(bsdf.inputs["Emission Color"].links):
            nt.links.remove(l)
        bsdf.inputs["Emission Strength"].default_value = 0.0
        table[mat.name] = entry
    return table


# Armor piece suffix -> bare body mesh it covers.
SLOT_OF = {"_chest": "shirt1", "_thighs": "pants1", "_calves": "boots1", "_forearms": "gloves1"}


def attach_armor(arm, glb):
    """Imports the pieces of an armor (nwbt glb) onto the character skeleton.
    Returns (pieces, covered body meshes)."""
    scene = bpy.context.scene
    before = set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=glb)
    new = set(scene.objects) - before
    pieces = [o for o in new if o.type == "MESH" and base(o.name) != "Icosphere"]
    # The glb repeats cape and skirt in two nodes with the same mesh: one object per mesh is kept.
    seen = set()
    for o in sorted(pieces, key=lambda o: o.name):
        if o.data.name in seen:
            pieces.remove(o)
        seen.add(o.data.name)
    for o in [o for o in new if o not in pieces]:
        bpy.data.objects.remove(o)
    for o in pieces:
        for g in o.vertex_groups:
            g.name = base(g.name)
        for m in o.modifiers:
            if m.type == "ARMATURE":
                m.object = arm
        o.parent = arm
        o.matrix_parent_inverse.identity()
    names = {base(o.name).lower() for o in pieces}
    covered = {slot for suf, slot in SLOT_OF.items() for n in names if n.endswith(suf)}
    return pieces, covered


def to_centimeters(arm, meshes):
    """Takes mesh, skeleton and animations to centimeters (Unreal's unit), with no scale on object or bone.

    Exporting in meters leaves the factor 100 on the "Armature" object, which Unreal drops: the reference pose
    gets the root bone at scale 100 and the animations (without bind pose) arrive with the root at scale 1.
    """
    from mathutils import Matrix
    k = 100.0
    scale = Matrix.Scale(k, 4)
    arm.data.transform(scale)
    # Once per mesh: objects sharing the mesh would scale it again (cape at 100x, the "shapes in the sky").
    for data in {o.data for o in meshes}:
        data.transform(scale)
    for act in bpy.data.actions:
        for fc in channelbag(act).fcurves:
            if fc.data_path.endswith(".location") or fc.data_path == "location":
                for kp in fc.keyframe_points:
                    kp.co[1] *= k
                    kp.handle_left[1] *= k
                    kp.handle_right[1] *= k
    us = bpy.context.scene.unit_settings
    us.system = "METRIC"
    us.scale_length = 0.01
    us.length_unit = "CENTIMETERS"
