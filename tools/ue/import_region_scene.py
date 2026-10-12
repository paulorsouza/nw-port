"""Coloca no L_SouthWindsward o cenário pré-gerado da região (glb do `nwbt models chunks`, em coordenadas do mundo).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_region_scene.py <cenario.glb> <Nome>"

- Assets em /Game/NW/Region/<Nome>; atores na pasta "Regiao/<Nome>" do Outliner (removidos e recriados a cada vez).
- Mundo → referencial do mapa (o da cidade, tools/make_terrain.py): u = Rz(giro) · (u_mundo − u_cidade), em cm.
- Colisão por triângulo e Nanite nas malhas; materiais prontos para Nanite/instâncias com fix_nanite_materials.py.
"""
import glob
import json
import math
import os
import sys
import time

import unreal

GLB, NAME = sys.argv[1], sys.argv[2]
ROOT = os.path.expanduser("~/workspace/nw-port")
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
# Cry (x, y, z) m → UE (−x, y, z) cm; o espelho em X inverte o sentido do giro: Rz_ue(giro).
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
unreal.log_warning(f"REGIAO import_scene={ok} atores={len(new)} giro={ang:.1f} em {time.time() - t0:.0f}s")

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
    raise RuntimeError("não consegui salvar o mapa")
unreal.log_warning(f"REGIAO pronta: {len(meshes)} malhas, {len(new)} atores em {time.time() - t0:.0f}s")
