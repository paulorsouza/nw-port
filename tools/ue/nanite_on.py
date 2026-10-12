"""Liga Nanite nas malhas estáticas de uma pasta (cidade, terreno).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/nanite_on.py /Game/NW/Cities /Game/NW/Terrain"

Precisa do SM6 ligado (DefaultEngine.ini: SF_VULKAN_SM6 no Linux, PCD3D_SM6 no Windows). A colisão continua
saindo da malha completa (complex as simple), não do proxy do Nanite.
"""
import sys

import unreal

eal = unreal.EditorAssetLibrary
total = on = 0
for folder in sys.argv[1:]:
    for p in eal.list_assets(folder, recursive=True):
        if str(eal.find_asset_data(p).asset_class_path.asset_name) != "StaticMesh":
            continue
        total += 1
        m = eal.load_asset(p)
        ns = m.get_editor_property("nanite_settings")
        if ns.enabled:
            continue
        m.modify()
        ns.enabled = True
        m.set_editor_property("nanite_settings", ns)  # dispara o rebuild da malha
        # set_editor_property não marca o pacote como modificado: salvar sem depender disso.
        eal.save_loaded_asset(m, only_if_is_dirty=False)
        on += 1
unreal.log(f"NANITE ligado em {on}/{total} malhas")
