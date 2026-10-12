"""Importa um conjunto gerado pelo build_set.py para o projeto Unreal aberto.

No Output Log do editor (Python):
    py "<repo>/tools/ue/import_nw.py" "<repo>/extract/ue/sword_shield/manifest.json"

Cria em /Game/NW/<conjunto>/:
    Mesh/SK_player_male (+ esqueleto e physics asset), Mesh/SM_sword, Mesh/SM_shield
    Textures/*, Materials/M_NW_SpecGloss + MI_<material>
    Anims/A_* (sequências; janelas de combate como curvas 0/1), Anims/BS_* (blend spaces), Anims/AM_* (montagens)
"""
import json
import math
import os
import re
import sys

import unreal

MANIFEST = next((a for a in sys.argv[1:] if a.endswith(".json")), os.environ.get("NW_MANIFEST", ""))
m = json.load(open(MANIFEST))
ROOT = f"/Game/NW/{m['name']}"
P_MESH, P_TEX, P_MAT, P_ANIM = (f"{ROOT}/{d}" for d in ("Mesh", "Textures", "Materials", "Anims"))

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
log = unreal.log

# O importador Interchange ignora o FbxImportUI; o clássico respeita esqueleto, root motion e taxa.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")


def run_import(filename, dest, options=None, name=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest
    if name:
        task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options is not None:
        task.options = options
    tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        unreal.log_error(f"falhou: {filename}")
    return paths


def get_or_create(name, folder, cls, factory):
    """create_asset devolve None se o asset já existe: nesse caso reaproveita (o script pode rodar de novo)."""
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def load(path):
    return eal.load_asset(path.split(".")[0] if "." in path else path)


# --anims-only: mantém malha, esqueleto, materiais e os ABPs; reimporta só animações, curvas, sons, blend spaces e montagens.
ANIMS_ONLY = "--anims-only" in sys.argv
if ANIMS_ONLY:
    sk = load(f"{P_MESH}/{m.get('mesh_name', 'SK_player_male')}")
    skeleton = sk.get_editor_property("skeleton")
    mel = unreal.MaterialEditingLibrary
    log(f"só animações; esqueleto existente: {skeleton.get_path_name()}")
else:
    # ---------- malha e armas ----------
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.import_animations = False
    ui.import_materials = True
    ui.import_textures = True
    ui.create_physics_asset = True
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
    sk_paths = run_import(m["fbx"]["skeletal_mesh"], P_MESH, ui)
    sk = next(load(p) for p in sk_paths if isinstance(load(p), unreal.SkeletalMesh))
    # Conjuntos que reaproveitam um esqueleto já importado (mesmo osso raiz) poderiam apontar para ele;
    # por ora cada conjunto tem o seu (o Lost usa damned_skel, diferente do jogador).
    skeleton = sk.get_editor_property("skeleton")
    # Reimportar a malha não atualiza a pose de referência do esqueleto já existente.
    tools_cls = getattr(unreal, "NWEditorTools", None)
    if tools_cls and tools_cls.update_skeleton_ref_pose_from_mesh(skeleton, sk):
        log("pose de referência do esqueleto atualizada pela malha")
    # O import salva só a malha; esqueleto e physics asset ficam em pacotes próprios e precisam ser salvos.
    eal.save_loaded_asset(skeleton)
    if sk.get_editor_property("physics_asset"):
        eal.save_loaded_asset(sk.get_editor_property("physics_asset"))
    log(f"SK: {sk.get_path_name()}  esqueleto: {skeleton.get_path_name()}")

    for key, fbx in m["fbx"]["weapons"].items():
        sui = unreal.FbxImportUI()
        sui.import_mesh = True
        sui.import_as_skeletal = False
        sui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        sui.import_materials = True
        sui.import_textures = True
        sui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        run_import(fbx, P_MESH, sui, f"SM_{key}")

    # ---------- materiais: specular/gloss do jogo -> metallic/roughness ----------
    mel = unreal.MaterialEditingLibrary


    def master_material(masked=False):
        # Mascarado: pelo/cabelo do jogo (cartões com recorte no alfa da textura de cor), dois lados.
        name = "M_NW_SpecGloss_Masked" if masked else "M_NW_SpecGloss"
        path = f"{P_MAT}/{name}"
        if eal.does_asset_exist(path):
            return load(path)
        mat = get_or_create(name, P_MAT, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("used_with_skeletal_mesh", True)
        if masked:
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
            mat.set_editor_property("two_sided", True)
            mat.set_editor_property("opacity_mask_clip_value", 0.4)
        def tex(name, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
            n.set_editor_property("parameter_name", name)
            n.set_editor_property("sampler_type", sampler)
            return n
        diff = tex("Diffuse", -900, -200)
        spec = tex("Spec", -900, 100)
        norm = tex("Normal", -900, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        # Amostrador de normal com a textura padrão de cor não compila (SM6): textura padrão de normal da engine.
        norm.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
        # metallic ~ intensidade do specular (dielétricos ficam perto de 0.04)
        mx = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -650, 100)
        mx.set_editor_property("r", True)
        mel.connect_material_expressions(spec, "", mx, "")
        sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -500, 100)
        sub.set_editor_property("const_b", 0.2)
        mel.connect_material_expressions(mx, "", sub, "A")
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -380, 100)
        mul.set_editor_property("const_b", 3.0)
        mel.connect_material_expressions(sub, "", mul, "A")
        sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -260, 100)
        mel.connect_material_expressions(mul, "", sat, "")
        lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -260, -150)
        mel.connect_material_expressions(diff, "RGB", lerp, "A")
        mel.connect_material_expressions(spec, "RGB", lerp, "B")
        mel.connect_material_expressions(sat, "", lerp, "Alpha")
        rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -260, 300)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.5)
        mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(norm, "RGB", unreal.MaterialProperty.MP_NORMAL)
        if masked:
            mel.connect_material_property(diff, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        return mat


    def import_texture(png, normal=False):
        name = "T_" + os.path.splitext(os.path.basename(png))[0]
        path = f"{P_TEX}/{name}"
        if not eal.does_asset_exist(path):
            run_import(png, P_TEX, None, name)
        tex = load(path)
        if normal and tex:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("flip_green_channel", True)  # PNG veio em convenção OpenGL
            eal.save_loaded_asset(tex)
        return tex


    master = master_material()
    master_masked = master_material(masked=True)
    instances = {}
    for mat_name, chans in m.get("materials", {}).items():
        if "Base Color" not in chans:
            continue
        mi = get_or_create(f"MI_{mat_name}", P_MAT, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master_masked if chans.get("Alpha") else master)
        mel.set_material_instance_texture_parameter_value(mi, "Diffuse", import_texture(chans["Base Color"]))
        if chans.get("Specular IOR Level"):
            mel.set_material_instance_texture_parameter_value(mi, "Spec", import_texture(chans["Specular IOR Level"]))
        if chans.get("Normal"):
            mel.set_material_instance_texture_parameter_value(mi, "Normal", import_texture(chans["Normal"], normal=True))
        eal.save_loaded_asset(mi)
        instances[mat_name] = mi


    def slot_key(name):
        # Slot da malha x material do glb: sem diferença de caixa nem sufixo do Blender (.001) / do FBX (_skin).
        n = re.sub(r"\.\d+$", "", str(name)).lower()
        return re.sub(r"_skin\d*$", "", n)

    by_key = {slot_key(k): v for k, v in instances.items()}

    def assign_materials(mesh):
        mats = mesh.get_editor_property("materials")
        changed = False
        # Iterar o array dá cópias dos slots: altera a cópia e devolve pelo índice (senão nada muda).
        for i in range(len(mats)):
            slot = mats[i]
            name = str(slot.get_editor_property("material_slot_name"))
            mi = instances.get(name) or by_key.get(slot_key(name))
            if mi is None:
                unreal.log_warning(f"material: slot {name} sem instância ({sorted(instances)})")
                continue
            slot.set_editor_property("material_interface", mi)
            mats[i] = slot
            changed = True
        if changed:
            mesh.set_editor_property("materials", mats)
            eal.save_loaded_asset(mesh)


    assign_materials(sk)

# ---------- animações ----------
anim_ui = unreal.FbxImportUI()
anim_ui.import_mesh = False
anim_ui.import_animations = True
anim_ui.import_materials = False
anim_ui.import_textures = False
anim_ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
anim_ui.skeleton = skeleton
ad = anim_ui.anim_sequence_import_data
ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ad.set_editor_property("use_default_sample_rate", False)
ad.set_editor_property("custom_sample_rate", m["fps"])
ad.set_editor_property("import_meshes_in_bone_hierarchy", False)
ad.set_editor_property("remove_redundant_keys", False)

loop_assets = {m["composites"][k]["asset"] for k in m["loops"].values()}
for bs in m["blendspaces"].values():
    loop_assets |= {m["composites"][s["anim"]]["asset"] for s in bs["samples"]}
# Montagens tocadas no lugar (pulo, queda, pouso): quem move é o CharacterMovement, como nos loops.
for name in m.get("inplace_montages", []):
    loop_assets |= {m["composites"][k]["asset"] for k in m["montages"].get(name, [])}

seqs = {}
for key, comp in m["composites"].items():
    paths = run_import(comp["fbx"], P_ANIM, anim_ui, comp["asset"])
    seq = next((load(p) for p in paths if isinstance(load(p), unreal.AnimSequence)), None)
    if seq is None:
        continue
    # Montagens usam o deslocamento do clipe; loops/blend spaces ficam no lugar (quem move é o CharacterMovement).
    is_loop = comp["asset"] in loop_assets
    seq.set_editor_property("enable_root_motion", not is_loop)
    seq.set_editor_property("force_root_lock", is_loop)
    eal.save_loaded_asset(seq)
    seqs[key] = seq
log(f"animações: {len(seqs)}")

# ---------- janelas de combate como curvas ----------
CURVE_TYPES = {"CAGE-Named", "CAGE-TransitionBuffered", "CAGE-TransitionInstant", "WeaponEffects", "Homing", "ActivateGrit",
               "Invulnerability", "CAGE-Damage", "HitStun", "CritWindow"}


def curve_name(w):
    if w["type"] == "WeaponEffects":
        return "WeaponTrail"
    if w["type"] == "CAGE-Damage":
        return "DamageWindow"
    if w["type"] in ("Homing", "ActivateGrit", "Invulnerability", "HitStun", "CritWindow"):
        return w["type"]
    prefix = "Buffer_" if w["type"] == "CAGE-TransitionBuffered" else ""
    return prefix + w["name"]


alib = unreal.AnimationLibrary
for montage, sections in m.get("windows", {}).items():
    for sec_idx, sec in enumerate(sections):
        comp_key = m["montages"][montage][min(sec_idx, len(m["montages"][montage]) - 1)]
        seq = seqs.get(comp_key)
        if seq is None:
            continue
        length = seq.get_play_length()
        curves = {}
        for w in sec["windows"]:
            if w["type"] not in CURVE_TYPES:
                continue
            curves.setdefault(curve_name(w), []).append((w["start"], w["end"] if w["end"] is not None else length))
        for cname, spans in curves.items():
            if alib.does_curve_exist(seq, cname, unreal.RawCurveTrackTypes.RCT_FLOAT):
                alib.remove_curve(seq, cname, False)
            alib.add_curve(seq, cname, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
            times, values = [0.0], [0.0]
            eps = 1.0 / m["fps"] / 4
            for a, b in sorted(spans):
                a, b = max(0.0, min(a, length)), max(0.0, min(b, length))
                times += [max(a - eps, 0.0), a, b, min(b + eps, length)]
                values += [0.0, 1.0, 1.0, 0.0]
            # tempos repetidos quebram a curva: mantém o último valor de cada tempo
            pts = {}
            for t, v in zip(times, values):
                pts[round(t, 5)] = v
            ts = sorted(pts)
            alib.add_float_curve_keys(seq, cname, ts, [pts[t] for t in ts])
        eal.save_loaded_asset(seq)
log("janelas de combate gravadas como curvas")

# ---------- blend spaces ----------
AXES = {
    # No jogo ângulo/giro positivos são para a esquerda; no Unreal (CalculateDirection, yaw) para a direita.
    "TravelAngle": ("Direção", -180.0, 180.0, lambda v: -math.degrees(v)),
    "MoveSpeed": ("Velocidade", 0.0, 400.0, lambda v: v * 100.0),
    "TurnSpeed": ("Giro", -2.5, 2.5, lambda v: -v),
}
for bs_name, bs in m["blendspaces"].items():
    dims = [d["name"] for d in bs["dimensions"]]
    # eixo X = direção; eixo Y = a outra dimensão; amostras com inclinação diferente de 0 ficam de fora
    # Quadrúpedes (javali, lobo) não andam de lado: giro × velocidade (X = giro, Y = velocidade).
    x_i = dims.index("TravelAngle") if "TravelAngle" in dims else dims.index("TurnSpeed")
    y_i = next(i for i, d in enumerate(dims) if d in ("MoveSpeed", "TurnSpeed") and i != x_i)
    slope_i = dims.index("TravelSlope") if "TravelSlope" in dims else None
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property("target_skeleton", skeleton)
    asset = get_or_create(bs_name, P_ANIM, unreal.BlendSpace, factory)
    params = []
    for i in (x_i, y_i):
        label, lo, hi, _ = AXES[dims[i]]
        if dims[i] == "MoveSpeed":
            # Corrida de bicho passa dos 4 m/s (lobo até 9,2): o eixo vai até o máximo do blend space do jogo.
            hi = max(hi, bs["dimensions"][i].get("max", 0.0) * 100.0 + 50.0)
        p = unreal.BlendParameter()
        p.set_editor_property("display_name", label)
        p.set_editor_property("min", lo)
        p.set_editor_property("max", hi)
        p.set_editor_property("grid_num", 8 if dims[i] == "TravelAngle" else 4)
        params.append(p)
    params.append(unreal.BlendParameter())
    asset.set_editor_property("blend_parameters", params)
    samples, seen = [], set()
    for s in bs["samples"]:
        if slope_i is not None and abs(s["params"][slope_i]) > 1e-3:
            continue
        params = list(s["params"])
        explicit = s.get("explicit", [True] * len(params))
        if dims[y_i] == "MoveSpeed" and not explicit[y_i]:
            params[y_i] = m["composites"][s["anim"]].get("clip_speed", 0.0) * s.get("rate", 1.0)
        x = AXES[dims[x_i]][3](params[x_i])
        y = AXES[dims[y_i]][3](params[y_i])
        if (round(x, 1), round(y, 1)) in seen or s["anim"] not in seqs:
            continue
        seen.add((round(x, 1), round(y, 1)))
        smp = unreal.BlendSample()
        smp.set_editor_property("animation", seqs[s["anim"]])
        smp.set_editor_property("sample_value", unreal.Vector(x, y, 0))
        smp.set_editor_property("rate_scale", s.get("rate", 1.0))
        samples.append(smp)
    # Blend space sem amostra parada (ex.: caminhada do Lost): o loop Idle entra em velocidade 0, em cada direção.
    if dims[y_i] == "MoveSpeed" and samples and min(sm.get_editor_property("sample_value").y for sm in samples) > 0:
        idle_key = m["loops"].get("Idle")
        if idle_key in seqs:
            for x in sorted({round(sm.get_editor_property("sample_value").x, 1) for sm in samples}):
                smp = unreal.BlendSample()
                smp.set_editor_property("animation", seqs[idle_key])
                smp.set_editor_property("sample_value", unreal.Vector(x, 0, 0))
                smp.set_editor_property("rate_scale", 1.0)
                samples.append(smp)
    asset.set_editor_property("sample_data", samples)
    grid = unreal.NWEditorTools.rebuild_blend_space(asset) if hasattr(unreal, "NWEditorTools") else -1
    eal.save_loaded_asset(asset)
    log(f"{bs_name}: {len(samples)} amostras, triangulação {grid}")

# ---------- montagens ----------
for montage, sections in m["montages"].items():
    for i, comp_key in enumerate(sections):
        seq = seqs.get(comp_key)
        if seq is None:
            continue
        name = montage if len(sections) == 1 else f"{montage}_{i + 1}"
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", skeleton)
        factory.set_editor_property("source_animation", seq)
        am = get_or_create(name, P_ANIM, unreal.AnimMontage, factory)
        # Montagens que seguram o último quadro até outra entrar (ex.: carga do pesado): sem auto blend out.
        am.set_editor_property("enable_auto_blend_out", montage not in m.get("hold_montages", []))
        eal.save_loaded_asset(am, only_if_is_dirty=False)
log("montagens criadas")

# ---------- sons (tools/make_sound_map.py) como notifies NW Som Aleatório ----------
sounds_path = os.path.join(os.path.dirname(MANIFEST), "sounds.json")
notify_cls = unreal.load_class(None, "/Script/NWPort.NWAnimNotify_RandomSound")
if os.path.exists(sounds_path) and notify_cls:
    sound_map = json.load(open(sounds_path))
    audio_root = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(MANIFEST))), "audio")
    imported = {}

    def import_sound(ogg):
        if ogg in imported:
            return imported[ogg]
        rel = os.path.relpath(os.path.dirname(ogg), audio_root).replace(os.sep, "/")
        dest = f"{ROOT}/Sounds/{rel}"
        name = "S_" + os.path.splitext(os.path.basename(ogg))[0]
        path = f"{dest}/{name}"
        if not eal.does_asset_exist(path):
            run_import(ogg, dest, None, name)
        imported[ogg] = load(path)
        return imported[ogg]

    by_asset = {c["asset"]: k for k, c in m["composites"].items()}
    count = 0
    for asset, events in sound_map.items():
        seq = seqs.get(by_asset.get(asset))
        if seq is None:
            continue
        if "Som" in [str(t) for t in alib.get_animation_notify_track_names(seq)]:
            alib.remove_animation_notify_track(seq, "Som")
        alib.add_animation_notify_track(seq, "Som")
        for ev in events:
            snds = [x for x in (import_sound(f) for f in ev["files"]) if x]
            if not snds:
                continue
            n = alib.add_animation_notify_event(seq, "Som", ev["time"], notify_cls)
            if n is None:
                unreal.log_warning(f"notify não criado: {asset} {ev['label']}")
                continue
            n.set_editor_property("sounds", snds)
            n.set_editor_property("label", ev["label"])
            if ev["bone"]:
                n.set_editor_property("bone", ev["bone"])
            count += 1
        eal.save_loaded_asset(seq)
    log(f"sons: {count} notifies, {len(imported)} arquivos")
