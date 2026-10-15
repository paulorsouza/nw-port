"""Game particle effect → Niagara system (docs/tech/particles.md).

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_fx.py extract/fx/<Lib>.<Effect>.json"

Reads the JSON from tools/make_particles.py, imports textures (extract/fx/src/**.png) and meshes (extract/fx/glb/**.glb)
into /Game/NW/FX, creates the particle master materials (translucent/additive sprite, mesh, decal) and one instance per
game material, translates each CryEngine emitter into a Niagara recipe (engine template, modules, constants, curves,
renderer) and calls UNWNiagaraTools.build_effect. Report in extract/fx/<Lib>.<Effect>.build.txt.
"""
import json
import os
import sys

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "extract", "fx", "src")
GLB = os.path.join(ROOT, "extract", "fx", "glb")
P_FX = "/Game/NW/FX"
P_TEX = f"{P_FX}/Tex"
P_MAT = f"{P_FX}/Materials"
P_MESH = f"{P_FX}/Mesh"
FOUNTAIN = "/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain"
eal = unreal.EditorAssetLibrary


def existing(path):
    """Loads by path: in the open editor the registry does not see what the commandlet created (does_asset_exist false)."""
    name = path.rsplit("/", 1)[-1]
    try:
        return unreal.load_asset(f"{path}.{name}")
    except Exception:
        return None
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
M = 100.0  # game meter → cm


def asset_name(path):
    return os.path.splitext(os.path.basename(path))[0].replace("-", "_").replace(".", "_")


def texture(game_path, normal=None):
    """textures/x/y.tif → /Game/NW/FX/Tex/T_y (png from extract/fx/src)."""
    if not game_path:
        return None
    png = os.path.join(SRC, os.path.splitext(game_path.lower())[0] + ".png")
    name = "T_" + asset_name(game_path)
    if normal is None:
        normal = asset_name(game_path).lower().endswith(("_ddn", "_ddna"))
    path = f"{P_TEX}/{name}"
    if not existing(path) and os.path.exists(png):
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = P_TEX
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        t = unreal.load_asset(path)
        if t and normal:
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
            eal.save_loaded_asset(t)
    return unreal.load_asset(path)


