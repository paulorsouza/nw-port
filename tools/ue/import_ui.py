"""Importa no Unreal as texturas e fontes da HUD do jogo (lista gerada por tools/ui_build_hud.py).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_ui.py"

Textura lyshineui/images/a/b.dds -> /Game/NW/UI/lyshineui/images/a/T_b (o ANWHUD resolve o mesmo caminho).
Fontes Nimbus Sans Novus (.otf do jogo) -> /Game/NW/UI/Fonts/FF_* (face) e F_Nimbus* (fonte de execução).
"""
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TEX = os.path.join(ROOT, "extract", "ui", "tex")
LISTS = [os.path.join(ROOT, "extract", "ui", n) for n in ("hud_textures.txt", "mastery_textures.txt", "login_textures.txt", "respawn_textures.txt", "group_textures.txt", "inventory_textures.txt")]
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def run_import(filename, dest, name):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = filename, dest, name
    task.automated = task.replace_existing = task.save = True
    tools.import_asset_tasks([task])
    return eal.load_asset(f"{dest}/{name}")


count = 0
lines = sorted({l.strip() for f in LISTS if os.path.exists(f) for l in open(f)})
for line in lines:
    path = line.strip()
    if not path:
        continue
    png = os.path.join(TEX, os.path.splitext(path)[0] + ".png")
    if not os.path.exists(png):
        unreal.log_warning(f"UI sem png: {path}")
        continue
    folder = "/Game/NW/UI/" + os.path.dirname(path)
    name = "T_" + os.path.splitext(os.path.basename(path))[0]
    if eal.does_asset_exist(f"{folder}/{name}"):
        count += 1
        continue
    tex = run_import(png, folder, name)
    if tex:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        eal.save_loaded_asset(tex, only_if_is_dirty=False)
        count += 1
unreal.log(f"UI texturas: {count}")

# Fontes: o ANWHUD carrega os .otf direto de Content/NW/UI/Fonts (o importador de fontes abre janela e
# derruba o commandlet). tools/ui_build_hud.py copia os arquivos.
