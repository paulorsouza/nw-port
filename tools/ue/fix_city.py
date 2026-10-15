"""Fixes on the map of a city imported by import_city.py.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_city.py <Name>"

- Nanite off on the meshes (without SM6 on Vulkan/Linux they do not show). With SM6 on, run tools/ue/nanite_on.py afterwards.
- Per-triangle collision (complex as simple).
- Movable sun and sky (dynamic lighting: no "lighting needs to be rebuilt").
- Invisible safety floor under everything and the start point 35 m above the center.
"""
import sys

import unreal

NAME = sys.argv[1]
CONTENT = f"/Game/NW/Cities/{NAME}"
MAP = f"/Game/NW/Maps/L_{NAME}"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

paths = [p for p in eal.list_assets(CONTENT, recursive=True)
         if str(eal.find_asset_data(p).asset_class_path.asset_name) == "StaticMesh"]
off = 0
for p in paths:
    m = eal.load_asset(p)
    m.modify()
    ns = m.get_editor_property("nanite_settings")
    if ns.enabled:
        ns.enabled = False
        m.set_editor_property("nanite_settings", ns)  # triggers the mesh rebuild
        off += 1
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    # A change through set_editor_property does not mark the package dirty: save without relying on that.
    eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log(f"CITY nanite off on {off}/{len(paths)} meshes")

les.load_level(MAP)
actors = eas.get_all_level_actors()
zs = [a.get_actor_location().z for a in actors if isinstance(a, unreal.StaticMeshActor)]
for a in actors:
    if isinstance(a, (unreal.DirectionalLight, unreal.SkyLight)):
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    if isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(0, 0, 3500), False, False)

# Safety net: large invisible plane 5 m below the lowest piece.
floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, min(zs) - 500), unreal.Rotator())
floor.set_actor_label("ChaoSeguranca")
floor.static_mesh_component.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(600, 600, 1))
floor.static_mesh_component.set_visibility(False)

if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log(f"CITY fixed: {MAP}")
