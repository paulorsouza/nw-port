"""Importa as aparências dos NPCs (tools/convert_npcs.py) e põe o campo de NPCs no mapa da cidade.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_npcs.py <extract/ue/npcs> <Conjunto> <Mapa>"
    ex.: ... import_npcs.py /…/extract/ue/npcs NpcQuedaEterna L_QuedaEterna
    Só materiais, no editor aberto (fora do Play):
    python3 tools/ue/remote.py -c "import sys; sys.argv=['', '<npcs>', 'NpcQuedaEterna', 'L_QuedaEterna', '--materials-only']; exec(open('tools/ue/import_npcs.py').read())"

Antes: import_nw.py do conjunto das animações (tools/sets/npc_queda_eterna.json), que cria o esqueleto npc_skel e
os materiais mestre em /Game/NW/<Conjunto>. Cada NPC vai para /Game/NW/<Conjunto>/NPCs/SK_<nome>/ no mesmo
esqueleto (as animações servem para todos). O mapa ganha um ANWNpcField (removido e recriado a cada execução).
"""
import glob
import json
import os
import re
import sys

import unreal

SRC, SET, MAP_NAME = sys.argv[1:4]
ROOT = f"/Game/NW/{SET}"
MAP = f"/Game/NW/Maps/{MAP_NAME}"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
log = unreal.log
# O importador Interchange ignora o FbxImportUI; o clássico respeita o esqueleto escolhido.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")


def run_import(filename, dest, options=None, name=None):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path = filename, dest
    if name:
        task.destination_name = name
    task.automated = task.replace_existing = task.save = True
    if options is not None:
        task.options = options
    tools.import_asset_tasks([task])
    return list(task.imported_object_paths)


def load(path):
    return eal.load_asset(path.split(".")[0] if "." in path else path)


base = load(f"{ROOT}/Mesh/SK_{SET}")
if not base:
    raise RuntimeError(f"falta {ROOT}/Mesh/SK_{SET}: rode antes o import_nw.py do conjunto")
skeleton = base.get_editor_property("skeleton")
master = load(f"{ROOT}/Materials/M_NW_SpecGloss")
master_masked = load(f"{ROOT}/Materials/M_NW_SpecGloss_Masked")


def import_texture(png, folder, normal=False):
    # Nome de asset só com letras, números e _ (há texturas do jogo com espaço, ex. "male_marauderfaction _chest").
    name = "T_" + re.sub(r"[^A-Za-z0-9_]", "_", os.path.splitext(os.path.basename(png))[0])
    path = f"{folder}/{name}"
    if not eal.does_asset_exist(path):
        run_import(png, folder, None, name)
    tex = load(path)
    if normal and tex:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # PNG em convenção OpenGL (como import_nw.py)
        eal.save_loaded_asset(tex)
    return tex


def slot_key(name):
    # Material do glb "body_mat.001" vira slot "body_mat_001" no FBX; cada um tem textura própria (não juntar).
    n = str(name).lower().replace(".", "_")
    return re.sub(r"_skin\d*$", "", n)


MATERIALS_ONLY = "--materials-only" in sys.argv  # só refaz instâncias e slots (dá para rodar no editor aberto)
fbxs = sorted(glob.glob(os.path.join(SRC, "SK_*", "SK_*.fbx")))

# 1. Malhas no esqueleto do conjunto.
if not MATERIALS_ONLY:
    for fbx in fbxs:
        mesh_name = os.path.splitext(os.path.basename(fbx))[0]
        ui = unreal.FbxImportUI()
        ui.import_mesh = True
        ui.import_as_skeletal = True
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        ui.skeleton = skeleton
        ui.import_animations = False
        ui.import_materials = False
        ui.import_textures = False
        ui.create_physics_asset = False
        ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
        ui.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
        if not run_import(fbx, f"{ROOT}/NPCs/{mesh_name}", ui, mesh_name):
            unreal.log_error(f"NPC falhou: {fbx}")

