"""Cria ABP_NW e ABP_Lost (classe pai NWAnimInstance, esqueleto de cada conjunto). O grafo é montado à mão no editor.

    py "<repo>/tools/ue/make_abp.py"            ABP_NW e ABP_Lost (apaga e recria)
    py "<repo>/tools/ue/make_abp.py" creature   só o ABP_NW_Creature, se ainda não existir

ABP_NW_Creature é um ABP de modelo (template, sem esqueleto): serve para qualquer criatura (javali, lobo...). Grafo,
uma vez no editor: Blend Space Player com o pino Blend Space exposto e ligado à variável Locomotion (o C++ põe o
BS_Walk do conjunto), Speed/Direction nos eixos; depois Slot 'DefaultSlot' e Output Pose.
"""
import sys

import unreal

if len(sys.argv) > 1 and sys.argv[1] == "creature":
    path = "/Game/NW/Creatures/ABP_NW_Creature"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("ABP_NW_Creature: já existe (grafo feito à mão; não recria)")
    else:
        f = unreal.AnimBlueprintFactory()
        f.set_editor_property("parent_class", unreal.load_class(None, "/Script/NWPort.NWAnimInstance"))
        f.set_editor_property("template", True)
        abp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("ABP_NW_Creature", "/Game/NW/Creatures", unreal.AnimBlueprint, f)
        unreal.EditorAssetLibrary.save_loaded_asset(abp)
        unreal.log("ABP_NW_Creature: criado (montar o grafo no editor)")
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
            unreal.log_error(f"{name}: esqueleto {skeleton} não existe; rode o import_nw.py antes")
            continue
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property("target_skeleton", eal.load_asset(skeleton))
        factory.set_editor_property("parent_class", parent)
        abp = tools.create_asset(name, folder, unreal.AnimBlueprint, factory)
        eal.save_loaded_asset(abp)
        unreal.log(f"{name}: criado")
