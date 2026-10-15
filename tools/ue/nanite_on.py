"""Turns Nanite on for the static meshes in a folder (city, terrain).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/nanite_on.py /Game/NW/Cities /Game/NW/Terrain"

Needs SM6 on (DefaultEngine.ini: SF_VULKAN_SM6 on Linux, PCD3D_SM6 on Windows). Collision still comes from the
full mesh (complex as simple), not from the Nanite proxy.
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
        m.set_editor_property("nanite_settings", ns)  # triggers the mesh rebuild
        # set_editor_property does not mark the package dirty: save without relying on that.
        eal.save_loaded_asset(m, only_if_is_dirty=False)
        on += 1
unreal.log(f"NANITE on for {on}/{total} meshes")
