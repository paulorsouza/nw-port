"""Imports a city (glb from `nwbt models slices`) into its own map, with collision, light and a start point.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_city.py <city.glb> <Name>"

Creates /Game/NW/Cities/<Name>/ (meshes, materials, textures) and /Game/NW/Maps/L_<Name>.
"""
import sys
import time

import unreal

args = [a for a in sys.argv[1:]]
GLB, NAME = args[0], args[1]
CONTENT = f"/Game/NW/Cities/{NAME}"
MAP = f"/Game/NW/Maps/L_{NAME}"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
t0 = time.time()

if eal.does_asset_exist(MAP):
    les.load_level(MAP)
    eas.destroy_actors(eas.get_all_level_actors())
elif not les.new_level(MAP):
    raise RuntimeError(f"could not create {MAP}")

im = unreal.InterchangeManager.get_interchange_manager_scripted()
src = unreal.InterchangeManager.create_source_data(GLB)
params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = True
ok = im.import_scene(CONTENT, src, params)
actors = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)]
unreal.log(f"CITY import_scene={ok} actors={len(actors)} in {time.time() - t0:.0f}s")

# Per-triangle collision on all meshes: you can walk on streets, stairs and roofs.
meshes = [eal.load_asset(p) for p in eal.list_assets(CONTENT, recursive=True)]
meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
for m in meshes:
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
unreal.log(f"CITY collision on {len(meshes)} meshes")

# Light, sky and start point. The city actors do not come with a 180° rotation, but I use names in the Rotator.
def spawn(cls, loc, rot=(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator())
    a.set_actor_rotation(unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]), False)
    if label:
        a.set_actor_label(label)
    return a


sun = spawn(unreal.DirectionalLight, (0, 0, 10000), (-38, -40, 0), "Sol")
sun.light_component.set_intensity(8.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, (0, 0, 0), label="Ceu")
sky = spawn(unreal.SkyLight, (0, 0, 5000), label="LuzDoCeu")
sky.light_component.set_editor_property("real_time_capture", True)
spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Neblina")
# Starts above the center and falls onto whatever is below (street, plaza or roof).
spawn(unreal.PlayerStart, (0, 0, 8000), label="Inicio")

for p in eal.list_assets(CONTENT, recursive=True):
    eal.save_asset(p, only_if_is_dirty=True)
if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log(f"CITY ready: {MAP} in {time.time() - t0:.0f}s")
