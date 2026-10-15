"""Imports the equipment appearances (tools/convert_appearances.py) onto the player skeleton.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript \
        -script="tools/ue/import_appearances.py <repo>/extract/ue/appearances"
    Materials only (redoes instances and slots): add --materials-only.

Before: import_nw.py of the sword_shield set (SK_player_male, its skeleton and M_NW_SpecGloss[_Masked]).
Creates, under /Game/NW/Appearances:
    Body/SK_body_<part>     base body parts (head, shirt1, gloves1, pants1, boots1), skeletal, player skeleton
    Armor/SK_<appearance>   one armor appearance, skeletal, player skeleton
    Weapons/SM_<appearance> weapon/shield/tool static mesh, origin at the grip point (as the set's SM_sword)
Materials and textures of each mesh go to a subfolder with its name (materials of different meshes share names):
MI_<material> children of M_NW_SpecGloss (or _Masked when the diffuse has alpha), as in import_npcs.py.
Data/Items/appearances.json (tools/make_appearances.py) points to these paths.
"""
import glob
import json
import os
import re
import sys

import unreal

SRC = next(a for a in sys.argv[1:] if not a.startswith("--"))
MATERIALS_ONLY = "--materials-only" in sys.argv
DEST = "/Game/NW/Appearances"
SET = "/Game/NW/sword_shield"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
log = unreal.log
# The Interchange importer ignores FbxImportUI; the classic one respects the chosen skeleton.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")


def run_import(filename, dest, options=None, name=None):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path = filename, dest
    if name:
        task.destination_name = name
    task.automated = task.replace_existing = task.save = True
    if options is not None:
        task.options = options
    tools.import_asset_tasks([task])
    return list(task.imported_object_paths)


def load(path):
    return eal.load_asset(path.split(".")[0] if "." in path else path)


player = load(f"{SET}/Mesh/SK_player_male")
if not player:
    raise RuntimeError(f"missing {SET}/Mesh/SK_player_male: import the sword_shield set first (import_nw.py)")
skeleton = player.get_editor_property("skeleton")
master = load(f"{SET}/Materials/M_NW_SpecGloss")
master_masked = load(f"{SET}/Materials/M_NW_SpecGloss_Masked")


def import_texture(png, folder, normal=False):
    name = "T_" + re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(os.path.basename(png))[0])
    path = f"{folder}/{name}"
    if not eal.does_asset_exist(path):
        run_import(png, folder, None, name)
    tex = load(path)
    if normal and tex:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # PNG in OpenGL convention (like import_nw.py)
        eal.save_loaded_asset(tex)
    return tex


def slot_key(name):
    # glb material "body_mat.001" becomes slot "body_mat_001" in the FBX; the importer may add "_skin".
    n = str(name).lower().replace(".", "_")
    return re.sub(r"_skin\d*$", "", n)


def skeletal_options():
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton = skeleton
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
    return ui


def static_options():
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.import_materials = False
    ui.import_textures = False
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    return ui


def nanite_off(mesh):
    # Skinned parts follow the leader pose: no Nanite (the engine version may not expose the setting).
    try:
        ns = mesh.get_editor_property("nanite_settings")
        if ns.enabled:
            ns.enabled = False
            mesh.set_editor_property("nanite_settings", ns)
    except Exception:
        pass


def instances_for(fbx, folder):
    chans_by_mat = json.load(open(os.path.join(os.path.dirname(fbx), "materials.json")))
    out = {}
    for mat_name, chans in chans_by_mat.items():
        if "Base Color" not in chans:
            continue
        asset = "MI_" + re.sub(r"[^A-Za-z0-9_]", "_", mat_name)
        mi_path = f"{folder}/{asset}"
        mi = load(mi_path) if eal.does_asset_exist(mi_path) else tools.create_asset(
            asset, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master_masked if chans.get("Alpha") else master)
        mel.set_material_instance_texture_parameter_value(mi, "Diffuse", import_texture(chans["Base Color"], folder))
        if chans.get("Specular IOR Level"):
            spec = import_texture(chans["Specular IOR Level"], folder)
            mel.set_material_instance_texture_parameter_value(mi, "Spec", spec)
        if chans.get("Normal"):
            normal = import_texture(chans["Normal"], folder, True)
            mel.set_material_instance_texture_parameter_value(mi, "Normal", normal)
        eal.save_loaded_asset(mi)
        out[slot_key(mat_name)] = mi
    return out


KINDS = {"Body": True, "Armor": True, "Weapons": False}  # kind -> skinned
fbxs = [(kind, f) for kind in KINDS for f in sorted(glob.glob(os.path.join(SRC, kind, "S*_*", "S*_*.fbx")))]

# 1. Meshes.
if not MATERIALS_ONLY:
    for kind, fbx in fbxs:
        name = os.path.splitext(os.path.basename(fbx))[0]
        opts = skeletal_options() if KINDS[kind] else static_options()
        if not run_import(fbx, f"{DEST}/{kind}", opts, name):
            unreal.log_error(f"APPEARANCE import failed: {fbx}")

# 2. Materials, in a pass after all the meshes (assigning right after each import lost the slots: import_npcs.py).
done = missing = 0
for kind, fbx in fbxs:
    name = os.path.splitext(os.path.basename(fbx))[0]
    path = f"{DEST}/{kind}/{name}"
    mesh = load(path)
    if mesh is None:
        unreal.log_error(f"APPEARANCE without mesh: {path}")
        continue
    instances = instances_for(fbx, f"{DEST}/{kind}/{name}")
    mesh.modify()
    prop = "materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials"
    mats = mesh.get_editor_property(prop)
    for i in range(len(mats)):
        slot = mats[i]
        mi = instances.get(slot_key(slot.get_editor_property("material_slot_name")))
        if mi is None:
            missing += 1
            slot_name = slot.get_editor_property("material_slot_name")
            unreal.log_warning(f"APPEARANCE {name}: slot {slot_name} without material")
        elif prop == "materials":
            # Iterating yields copies of the slots: change the copy and put it back by index.
            slot.set_editor_property("material_interface", mi)
            mats[i] = slot
        else:
            mesh.set_material(i, mi)  # as tools/ue/catalog.py
    if isinstance(mesh, unreal.SkeletalMesh):
        mesh.set_editor_property(prop, mats)
        nanite_off(mesh)
    eal.save_asset(path, only_if_is_dirty=False)
    done += 1
unreal.log_warning(f"APPEARANCE materials on {done}/{len(fbxs)} meshes; slots without material: {missing}")

# A piece with a bone the player skeleton lacks (cloth): Unreal adds it to the skeleton in memory when loading the
# mesh. Save the skeleton after everything (as import_npcs.py), otherwise the editor warns "missing bones".
nw_tools = getattr(unreal, "NWEditorTools", None)
if nw_tools:
    for kind, fbx in fbxs:
        if not KINDS[kind]:
            continue
        name = os.path.splitext(os.path.basename(fbx))[0]
        sk = load(f"{DEST}/{kind}/{name}")
        if sk and nw_tools.merge_mesh_bones_to_skeleton(skeleton, sk):
            log(f"APPEARANCE bones of {name} added to the player skeleton")
eal.save_loaded_asset(skeleton, False)
