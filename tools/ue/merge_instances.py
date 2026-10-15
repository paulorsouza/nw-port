"""Merges the actors of each "Regiao/<layer>" folder of L_SouthWindsward into instanced components.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/merge_instances.py [folder...]"

No arguments: all Regiao/* folders. With 190k actors the editor ran out of memory when opening the map
(2026-10-11); merged, there is one actor per layer (UNWEditorTools.MergeFolderToInstances, C++).
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
# Empty nodes of the glb hierarchy (actors without a mesh): tens of thousands, just dead weight. Lights stay.
def drop_empty(folder):
    empty = [a for a in eas.get_all_level_actors()
             if str(a.get_folder_path()).startswith(folder) and type(a) is unreal.Actor
             and not a.get_actor_label().startswith("Instancias_")]
    eas.destroy_actors(empty)
    return len(empty)


total = 0
for f in folders:
    n = unreal.NWEditorTools.merge_folder_to_instances(world, f)
    unreal.log_warning(f"MERGE {f}: {n} actors, {drop_empty(f)} empty removed")
    total += n
if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log_warning(f"MERGE done: {total} actors in {len(folders)} folders, {len(eas.get_all_level_actors())} actors on the map, {time.time() - t0:.0f}s")