def mesh(game_path):
    """Effect mesh (glb from nwbt) in /Game/NW/FX/Mesh/SM_<name>/ (the import folder; moving it inside the editor does not save)."""
    name = "SM_" + asset_name(game_path)
    folder = f"{P_MESH}/{name}"
    found = [p for p in eal.list_assets(folder, recursive=True) if isinstance(eal.load_asset(p), unreal.StaticMesh)] \
        if eal.does_directory_exist(folder) else []
    if found:
        return found[0].split(".")[0]
    glb = os.path.join(GLB, game_path.lower() + ".glb")
    im = unreal.InterchangeManager.get_interchange_manager_scripted()
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    im.import_asset(folder, src, params)
    found = [p for p in eal.list_assets(folder, recursive=True) if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not found:
        unreal.log_warning(f"FX: mesh not imported {glb}")
        return None
    for p in eal.list_assets(folder, recursive=True):
        eal.save_asset(p, only_if_is_dirty=False)
    return found[0].split(".")[0]


# ---------------------------------------------------------------- master materials

def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def mask(mat, src, out, chans, x, y, rgba=False):
    """RGB = default output (float3); alpha = output "A" (the default output has no alpha). rgba: single-output node (DecalColor)."""
    m = node(mat, unreal.MaterialExpressionComponentMask, x, y, **{c: True for c in chans})
    mel.connect_material_expressions(src, "RGBA" if rgba else ("A" if chans == "a" else ""), m, "")
    return m


def mul(mat, a, b, x, y):
    n = node(mat, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, "", n, "A")
    mel.connect_material_expressions(b, "", n, "B")
    return n


def master(name, kind):
    path = f"{P_MAT}/{name}"
    # Rebuilt on every run (the graph belongs to this script).
    mat = existing(path)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, P_MAT, unreal.Material, unreal.MaterialFactoryNew())
    default_tex = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if kind == "decal":
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        # Texture spin (the game's RotationRateY, turns per second): rotates the UVs around the center.
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
        spin = node(mat, unreal.MaterialExpressionScalarParameter, -1500, 150, parameter_name="Spin", default_value=0.0)
        tm = node(mat, unreal.MaterialExpressionTime, -1500, 250)
        ang = mul(mat, spin, tm, -1300, 200)  # turns (Rotator: 1 = one full turn)
        rot = node(mat, unreal.MaterialExpressionRotator, -1100, 0, center_x=0.5, center_y=0.5, speed=6.2831853)
        mel.connect_material_expressions(uv, "", rot, "Coordinate")
        mel.connect_material_expressions(ang, "", rot, "Time")
        tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Tex", texture=default_tex)
        mel.connect_material_expressions(rot, "", tex, "UVs")
        col = node(mat, unreal.MaterialExpressionDecalColor, -900, 300)
        rgb = mul(mat, mask(mat, tex, "", "rgb", -600, 0), col, -350, 0)
        # Alpha from the texture only: wiring from Python does not find DecalColor's named outputs (only RGB).
        a = mask(mat, tex, "", "a", -600, 150)
        em = node(mat, unreal.MaterialExpressionScalarParameter, -600, 600, parameter_name="Emissive", default_value=0.5)
        mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(mul(mat, rgb, em, -150, 400), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    elif kind == "mesh_lit":
        # The game's ice meshes (Illum, Fxmeshadvanced): lit, normal, emissive and cyan rim (Fresnel, Rim_Major_Color
        # of the tomb ice); the particle alpha becomes a cutout (dissolve).
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("used_with_niagara_mesh_particles", True)
        normal_default = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
        pc = node(mat, unreal.MaterialExpressionParticleColor, -900, 300)
        tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Tex", texture=default_tex)
        nrm = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 150, parameter_name="Normal", texture=normal_default,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        emit = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 500, parameter_name="Emit", texture=default_tex)
        em = node(mat, unreal.MaterialExpressionScalarParameter, -900, 700, parameter_name="Emissive", default_value=0.0)
        rim = node(mat, unreal.MaterialExpressionVectorParameter, -900, 800, parameter_name="Rim",
                   default_value=unreal.LinearColor(0.23, 0.98, 1.0, 1.0))
        fres = node(mat, unreal.MaterialExpressionFresnel, -700, 850, exponent=2.0)
        base_rgb = mul(mat, mask(mat, tex, "", "rgb", -600, 0), mask(mat, pc, "", "rgb", -600, 300), -350, 0)
        glow = node(mat, unreal.MaterialExpressionAdd, -150, 600)
        mel.connect_material_expressions(mul(mat, mul(mat, mask(mat, emit, "", "rgb", -600, 500), em, -400, 550), mask(mat, pc, "", "rgb", -600, 650), -300, 550), "", glow, "A")
        mel.connect_material_expressions(mul(mat, mask(mat, rim, "", "rgb", -600, 800), fres, -400, 800), "", glow, "B")
        rough = node(mat, unreal.MaterialExpressionConstant, -300, 200, r=0.25)
        mel.connect_material_property(base_rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(mask(mat, pc, "", "a", -300, 400), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if kind == "sprite_add" else unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("used_with_niagara_sprites", True)
        mat.set_editor_property("used_with_niagara_mesh_particles", True)
        pc = node(mat, unreal.MaterialExpressionParticleColor, -900, 300)
        em = node(mat, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="Emissive", default_value=1.0)
        if kind == "mesh":
            # The game's Meshparticle: scrolling noise with alpha (Diffuse) and emissive (Emittance); rim more visible
            # (viewDependencyFactor) with Fresnel.
            uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
            p1 = node(mat, unreal.MaterialExpressionPanner, -1200, 0, speed_x=0.05, speed_y=0.1)
            p2 = node(mat, unreal.MaterialExpressionPanner, -1200, 200, speed_x=-0.07, speed_y=0.05)
            mel.connect_material_expressions(uv, "", p1, "Coordinate")
            mel.connect_material_expressions(uv, "", p2, "Coordinate")
            tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Tex", texture=default_tex)
            emit = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 150, parameter_name="Emit", texture=default_tex)
            mel.connect_material_expressions(p1, "", tex, "UVs")
            mel.connect_material_expressions(p2, "", emit, "UVs")
            fres = node(mat, unreal.MaterialExpressionFresnel, -600, 650, exponent=2.0)
            rgb = mul(mat, mul(mat, mask(mat, emit, "", "rgb", -600, 150), mask(mat, pc, "", "rgb", -600, 300), -400, 150), em, -200, 150)
            a = mul(mat, mul(mat, mask(mat, tex, "", "a", -600, 0), mask(mat, pc, "", "a", -600, 450), -400, 400), fres, -200, 400)
            # Veil: in the game the dome barely shows (maintainer's reference screenshot, 2026-10-15).
            op = node(mat, unreal.MaterialExpressionScalarParameter, -400, 600, parameter_name="Opacity", default_value=0.2)
            a = mul(mat, a, op, -100, 450)
        else:
            tex = node(mat, unreal.MaterialExpressionTextureSampleParameterSubUV, -900, 0, parameter_name="Tex", texture=default_tex)
            rgb = mul(mat, mul(mat, mask(mat, tex, "", "rgb", -600, 0), mask(mat, pc, "", "rgb", -600, 300), -400, 0), em, -200, 0)
            a = mul(mat, mask(mat, tex, "", "a", -600, 150), mask(mat, pc, "", "a", -600, 450), -400, 300)
            if kind == "sprite_add":
                rgb = mul(mat, rgb, a, -100, 100)
            # Soft particle (the game's SoftParticle).
            fade = node(mat, unreal.MaterialExpressionDepthFade, -300, 600)
            mel.connect_material_expressions(a, "", fade, "Opacity")
            a = fade
        mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if kind != "sprite_add":
            mel.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    return mat


def instance(name, parent, textures, scalars):
    path = f"{P_MAT}/{name}"
    mi = existing(path) or tools.create_asset(
        name, P_MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, t in textures.items():
        if t:
            mel.set_material_instance_texture_parameter_value(mi, k, t)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    eal.save_asset(path, only_if_is_dirty=False)
    return f"{path}.{name}"


# ---------------------------------------------------------------- CryEngine → recipe translation

def base(p, key, default):
    v = p.get(key, {})
    return v.get("base", default) if isinstance(v.get("base", default), (int, float, list)) else default


def rnd(p, key):
    return p.get(key, {}).get("random", 0.0)


def age(p, key):
    return p.get(key, {}).get("age")


def split3_all(s):
    """Splits on commas outside parentheses."""
    out, depth, cur = [], 0, ""
    for ch in s or "":
        depth += ch == "("
        depth -= ch == ")"
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def split3(s):
    """'X=0.5,Y=(25,Random=1),Z=1' → ['X=0.5', 'Y=(25,Random=1)', 'Z=1']."""
    out, depth, cur = [], 0, ""
    for ch in s:
        depth += ch == "("
        depth -= ch == ")"
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    by = {x.split("=")[0].strip().upper(): x for x in out if "=" in x}
    return [by.get(k, "") for k in ("X", "Y", "Z")] if by else []


def recipe(e, mats):
    """One game emitter → recipe for UNWNiagaraTools.build_effect (Fountain template)."""
    p = e["params"]
    life = base(p, "ParticleLifeTime", 1.0)
    life_min = life * (1.0 - rnd(p, "ParticleLifeTime"))
    count = base(p, "Count", 1.0)
    continuous = p.get("Continuous", {}).get("base") in ("true", True)
    pulse = base(p, "PulsePeriod", 0.0)
    size = base(p, "SizeX", 1.0)
    color = base(p, "Color", [1.0, 1.0, 1.0])
    r = {"name": e["name"].split(".")[-1].replace(" ", "_"), "template": FOUNTAIN, "disable": [], "add": [], "params": [],
         "constants": {}, "curves": []}
    c = r["constants"]
    if pulse > 0:
        c["SpawnRate.SpawnRate"] = count / pulse
    elif continuous:
        c["SpawnRate.SpawnRate"] = count / max(life, 0.01)
    else:
        r["disable"].append("SpawnRate")
        r["add"].append({"usage": "EmitterUpdate", "script": "/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"})
        c["SpawnBurst_Instantaneous.Spawn Count"] = int(count)

    # The effect lasts as long as the actor or notify does (tower 15 s, tomb 10 s): long loop, the burst fires only once.
    c["EmitterState.Loop Duration"] = 1000.0
    c["InitializeParticle.Lifetime Min"] = life_min
    c["InitializeParticle.Lifetime Max"] = life
    c["InitializeParticle.Color"] = list(color) + [1.0]
    c["InitializeParticle.Uniform Sprite Size Min"] = 2 * size * M * (1.0 - rnd(p, "SizeX"))
    c["InitializeParticle.Uniform Sprite Size Max"] = 2 * size * M
    c["ShapeLocation.Sphere Radius"] = base(p, "EmitterSizeDiameter", 0.0) * M / 2 if e["kind"] == "sprite" else 0.0
    c["GravityForce.Gravity"] = [0.0, 0.0, -980.0 * base(p, "GravityScale", 0.0)]
    c["Drag.Drag"] = base(p, "AirResistance", 0.0)
    speed = base(p, "Speed", 0.0)
    if speed > 0 and e["kind"] == "sprite":
        c["AddVelocity.Velocity"] = [0.0, 0.0, speed * M]
        c["AddVelocity.Cone Angle"] = base(p, "EmitAngle", 0.0)
    else:
        r["disable"].append("AddVelocity")
    alpha = age(p, "Alpha")
    r["curves"].append({"function": "FloatFromCurve", "index": 0, "keys": alpha if alpha else [[0, 1], [1, 1]]})
    mat = mats.get(e["name"])
    if e["kind"] == "decal":
        r["params"].append({"usage": "ParticleSpawn", "name": "Particles.DecalSize", "type": "vec3",
                            # Particles.DecalSize is the full size (not half); the game's Size is the radius.
                            "value": f"{4 * M},{2 * size * M},{2 * size * M}"})
        # Unreal decals project along local X: rotate 90° on Y to project downward (the game's Facing=Decal, on the ground).
        r["params"].append({"usage": "ParticleSpawn", "name": "Particles.MeshOrientation", "type": "quat",
                            "value": "0,0.70710678,0,0.70710678"})
        r["renderer"] = {"kind": "decal", "material": mat}
    elif e["kind"] == "mesh":
        r["renderer"] = {"kind": "mesh", "material": mat, "mesh": e.get("mesh_asset"), "mesh_scale": size}
    else:
        tiling = e["raw"].get("TextureTiling", "")
        tiles = dict(kv.split("=", 1) for kv in split3_all(tiling) if "=" in kv)
        tx = int(float(tiles.get("TilesX", 1)))
        ty = int(float(tiles.get("TilesY", 1)))
        r["renderer"] = {"kind": "sprite", "material": mat, "subuv": [tx, ty]}
        if e["raw"].get("OrientToVelocity") == "true" or e["facing"] == "Shape" or "MotionBlur" in e["raw"]:
            r["renderer"]["alignment"] = "velocity"
        if e["kind"] == "sprite" and e["raw"].get("EmitterShape") == "CIRCLE":
            # The game's ring (CIRCLE, tangential Velocity Y): flattened sphere, surface only + vortex.
            c["ShapeLocation.Non Uniform Scale"] = [1.0, 1.0, 0.02]
            c["ShapeLocation.Sphere Surface Distribution"] = 1.0
            # The game's Velocity X (radial), Y (tangent), Z (up): rise and vortex around the vertical axis.
            def first_num(x):
                import re
                m = re.search(r"-?\d+(\.\d+)?", x.split("=", 1)[1]) if "=" in x else None
                return float(m.group(0)) if m else 0.0
            v = [first_num(x) for x in split3(e["raw"].get("Velocity", ""))]
            if v:
                r["disable"] = [d for d in r["disable"] if d != "AddVelocity"]
                c["AddVelocity.Velocity"] = [0.0, 0.0, v[2] * M]
                c["AddVelocity.Cone Angle"] = 0.0
                r["add"].append({"usage": "ParticleUpdate", "script": "/Niagara/Modules/Update/Velocity/VortexVelocity.VortexVelocity"})
                c["VortexVelocity.Vortex Axis"] = [0.0, 0.0, 1.0]
                c["VortexVelocity.Velocity Amount"] = v[1] * M
    return r


def material_for(e, info, masters):
    """The emitter's material instance: by the shader of the game's .mtl, or by the emitter's texture (sprites)."""
    mname = e.get("material")
    raw = e["raw"]
    first = lambda k, d: float(str(raw.get(k, d)).split(",")[0] or d)
    if mname and mname in info:
        mi = info[mname]
        maps = mi["maps"]
        shader = mi["shader"]
        if shader == "Meshparticle":
            return instance("MI_" + asset_name(mname), masters["mesh"],
                            {"Tex": texture(maps.get("Diffuse")), "Emit": texture(maps.get("Emittance"))},
                            {"Emissive": float(mi["params"].get("Brightness", 1))})
        if e["kind"] == "decal":
            return instance("MI_" + asset_name(mname), masters["decal"], {"Tex": texture(maps.get("Diffuse"))},
                            {"Spin": first("RotationRateY", 0) / 360.0, "Emissive": first("EmissiveLighting", 0)})
        if shader in ("Illum", "Fxmeshadvanced"):
            return instance("MI_" + asset_name(mname), masters["mesh_lit"],
                            {"Tex": texture(maps.get("Diffuse")), "Normal": texture(maps.get("Bumpmap")),
                             "Emit": texture(maps.get("Emittance") or maps.get("Diffuse"))},
                            {"Emissive": 1.0 if maps.get("Emittance") else 0.0})
        tex = texture(maps.get("Diffuse") or maps.get("Custom") or maps.get("Detail"))
        if e["kind"] == "mesh":
            return instance("MI_" + asset_name(mname), masters["mesh"], {"Tex": tex, "Emit": tex},
                            {"Emissive": first("EmissiveLighting", 1), "Opacity": 0.6})
        kind = "sprite_add" if e["blend"] == "Additive" else "sprite"
        return instance("MI_" + asset_name(mname) + ("_add" if kind == "sprite_add" else ""), masters[kind], {"Tex": tex},
                        {"Emissive": first("EmissiveLighting", 1)})
    kind = "sprite_add" if e["blend"] == "Additive" else "sprite"
    if e["kind"] == "mesh":
        return instance("MI_mesh_" + asset_name(e["texture"] or "white"), masters["mesh"],
                        {"Tex": texture(e["texture"]), "Emit": texture(e["texture"])}, {"Emissive": first("EmissiveLighting", 1), "Opacity": 0.6})
    return instance("MI_" + asset_name(e["texture"] or "white") + ("_add" if kind == "sprite_add" else ""), masters[kind],
                    {"Tex": texture(e["texture"])}, {"Emissive": first("EmissiveLighting", 1)})


def main():
    files = [a for a in sys.argv[1:] if a.endswith(".json")]
    masters = {k: master(f"M_NW_P_{k.title().replace('_', '')}", k) for k in ("sprite", "sprite_add", "mesh", "mesh_lit", "decal")}
    for src in files:
        fx = json.load(open(src))
        info = fx["assets"].get("material_info", {})
        mats = {}
        for e in fx["emitters"]:
            mats[e["name"]] = material_for(e, info, masters)
            if e["kind"] == "mesh" and e.get("geometry"):
                e["mesh_asset"] = mesh(e["geometry"])
        spec = {"emitters": [recipe(e, mats) for e in fx["emitters"]],
                "system_constants": {"SystemState.Loop Duration": 1000.0}}
        name = f"NS_{fx['library']}_{fx['effect']}"
        out = os.path.splitext(src)[0]
        json.dump(spec, open(out + ".recipe.json", "w"), indent=1)
        report = unreal.NWNiagaraTools.build_effect(json.dumps(spec), f"{P_FX}/{name}")
        open(out + ".build.txt", "w").write(report)
        unreal.log(f"FX: {name}: {report.count('não')} warnings")


main()
