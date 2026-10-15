"""Imports the Windsward background sounds (ogg extracted from the game) as looping SoundWaves.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_audio.py"

Territory and city music (extract/audio/music/gameplay/territory) and ambience rendered from the game's .txtp files
(extract/audio/ambient/windsward). Output /Game/NW/Audio/{Music,Ambient}/S_<name>, used by UNWAmbienceComponent.
"""
import glob
import os

import unreal

# Repository root from the project (unreal/NWPort/), with no machine-specific path.
ROOT = os.path.join(os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", "..")), "extract", "audio")
SETS = {
    "Music": [os.path.join(ROOT, "music/gameplay/territory", f"{n}.ogg") for n in ("windsward_ambient", "windsward_settlement_august")],
    "Ambient": sorted(glob.glob(os.path.join(ROOT, "ambient/windsward/*.ogg"))),
}
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
n = 0
for folder, files in SETS.items():
    for f in files:
        if not os.path.exists(f):
            unreal.log_warning(f"AUDIO missing {f}")
            continue
        name = "S_" + os.path.splitext(os.path.basename(f))[0]
        task = unreal.AssetImportTask()
        task.filename = f
        task.destination_path = f"/Game/NW/Audio/{folder}"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        path = f"/Game/NW/Audio/{folder}/{name}"
        s = eal.load_asset(path)
        if s:
            s.set_editor_property("looping", True)
            eal.save_asset(path, only_if_is_dirty=False)
            n += 1
unreal.log_warning(f"AUDIO {n} sounds imported")
