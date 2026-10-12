"""Limita o tamanho máximo das texturas de uma pasta (VRAM de 8 GB: a paisagem tem ~530 texturas).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/cap_textures.py /Game/NW/Gatherables 1024"

Com 0 tira o limite (tamanho original).
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
unreal.log_warning(f"TEXTURAS {n} {'sem limite' if MAX == 0 else f'limitadas a {MAX}'} em {ROOT}")
