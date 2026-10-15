"""Caps the maximum texture size in a folder (8 GB of VRAM: the landscape has ~530 textures).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/cap_textures.py /Game/NW/Gatherables 1024"

0 removes the cap (original size).
"""
import sys

import unreal

ROOT, MAX = sys.argv[1], int(sys.argv[2])
eal = unreal.EditorAssetLibrary
n = 0
for p in eal.list_assets(ROOT, recursive=True):
    a = eal.load_asset(p)
    if isinstance(a, unreal.Texture2D) and a.get_editor_property("max_texture_size") != MAX \
            and (MAX == 0 or max(a.blueprint_get_size_x(), a.blueprint_get_size_y()) > MAX):
        a.set_editor_property("max_texture_size", MAX)
        eal.save_asset(p, only_if_is_dirty=False)
        n += 1
unreal.log_warning(f"TEXTURES {n} {'uncapped' if MAX == 0 else f'capped at {MAX}'} in {ROOT}")
