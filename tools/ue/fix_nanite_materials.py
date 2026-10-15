"""Nanite-ready materials: turns on "Used with Nanite" and "Used with Instanced Static Meshes" on the materials and turns Nanite off on meshes
that use a translucent/additive material (Nanite only draws opaque and masked). Instances whose parent is outside /Game
(Interchange's glTF uses /InterchangeAssets/gltf/M_Default) switch to a copy of the parent in /Game/NW/Materials/Interchange,
with the flag: the engine install is not modified.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_nanite_materials.py /Game/NW/Cities /Game/NW/Gatherables ..."

Without the flag, the game outside the editor (-game, package) swaps the material for the default: the city showed up
gray/dark (2026-10-11). Run after importing new meshes with Nanite (tools/ue/nanite_on.py, import_gatherables.py).
"""
import sys
import time

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
t0 = time.time()
OK_BLEND = (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED)

assets = []
for root in sys.argv[1:]:
    assets += eal.list_assets(root, recursive=True)

COPIES = "/Game/NW/Materials/Interchange"
copies = {}


def project_parent(parent):
    """Copy in /Game of a parent that lives in a plugin/the engine; an intermediate instance takes the chain along."""
    path = parent.get_path_name().split(".")[0]
    if path.startswith("/Game/"):
        return parent
    if path not in copies:
        dst = f"{COPIES}/{path.split('/')[-1]}"
        copy = eal.load_asset(dst) if eal.does_asset_exist(dst) else eal.duplicate_asset(path, dst)
        if isinstance(copy, unreal.MaterialInstanceConstant):
            mel.set_material_instance_parent(copy, project_parent(parent.get_editor_property("parent")))
        else:
            copy.set_editor_property("used_with_nanite", True)
            copy.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(copy)
        eal.save_asset(dst, only_if_is_dirty=False)
        copies[path] = copy
    return copies[path]


reparented = 0
for p in assets:
    a = eal.load_asset(p)
    if isinstance(a, unreal.MaterialInstanceConstant) and a.get_editor_property("parent"):
        parent = a.get_editor_property("parent")
        if not parent.get_path_name().startswith("/Game/"):
            mel.set_material_instance_parent(a, project_parent(parent))
            eal.save_asset(p, only_if_is_dirty=False)
            reparented += 1

flagged, already = 0, 0
for p in assets:
    a = eal.load_asset(p)
    if isinstance(a, unreal.Material):
        # Nanite and instanced meshes (the gatherables landscape uses UInstancedStaticMeshComponent).
        if a.get_editor_property("used_with_nanite") and a.get_editor_property("used_with_instanced_static_meshes"):
            already += 1
            continue
        a.set_editor_property("used_with_nanite", True)
        a.set_editor_property("used_with_instanced_static_meshes", True)
        mel.recompile_material(a)
        eal.save_asset(p, only_if_is_dirty=False)
        flagged += 1


def base_blend(mi):
    """Effective blend mode: the first override in the instance chain, otherwise the base material's."""
    m = mi
    while isinstance(m, unreal.MaterialInstance):
        ov = m.get_editor_property("base_property_overrides")
        if ov and ov.get_editor_property("override_blend_mode"):
            return ov.get_editor_property("blend_mode")
        m = m.get_editor_property("parent")
    return m.get_editor_property("blend_mode") if m else unreal.BlendMode.BLEND_OPAQUE


nanite_off = []
for p in assets:
    a = eal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh):
        continue
    ns = a.get_editor_property("nanite_settings")
    if not ns.enabled:
        continue
    mats = [s.material_interface for s in a.get_editor_property("static_materials") if s.material_interface]
    if any(base_blend(m) not in OK_BLEND for m in mats):
        ns.enabled = False
        a.set_editor_property("nanite_settings", ns)
        eal.save_asset(p, only_if_is_dirty=False)
        nanite_off.append(p.split("/")[-1].split(".")[0])

unreal.log_warning(f"NANITE instances with copied parent={reparented} ({list(copies)}) materials flagged={flagged} "
           f"already had it={already} meshes without Nanite (translucent)={len(nanite_off)} "
           f"{nanite_off[:15]} in {time.time() - t0:.0f}s")