# 2. Materiais, numa passada depois de todas as malhas: atribuir logo após o import de cada uma perdia os slots
# (a malha ainda compilava em segundo plano e voltava sem material; NPCs cinzas em 2026-10-12).
done = missing = 0
for fbx in fbxs:
    mesh_name = os.path.splitext(os.path.basename(fbx))[0]
    folder = f"{ROOT}/NPCs/{mesh_name}"
    sk_path = f"{folder}/{mesh_name}"
    sk = load(sk_path)
    if not isinstance(sk, unreal.SkeletalMesh):
        unreal.log_error(f"NPC sem malha: {sk_path}")
        continue
    chans_by_mat = json.load(open(os.path.join(os.path.dirname(fbx), "materials.json")))
    instances = {}
    for mat_name, chans in chans_by_mat.items():
        if "Base Color" not in chans:
            continue
        asset = "MI_" + re.sub(r"[^A-Za-z0-9_]", "_", mat_name)  # nome de asset sem ponto nem espaço
        mi_path = f"{folder}/{asset}"
        mi = load(mi_path) if eal.does_asset_exist(mi_path) else tools.create_asset(
            asset, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master_masked if chans.get("Alpha") else master)
        mel.set_material_instance_texture_parameter_value(mi, "Diffuse", import_texture(chans["Base Color"], folder))
        if chans.get("Specular IOR Level"):
            mel.set_material_instance_texture_parameter_value(mi, "Spec", import_texture(chans["Specular IOR Level"], folder))
        if chans.get("Normal"):
            mel.set_material_instance_texture_parameter_value(mi, "Normal", import_texture(chans["Normal"], folder, True))
        eal.save_loaded_asset(mi)
        instances[slot_key(mat_name)] = mi
    sk.modify()
    mats = sk.get_editor_property("materials")
    for i in range(len(mats)):
        slot = mats[i]
        mi = instances.get(slot_key(slot.get_editor_property("material_slot_name")))
        if mi:
            slot.set_editor_property("material_interface", mi)
            mats[i] = slot
        else:
            missing += 1
            unreal.log_warning(f"NPC {mesh_name}: slot {slot.get_editor_property('material_slot_name')} sem material")
    sk.set_editor_property("materials", mats)
    eal.save_asset(sk_path, only_if_is_dirty=False)
    # Confere o que ficou gravado.
    got = sum(1 for m in load(sk_path).get_editor_property("materials") if m.get_editor_property("material_interface"))
    if got != len(mats):
        unreal.log_warning(f"NPC {mesh_name}: {got}/{len(mats)} slots com material depois de salvar")
    done += 1
unreal.log_warning(f"NPC materiais em {done} malhas; slots sem material: {missing}")
# Malha com osso que o esqueleto base não tem (ex. c_jaw_BND do Ou Shen): a Unreal acrescenta o osso ao esqueleto
# ao carregar a malha (passada 2), só na memória. Salvar o esqueleto depois de tudo, senão o editor avisa
# "missing bones" ao abrir e o AssetCheck acusa esqueleto incompatível.
nw_tools = getattr(unreal, "NWEditorTools", None)
if nw_tools:
    # O Ou Shen tem rosto montado diferente (c_jaw_BND e 6 ossos de olho/sobrancelha presos na Head, não em
    # bind_upper_face_a): a checagem de cadeia de pais recusa a fusão. As animações paradas não mexem no rosto.
    unreal.SystemLibrary.execute_console_command(None, "a.Skeleton.AllowIncompatibleSkeletalMeshMerge 1")
    for fbx in fbxs:
        mesh_name = os.path.splitext(os.path.basename(fbx))[0]
        sk = load(f"{ROOT}/NPCs/{mesh_name}/{mesh_name}")
        if sk and nw_tools.merge_mesh_bones_to_skeleton(skeleton, sk):
            log(f"NPC ossos de {mesh_name} acrescentados ao esqueleto")
    unreal.SystemLibrary.execute_console_command(None, "a.Skeleton.AllowIncompatibleSkeletalMeshMerge 0")
eal.save_loaded_asset(skeleton, False)

# 3. Campo de NPCs no mapa.
if not MATERIALS_ONLY:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les.load_level(MAP)
    eas.destroy_actors([a for a in eas.get_all_level_actors() if a.get_class().get_name() == "NWNpcField"])
    field = eas.spawn_actor_from_class(unreal.load_class(None, "/Script/NWPort.NWNpcField"), unreal.Vector(0, 0, 0))
    field.set_actor_label("NPCs")
    if not les.save_current_level():
        raise RuntimeError("não consegui salvar o mapa")
    log(f"NPC campo no mapa {MAP}")
