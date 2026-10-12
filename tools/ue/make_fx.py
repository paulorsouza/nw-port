"""Materiais e ajustes simples de efeito de combate (o port ainda não converte as partículas do jogo):

- /Game/NW/Materials/M_NW_FX: sem luz, aditivo, emissivo = Color × Intensity (parâmetros), para rastro da flecha,
  clarão da explosão e nuvem de veneno (ANWProjectile, ANWFxActor).
- Montagens do arco (corpo superior): transições curtas (entrada 0,1 s, saída 0,15 s) e AM_Bw_AimStart terminando
  normalmente (antes segurava o último quadro e nunca acabava, travando a puxada).

    python3 tools/ue/remote.py tools/ue/make_fx.py          (editor aberto)
    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/make_fx.py"
"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

PATH = "/Game/NW/Materials/M_NW_FX"
if not eal.does_asset_exist(PATH):
    m = tools.create_asset("M_NW_FX", "/Game/NW/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.85, 0.6, 1.0))
    inten = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 250)
    inten.set_editor_property("parameter_name", "Intensity")
    inten.set_editor_property("default_value", 4.0)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(inten, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_asset(PATH, only_if_is_dirty=False)
    unreal.log("FX: M_NW_FX criado")

ANIMS = "/Game/NW/sword_shield/Anims"
for name in eal.list_assets(ANIMS, recursive=False):
    short = name.split("/")[-1].split(".")[0]
    if not short.startswith("AM_Bw_"):
        continue
    am = unreal.load_asset(f"{ANIMS}/{short}")
    if not isinstance(am, unreal.AnimMontage):
        continue
    if short == "AM_Bw_AimStart":
        am.set_editor_property("enable_auto_blend_out", True)
    for prop, t in (("blend_in", 0.1), ("blend_out", 0.15)):
        b = am.get_editor_property(prop)
        b.set_editor_property("blend_time", t)
        am.set_editor_property(prop, b)
    eal.save_loaded_asset(am, only_if_is_dirty=False)
unreal.log("FX: montagens do arco ajustadas")
