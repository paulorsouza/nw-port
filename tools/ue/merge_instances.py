"""Junta os atores de cada pasta "Regiao/<camada>" do L_SouthWindsward em componentes instanciados.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/merge_instances.py [pasta...]"

Sem argumentos: todas as pastas Regiao/*. Com 190 mil atores o editor esgotava a memória ao abrir o mapa
(2026-10-11); juntando fica um ator por camada (UNWEditorTools.MergeFolderToInstances, C++).
"""
import sys
import time

import unreal

MAP = "/Game/NW/Maps/L_SouthWindsward"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
t0 = time.time()

les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
folders = sys.argv[1:] or sorted({str(a.get_folder_path()) for a in eas.get_all_level_actors()
                                   if str(a.get_folder_path()).startswith("Regiao/")})
# Nós vazios da hierarquia do glb (atores sem malha): dezenas de milhares, só pesam. Luzes ficam.
def drop_empty(folder):
    empty = [a for a in eas.get_all_level_actors()
             if str(a.get_folder_path()).startswith(folder) and type(a) is unreal.Actor
             and not a.get_actor_label().startswith("Instancias_")]
    eas.destroy_actors(empty)
    return len(empty)


total = 0
for f in folders:
    n = unreal.NWEditorTools.merge_folder_to_instances(world, f)
    unreal.log_warning(f"JUNTAR {f}: {n} atores, {drop_empty(f)} vazios removidos")
    total += n
if not les.save_current_level():
    raise RuntimeError("não consegui salvar o mapa")
unreal.log_warning(f"JUNTAR pronto: {total} atores em {len(folders)} pastas, {len(eas.get_all_level_actors())} atores no mapa, {time.time() - t0:.0f}s")
