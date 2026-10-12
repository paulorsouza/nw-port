"""Ajustes no mapa de uma cidade importada por import_city.py.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_city.py <Nome>"

- Nanite desligado nas malhas (sem SM6 no Vulkan/Linux elas não aparecem). Com SM6 ligado, rodar tools/ue/nanite_on.py depois.
- Colisão por triângulo (complex as simple).
- Sol e céu móveis (iluminação dinâmica: sem "precisa reconstruir a iluminação").
- Chão de segurança invisível embaixo de tudo e ponto de partida a 35 m sobre o centro.
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
        m.set_editor_property("nanite_settings", ns)  # dispara o rebuild da malha
        off += 1
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    # A mudança por set_editor_property não marca o pacote como modificado: salvar sem depender disso.
    eal.save_loaded_asset(m, only_if_is_dirty=False)
unreal.log(f"CIDADE nanite desligado em {off}/{len(paths)} malhas")

les.load_level(MAP)
actors = eas.get_all_level_actors()
zs = [a.get_actor_location().z for a in actors if isinstance(a, unreal.StaticMeshActor)]
for a in actors:
    if isinstance(a, (unreal.DirectionalLight, unreal.SkyLight)):
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    if isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(0, 0, 3500), False, False)

# Rede de segurança: plano grande e invisível 5 m abaixo da peça mais baixa.
floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, min(zs) - 500), unreal.Rotator())
floor.set_actor_label("ChaoSeguranca")
floor.static_mesh_component.set_static_mesh(eal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(600, 600, 1))
floor.static_mesh_component.set_visibility(False)

if not les.save_current_level():
    raise RuntimeError("não consegui salvar o mapa")
unreal.log(f"CIDADE ajustada: {MAP}")
