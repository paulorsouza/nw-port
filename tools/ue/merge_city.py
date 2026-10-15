"""Merges the city's loose pieces (old import, no folder) into instanced components and removes the empty actors.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/merge_city.py"

Puts the StaticMeshActors without a folder (except terrain and safety floor) in "Cidade/Construcoes", merges them with
UNWEditorTools.MergeFolderToInstances and removes the empty actors of the glb hierarchy (lights and the rest stay).
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
    raise RuntimeError("could not save the map")
unreal.log_warning(f"CITY merged: {moved} pieces → {merged} merged, {len(empty)} empty removed, "
                   f"{len(eas.get_all_level_actors())} actors on the map, {time.time() - t0:.0f}s")
