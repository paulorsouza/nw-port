"""Materiais prontos para Nanite: liga "Used with Nanite" e "Used with Instanced Static Meshes" nos materiais e desliga o Nanite nas malhas que usam
material translúcido/aditivo (Nanite só desenha opaco e masked). Instâncias cujo pai está fora de /Game (o glTF do
Interchange usa /InterchangeAssets/gltf/M_Default) passam a usar uma cópia do pai em /Game/NW/Materials/Interchange,
com a flag: a instalação da engine não é alterada.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/fix_nanite_materials.py /Game/NW/Cities /Game/NW/Gatherables ..."

Sem a flag, o jogo fora do editor (-game, pacote) troca o material pelo padrão: a cidade aparecia cinza/escura
(2026-10-11). Rodar depois de importar malhas novas com Nanite (tools/ue/nanite_on.py, import_gatherables.py).
"""
import sys
import time

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
t0 = time.time()
OK_BLEND = (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED)

assets = []
for root in sys.argv[1:]:
    assets += eal.list_assets(root, recursive=True)

COPIES = "/Game/NW/Materials/Interchange"
copies = {}


def project_parent(parent):
    """Cópia em /Game de um pai que mora num plugin/engine; instância intermediária leva a cadeia junto."""
    path = parent.get_path_name().split(".")[0]
    if path.startswith("/Game/"):
        return parent
    if path not in copies:
        dst = f"{COPIES}/{path.split('/')[-1]}"
        copy = eal.load_asset(dst) if eal.does_asset_exist(dst) else eal.duplicate_asset(path, dst)
        if isinstance(copy, unreal.MaterialInstanceConstant):
            mel.set_material_instance_parent(copy, project_parent(parent.get_editor_property("parent")))
        else:
            copy.set_editor_property("used_with_nanite", True)
            copy.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(copy)
        eal.save_asset(dst, only_if_is_dirty=False)
        copies[path] = copy
    return copies[path]


reparented = 0
for p in assets:
    a = eal.load_asset(p)
    if isinstance(a, unreal.MaterialInstanceConstant) and a.get_editor_property("parent"):
        parent = a.get_editor_property("parent")
        if not parent.get_path_name().startswith("/Game/"):
            mel.set_material_instance_parent(a, project_parent(parent))
            eal.save_asset(p, only_if_is_dirty=False)
            reparented += 1

flagged, already = 0, 0
for p in assets:
    a = eal.load_asset(p)
    if isinstance(a, unreal.Material):
        # Nanite e malhas instanciadas (a paisagem de coletáveis usa UInstancedStaticMeshComponent).
        if a.get_editor_property("used_with_nanite") and a.get_editor_property("used_with_instanced_static_meshes"):
            already += 1
            continue
        a.set_editor_property("used_with_nanite", True)
        a.set_editor_property("used_with_instanced_static_meshes", True)
        mel.recompile_material(a)
        eal.save_asset(p, only_if_is_dirty=False)
        flagged += 1


def base_blend(mi):
    """Blend mode efetivo: a primeira sobreposição na cadeia de instâncias, senão o do material base."""
    m = mi
    while isinstance(m, unreal.MaterialInstance):
        ov = m.get_editor_property("base_property_overrides")
        if ov and ov.get_editor_property("override_blend_mode"):
            return ov.get_editor_property("blend_mode")
        m = m.get_editor_property("parent")
    return m.get_editor_property("blend_mode") if m else unreal.BlendMode.BLEND_OPAQUE


nanite_off = []
for p in assets:
    a = eal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh):
        continue
    ns = a.get_editor_property("nanite_settings")
    if not ns.enabled:
        continue
    mats = [s.material_interface for s in a.get_editor_property("static_materials") if s.material_interface]
    if any(base_blend(m) not in OK_BLEND for m in mats):
        ns.enabled = False
        a.set_editor_property("nanite_settings", ns)
        eal.save_asset(p, only_if_is_dirty=False)
        nanite_off.append(p.split("/")[-1].split(".")[0])

unreal.log_warning(f"NANITE instâncias com pai copiado={reparented} ({list(copies)}) materiais marcados={flagged} "
           f"já tinham={already} malhas sem Nanite (translúcidas)={len(nanite_off)} "
           f"{nanite_off[:15]} em {time.time() - t0:.0f}s")
