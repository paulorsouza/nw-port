"""Places the region's water (glb from tools/make_water.py) in L_SouthWindsward, with a water material.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_water.py <water.glb>"

- Meshes in /Game/NW/Region/Water; actors in the "Regiao/Agua" folder (merged into one actor, removed and recreated
  each time). No collision, no Nanite (water is not opaque).
- M_NW_Agua: Single Layer Water shading model (Unreal's water model, with Lumen); vertex color R (steps =
  waterfalls) brightens and roughens it, like foam.
"""
import sys
import time

import unreal

GLB = sys.argv[1]
CONTENT = "/Game/NW/Region/Water"
MAP = "/Game/NW/Maps/L_SouthWindsward"
FOLDER = "Regiao/Agua"
P_MAT = "/Game/NW/Materials"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
t0 = time.time()


def add_water_output(m):
    """Single Layer Water needs its own output node (without it, it does not compile on SM6): scattering and
    absorption of greenish river water (absorbs more red), phase slightly forward."""
    mel = unreal.MaterialEditingLibrary
    out = mel.create_material_expression(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 650)
    for name, value, y in (("ScatteringCoefficients", (0.02, 0.05, 0.05), 600), ("AbsorptionCoefficients", (0.35, 0.09, 0.07), 700)):
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -250, y)
        c.set_editor_property("constant", unreal.LinearColor(*value, 1))
        mel.connect_material_expressions(c, "", out, name)
    g = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 800)
    g.set_editor_property("r", 0.2)
    mel.connect_material_expressions(g, "", out, "PhaseG")


def water_material():
    path = f"{P_MAT}/M_NW_Agua"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    m = tools.create_asset("M_NW_Agua", P_MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    MP = unreal.MaterialProperty
    deep = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -500, 0)
    deep.set_editor_property("constant", unreal.LinearColor(0.02, 0.05, 0.06, 1))
    foam = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -500, 150)
    foam.set_editor_property("constant", unreal.LinearColor(0.55, 0.62, 0.62, 1))
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -500, 300)
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -250, 50)
    mel.connect_material_expressions(deep, "", lerp, "A")
    mel.connect_material_expressions(foam, "", lerp, "B")
    mel.connect_material_expressions(vc, "R", lerp, "Alpha")
    mel.connect_material_property(lerp, "", MP.MP_BASE_COLOR)
    rough_still = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 450)
    rough_still.set_editor_property("r", 0.04)
    rough_foam = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 550)
    rough_foam.set_editor_property("r", 0.6)
    lerp_r = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -250, 450)
    mel.connect_material_expressions(rough_still, "", lerp_r, "A")
    mel.connect_material_expressions(rough_foam, "", lerp_r, "B")
    mel.connect_material_expressions(vc, "R", lerp_r, "Alpha")
    mel.connect_material_property(lerp_r, "", MP.MP_ROUGHNESS)
    add_water_output(m)
    mel.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


mat = water_material()
les.load_level(MAP)
eas.destroy_actors([a for a in eas.get_all_level_actors() if str(a.get_folder_path()) == FOLDER])
before = set(a.get_path_name() for a in eas.get_all_level_actors())
im = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = True
im.import_scene(CONTENT, unreal.InterchangeManager.create_source_data(GLB), params)
new = [a for a in eas.get_all_level_actors() if a.get_path_name() not in before]
for a in new:
    a.set_folder_path(FOLDER)
    if isinstance(a, unreal.StaticMeshActor):
        c = a.static_mesh_component
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", False)
        for i in range(c.get_num_materials()):
            c.set_material(i, mat)
meshes = [eal.load_asset(p) for p in eal.list_assets(CONTENT, recursive=True)]
for m in meshes:
    if isinstance(m, unreal.StaticMesh):
        ns = m.get_editor_property("nanite_settings")
        ns.enabled = False
        m.set_editor_property("nanite_settings", ns)
        m.set_material(0, mat)
for p in eal.list_assets(CONTENT, recursive=True):
    eal.save_asset(p, only_if_is_dirty=True)
eas.destroy_actors([a for a in new if type(a) is unreal.Actor])
if not les.save_current_level():
    raise RuntimeError("could not save the map")
unreal.log_warning(f"WATER {len(new)} actors in {time.time() - t0:.0f}s")
