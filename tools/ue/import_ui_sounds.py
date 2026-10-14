"""Importa os sons da interface das estações (extract/audio/ui/crafting, tools/extract_audio.py) como SoundWave.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_ui_sounds.py"

Saída /Game/NW/Audio/UI/S_<nome> (ex.: S_play_ui_crafting_rarity_3), tocada por FNWRefiningMenu (craftroll-equipamento.md
§9). Um evento com variações aleatórias vira <evento>_01, _02...; o C++ sorteia entre elas.
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
unreal.log_warning(f"UI SONS {n} importados de {ROOT}")
