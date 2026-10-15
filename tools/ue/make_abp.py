"""Creates ABP_NW and ABP_Lost (parent class NWAnimInstance, each set's skeleton). The graph is built by hand in the editor.

    py "<repo>/tools/ue/make_abp.py"            ABP_NW and ABP_Lost (deletes and recreates)
    py "<repo>/tools/ue/make_abp.py" creature   only ABP_NW_Creature, if it does not exist yet

ABP_NW_Creature is a template ABP (no skeleton): it works for any creature (boar, wolf...). Graph, once in the
editor: Blend Space Player with the Blend Space pin exposed and bound to the Locomotion variable (the C++ sets the
set's BS_Walk), Speed/Direction on the axes; then Slot 'DefaultSlot' and Output Pose.
"""
import sys

import unreal

if len(sys.argv) > 1 and sys.argv[1] == "creature":
    path = "/Game/NW/Creatures/ABP_NW_Creature"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("ABP_NW_Creature: already exists (graph made by hand; not recreated)")
    else:
        f = unreal.AnimBlueprintFactory()
        f.set_editor_property("parent_class", unreal.load_class(None, "/Script/NWPort.NWAnimInstance"))
        f.set_editor_property("template", True)
        abp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("ABP_NW_Creature", "/Game/NW/Creatures", unreal.AnimBlueprint, f)
        unreal.EditorAssetLibrary.save_loaded_asset(abp)
        unreal.log("ABP_NW_Creature: created (build the graph in the editor)")
else:

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    parent = unreal.load_class(None, "/Script/NWPort.NWAnimInstance")

    for name, folder, skeleton in (
        ("ABP_NW", "/Game/NW/sword_shield", "/Game/NW/sword_shield/Mesh/SK_player_male_Skeleton"),
        ("ABP_Lost", "/Game/NW/lost_sword", "/Game/NW/lost_sword/Mesh/SK_lost_Skeleton"),
    ):
        if eal.does_asset_exist(f"{folder}/{name}"):
            eal.delete_asset(f"{folder}/{name}")
        if not eal.does_asset_exist(skeleton):
            unreal.log_error(f"{name}: skeleton {skeleton} does not exist; run import_nw.py first")
            continue
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property("target_skeleton", eal.load_asset(skeleton))
        factory.set_editor_property("parent_class", parent)
        abp = tools.create_asset(name, folder, unreal.AnimBlueprint, factory)
        eal.save_loaded_asset(abp)
        unreal.log(f"{name}: created")
