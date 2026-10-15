"""Places the region's pre-generated scenery (glb from `nwbt models chunks`, in world coordinates) in L_SouthWindsward.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_region_scene.py <scenery.glb> <Name>"

- Assets in /Game/NW/Region/<Name>; one "Instancias_<Name>" actor in the Outliner folder "Regiao/<Name>", with one
  instanced component per mesh (removed and recreated each time).
- World → map frame (the city's, tools/make_terrain.py): u = Rz(turn) · (u_world − u_city), in cm.
- Per-triangle collision and Nanite on the meshes; materials made ready for Nanite/instances by fix_nanite_materials.py.
"""
import glob
import json
import math
import os
import sys
import time

import unreal

GLB, NAME = sys.argv[1], sys.argv[2]
# Repository root from the project (unreal/NWPort/), with no machine-specific path.
ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
CONTENT = f"/Game/NW/Region/{NAME}"
MAP = "/Game/NW/Maps/L_SouthWindsward"
FOLDER = f"Regiao/{NAME}"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
t0 = time.time()

caps = glob.glob(os.path.join(ROOT, "extract/terrain/**/r_+04_+01/capitals/06swindsward_persistence/*.capitals.json"), recursive=True)[0]
city = next(c for c in json.load(open(caps))["Capitals"] if c["sliceName"].endswith("Settlement_SouthWindsward_Complete"))
T = city["worldPosition"]
ang = math.degrees(2 * math.atan2(city["rotation"]["z"], city["rotation"]["w"]))
# Cry (x, y, z) m → UE (−x, y, z) cm; the mirror on X flips the turn direction: Rz_ue(turn).
to_map = unreal.Transform(location=[-T["x"] * 100, T["y"] * 100, T["z"] * 100]).inverse()
to_map = to_map * unreal.Transform(rotation=unreal.Rotator(roll=0, pitch=0, yaw=ang))

les.load_level(MAP)
old = [a for a in eas.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
eas.destroy_actors(old)
before = set(a.get_path_name() for a in eas.get_all_level_actors())

im = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = True
ok = im.import_scene(CONTENT, unreal.InterchangeManager.create_source_data(GLB), params)
new = [a for a in eas.get_all_level_actors() if a.get_path_name() not in before]
for a in new:
    a.set_actor_transform(a.get_actor_transform() * to_map, False, False)
    a.set_folder_path(FOLDER)
unreal.log_warning(f"REGION import_scene={ok} actors={len(new)} turn={ang:.1f} in {time.time() - t0:.0f}s")
# One actor per layer, with instanced components (tens of thousands of actors exhaust the editor's memory).
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
merged = unreal.NWEditorTools.merge_folder_to_instances(world, FOLDER)
# Empty nodes of the glb hierarchy (actors without a mesh): just dead weight. Lights stay.
eas.destroy_actors([a for a in eas.get_all_level_actors() if str(a.get_folder_path()) == FOLDER
                    and type(a) is unreal.Actor and not a.get_actor_label().startswith("Instancias_")])

meshes = [eal.load_asset(p) for p in eal.list_assets(CONTENT, recursive=True)]
meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
for m in meshes:
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    ns = m.get_editor_property("nanite_settings")
    ns.enabled = True
    m.set_editor_property("nanite_settings", ns)
for p in eal.list_assets(CONTENT, recursive=True):
    eal.save_asset(p, only_if_is_dirty=True)
if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log_warning(f"REGION ready: {len(meshes)} meshes, {len(new)} pieces merged ({merged}) in {time.time() - t0:.0f}s")
