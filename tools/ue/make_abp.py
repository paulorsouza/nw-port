"""Cria ABP_NW e ABP_Lost (classe pai NWAnimInstance, esqueleto de cada conjunto). O grafo é montado à mão no editor.

    py "<repo>/tools/ue/make_abp.py"
"""
import unreal

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
        unreal.log_error(f"{name}: esqueleto {skeleton} não existe; rode o import_nw.py antes")
        continue
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("target_skeleton", eal.load_asset(skeleton))
    factory.set_editor_property("parent_class", parent)
    abp = tools.create_asset(name, folder, unreal.AnimBlueprint, factory)
    eal.save_loaded_asset(abp)
    unreal.log(f"{name}: criado")
