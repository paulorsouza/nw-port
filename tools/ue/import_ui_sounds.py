"""Imports the station interface sounds (extract/audio/ui/crafting, tools/extract_audio.py) as SoundWaves.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_ui_sounds.py"

Output /Game/NW/Audio/UI/S_<name> (e.g. S_play_ui_crafting_rarity_3), played by FNWRefiningMenu (craftroll-equipamento.md
§9). An event with random variations becomes <event>_01, _02...; the C++ picks one at random.
"""
import glob
import os

import unreal

ROOT = os.path.join(os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", "..")), "extract", "audio", "ui", "crafting")
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
n = 0
for f in sorted(glob.glob(os.path.join(ROOT, "**", "*.ogg"), recursive=True)):
    name = "S_" + os.path.splitext(os.path.basename(f))[0]
    task = unreal.AssetImportTask()
    task.filename = f
    task.destination_path = "/Game/NW/Audio/UI"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    if eal.does_asset_exist(f"/Game/NW/Audio/UI/{name}"):
        n += 1
unreal.log_warning(f"UI SOUNDS {n} imported from {ROOT}")
