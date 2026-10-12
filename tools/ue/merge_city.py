"""Junta as peças soltas da cidade (import antigo, sem pasta) em componentes instanciados e tira os atores vazios.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/merge_city.py"

Põe os StaticMeshActor sem pasta (menos terreno e chão de segurança) em "Cidade/Construcoes", junta com
UNWEditorTools.MergeFolderToInstances e remove os atores vazios da hierarquia do glb (luzes e o resto ficam).
"""
import time

import unreal

MAP = "/Game/NW/Maps/L_SouthWindsward"
FOLDER = "Cidade/Construcoes"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
t0 = time.time()
les.load_level(MAP)
moved = 0
for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if isinstance(a, unreal.StaticMeshActor) and str(a.get_folder_path()) in ("", "None") \
            and not label.startswith(("terreno_", "ChaoSeguranca")):
        a.set_folder_path(FOLDER)
        moved += 1
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
merged = unreal.NWEditorTools.merge_folder_to_instances(world, FOLDER)
empty = [a for a in eas.get_all_level_actors() if type(a) is unreal.Actor and not a.get_actor_label().startswith("Instancias_")
         and not a.get_components_by_class(unreal.PrimitiveComponent)]
eas.destroy_actors(empty)
if not les.save_current_level():
    raise RuntimeError("não consegui salvar o mapa")
unreal.log_warning(f"CIDADE juntada: {moved} peças → {merged} juntadas, {len(empty)} vazios removidos, "
                   f"{len(eas.get_all_level_actors())} atores no mapa, {time.time() - t0:.0f}s")