else:
    unreal.log_warning("sons: pulei (sounds.json ausente ou classe NWAnimNotify_RandomSound não compilada)")

# ---------- DataTables (precisam do módulo C++ NWPort compilado) ----------
data_dir = os.path.join(os.path.dirname(MANIFEST), "data")
for csv_name in sorted(os.listdir(data_dir)) if os.path.isdir(data_dir) else []:
    table = os.path.splitext(csv_name)[0]
    struct_path = "/Script/NWPort.NWDamageRow" if "Damage" in table else "/Script/NWPort.NWActionRow"
    csv_path = os.path.join(data_dir, csv_name)
    struct = unreal.load_object(None, struct_path)
    if not os.path.exists(csv_path) or struct is None:
        unreal.log_warning(f"{table}: pulei (csv ausente ou struct {struct_path} não carregada; compile o projeto e rode tools/make_datatables.py)")
        continue
    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_row_struct", struct)
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    factory.set_editor_property("automated_import_settings", settings)
    task = unreal.AssetImportTask()
    task.filename = csv_path
    task.destination_path = "/Game/NW/Data"
    task.destination_name = table
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = factory
    tools.import_asset_tasks([task])
    log(f"{table}: {list(task.imported_object_paths)}")
# ABPs feitos à mão no editor: recompilar contra o esqueleto/animações novos.
for abp_path in eal.list_assets(ROOT, recursive=False):
    obj = eal.load_asset(abp_path)
    if isinstance(obj, unreal.AnimBlueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        log(f"recompilado: {obj.get_name()}")
eal.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
eal.save_directory("/Game/NW/Data", only_if_is_dirty=False, recursive=True)
log(f"pronto: {ROOT}")
