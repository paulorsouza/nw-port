"""Coloca o terreno (glb do tools/make_terrain.py) no mapa de uma cidade já importada por import_city.py.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_terrain.py <terreno.glb> <Nome> <grama.png> <terra.png>"

- Malhas em /Game/NW/Cities/<Nome>/Terrain, atores "terreno_*" no mapa (removidos e recriados a cada execução).
- Material M_NW_Terreno: grama e terra misturadas pela cor de vértice R (peso de estrada do jogo).
- Nanite desligado e colisão por triângulo, como em fix_city.py.
- PlayerStart no ponto do <terreno>.json; sobra só um chão de segurança.
"""
import json
import os
import sys

import unreal

GLB, NAME, GRASS, SOIL = sys.argv[1:5]
CONTENT = f"/Game/NW/Cities/{NAME}/Terrain"
MAP = f"/Game/NW/Maps/L_{NAME}"
P_MAT = "/Game/NW/Materials"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = unreal.log


def import_texture(png):
    name = "T_" + os.path.splitext(os.path.basename(png))[0]
    path = f"{CONTENT}/{name}"
    if not eal.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename, task.destination_path, task.destination_name = png, CONTENT, name
        task.automated = task.replace_existing = task.save = True
        tools.import_asset_tasks([task])
    return eal.load_asset(path)


def terrain_material(grass, soil):
    path = f"{P_MAT}/M_NW_Terreno"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = tools.create_asset("M_NW_Terreno", P_MAT, unreal.Material, unreal.MaterialFactoryNew())
    def tex(t, name, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", t)
        return n
    g, s = tex(grass, "Grama", -250), tex(soil, "Terra", 50)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -700, 350)
    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -300, 0)
    mel.connect_material_expressions(g, "RGB", lerp, "A")
    mel.connect_material_expressions(s, "RGB", lerp, "B")
    mel.connect_material_expressions(vc, "R", lerp, "Alpha")
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 250)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


mat = terrain_material(import_texture(GRASS), import_texture(SOIL))

les.load_level(MAP)
old = [a for a in eas.get_all_level_actors() if a.get_actor_label().startswith("terreno_")]
eas.destroy_actors(old)
floors = sorted((a for a in eas.get_all_level_actors() if a.get_actor_label().startswith("ChaoSeguranca")),
                key=lambda a: a.get_actor_location().z)
eas.destroy_actors(floors[1:])
log(f"TERRENO removidos {len(old)} atores antigos e {len(floors) - 1} chãos repetidos")

im = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = True
im.import_scene(CONTENT, unreal.InterchangeManager.create_source_data(GLB), params)

meshes = [eal.load_asset(p) for p in eal.list_assets(CONTENT, recursive=True)
          if str(eal.find_asset_data(p).asset_class_path.asset_name) == "StaticMesh"]
for m in meshes:
    m.modify()
    ns = m.get_editor_property("nanite_settings")
    ns.enabled = False
    m.set_editor_property("nanite_settings", ns)
    m.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    m.set_material(0, mat)
    eal.save_loaded_asset(m, only_if_is_dirty=False)

terrain = [a for a in eas.get_all_level_actors()
           if isinstance(a, unreal.StaticMeshActor) and a.get_actor_label().startswith("terreno_")]
for a in terrain:
    a.static_mesh_component.set_editor_property("override_materials", [])
log(f"TERRENO {len(meshes)} malhas, {len(terrain)} atores")

# Rede de segurança (ChaoSeguranca do fix_city) 10 m abaixo do ponto mais baixo do terreno: no lugar antigo (abaixo
# da cidade) ela ficava acima dos vales e rios, e o personagem andava sobre ela "voando".
if terrain:
    lowest = min(a.get_actor_bounds(False)[0].z - a.get_actor_bounds(False)[1].z for a in terrain)
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith("ChaoSeguranca"):
            a.set_actor_location(unreal.Vector(0, 0, lowest - 1000), False, False)
            log(f"TERRENO chão de segurança em {lowest - 1000:.0f}")

spawn = json.load(open(os.path.splitext(GLB)[0] + ".json"))["spawn"]
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(*spawn), False, False)
        log(f"TERRENO partida em {spawn}")

if not les.save_current_level():
    raise RuntimeError("não consegui salvar o mapa")
log(f"TERRENO pronto: {MAP}")
