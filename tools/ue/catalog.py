"""Catálogo único: junta texturas, materiais e malhas repetidos entre camadas, cidades e coletáveis.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/catalog.py [--dry-run]"

Cada import (camada da região, cidade, coletável) traz cópias próprias do que já existe em outro: a mesma textura do
jogo, o mesmo material, a mesma rocha. Aqui cada grupo de iguais fica com um original em /Game/NW/Catalogo
(Texturas, Materiais, Malhas), as referências passam para ele e as cópias são apagadas.

- Textura igual: mesmo nome de asset (o nome é o caminho do .dds no jogo) e mesmos ajustes de compressão.
- Material igual: mesmo pai e mesmos parâmetros, já com as texturas trocadas pelas do catálogo.
- Malha igual: mesma geometria no glb (tools/catalog_meshes.py → extract/ue/catalogo_malhas.json) e mesmos
  materiais nos slots. Só cenário (Region, Cities); as malhas dos coletáveis ficam no lugar (ANWGatherField
  carrega pelo caminho).
- Regra do catálogo: o que está nele só referencia o catálogo ou /Game/NW/Materials. Apagar a pasta de uma camada
  para reimportar não quebra outra.
- Referências trocadas à mão nas instâncias de material, nos slots das malhas e nos componentes dos mapas. A cópia
  só é apagada sem nenhuma referência forte de fora; senão vai pelo consolidate da engine.

--dry-run: só conta grupos e o espaço que sai, sem mudar nada. Rodar de novo depois de importar uma região nova.
"""
import collections
import hashlib
import json
import os
import re
import sys
import time

import unreal

DRY = "--dry-run" in sys.argv
ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
CONTENT_DIR = os.path.join(unreal.Paths.project_dir(), "Content")
CAT = "/Game/NW/Catalogo"
SCOPE = ["/Game/NW/Region", "/Game/NW/Cities", "/Game/NW/Gatherables", CAT]
SKIP = ("/Terrain/", "/Region/Water/")  # terreno e água são de um mapa só
MESH_SCOPE = ("/Game/NW/Region/", "/Game/NW/Cities/", CAT + "/")
MAPS = ["/Game/NW/Maps/L_SouthWindsward", "/Game/NW/Maps/L_QuedaEterna", "/Game/NW/Maps/L_Arena"]
MESH_NAME = re.compile(r"^(.+)_mesh_(\d+)$")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
tools = unreal.AssetToolsHelpers.get_asset_tools()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
t0 = time.time()


def log(msg):
    unreal.log_warning(f"CATALOGO {msg} [{time.time() - t0:.0f}s]")


def pkg(obj_or_path):
    p = obj_or_path if isinstance(obj_or_path, str) else obj_or_path.get_path_name()
    return p.split(".")[0]


def disk_size(path):
    f = os.path.join(CONTENT_DIR, path[len("/Game/"):] + ".uasset")
    return os.path.getsize(f) if os.path.exists(f) else 0


def listed(cls):
    """Caminhos (pacote) dos assets da classe dentro do escopo, sem carregar."""
    out = []
    for root in SCOPE:
        if not eal.does_directory_exist(root):
            continue
        for a in reg.get_assets_by_path(root, recursive=True):
            path = str(a.package_name)
            if str(a.asset_class_path.asset_name) == cls and not any(s in path + "/" for s in SKIP):
                out.append(path)
    return sorted(out)


def short(text):
    return hashlib.sha1(text.encode()).hexdigest()[:8]


def canon_path(remap, obj):
    if obj is None:
        return ""
    p = pkg(obj)
    return remap.get(p, p)


def pick(paths, rank=lambda p: 0):
    """Original do grupo: o que já está no catálogo; senão o de maior rank; empate pelo caminho."""
    inside = [p for p in paths if p.startswith(CAT + "/")]
    if inside:
        return sorted(inside)[0]
    return sorted(paths, key=lambda p: (-rank(p), p))[0]


def groups_to_remap(keys, rank=lambda p: 0):
    by = collections.defaultdict(list)
    for p, k in keys.items():
        by[k].append(p)
    remap = {}
    for k, ps in by.items():
        if len(ps) > 1:
            c = pick(ps, rank)
            remap.update({p: c for p in ps if p != c})
    return remap, by


# ---------------------------------------------------------------- texturas
def texture_key(t):
    parts = [t.get_name()]
    for prop in ("srgb", "compression_settings", "lod_group", "filter", "address_x", "address_y", "mip_gen_settings"):
        try:
            parts.append(str(t.get_editor_property(prop)))
        except Exception:
            parts.append("?")
    return "|".join(parts)


tex_paths = listed("Texture2D")
by_name = collections.defaultdict(list)
for p in tex_paths:
    by_name[p.rsplit("/", 1)[1]].append(p)
tex_keys, tex_max = {}, {}
for name, ps in by_name.items():
    if len(ps) == 1:
        continue
    for p in ps:
        t = eal.load_asset(p)
        tex_keys[p] = texture_key(t)
        m = t.get_editor_property("max_texture_size")
        tex_max[p] = m if m else 1 << 30
T, tex_groups = groups_to_remap(tex_keys, rank=lambda p: tex_max.get(p, 0))
log(f"texturas: {len(tex_paths)}, nomes repetidos {sum(1 for v in by_name.values() if len(v) > 1)}, "
    f"cópias {len(T)} ({sum(map(disk_size, T)) / 1e9:.2f} GB)")

# ---------------------------------------------------------------- materiais
mat_paths = listed("MaterialInstanceConstant")
mats = {p: eal.load_asset(p) for p in mat_paths}


def struct_text(s):
    try:
        return s.export_text()
    except Exception:
        return str(s)


def material_key(mi):
    parts = [canon_path({}, mi.get_editor_property("parent"))]
    for v in sorted(mi.get_editor_property("scalar_parameter_values"), key=lambda v: str(v.parameter_info.name)):
        parts.append(f"s:{v.parameter_info.name}={v.parameter_value:.6g}")
    for v in sorted(mi.get_editor_property("vector_parameter_values"), key=lambda v: str(v.parameter_info.name)):
        c = v.parameter_value
        parts.append(f"v:{v.parameter_info.name}={c.r:.5g},{c.g:.5g},{c.b:.5g},{c.a:.5g}")
    for v in sorted(mi.get_editor_property("texture_parameter_values"), key=lambda v: str(v.parameter_info.name)):
        parts.append(f"t:{v.parameter_info.name}={canon_path(T, v.parameter_value)}")
    parts.append("o:" + struct_text(mi.get_editor_property("base_property_overrides")))
    try:
        for n in mel.get_static_switch_parameter_names(mi):
            parts.append(f"w:{n}={mel.get_material_instance_static_switch_parameter_value(mi, n)}")
    except Exception as e:  # sem a função: o pai + overrides já separam os casos do glTF
        parts.append(f"w:?{type(e).__name__}")
    return "|".join(parts)


mat_keys = {p: material_key(m) for p, m in mats.items()}
M, mat_groups = groups_to_remap(mat_keys)
log(f"materiais: {len(mat_paths)}, cópias {len(M)} ({sum(map(disk_size, M)) / 1e9:.2f} GB)")

# ---------------------------------------------------------------- malhas
geo = json.load(open(os.path.join(ROOT, "extract/ue/catalogo_malhas.json")))
mesh_paths = [p for p in listed("StaticMesh") if p.startswith(MESH_SCOPE)]
mesh_keys, mesh_label, bad = {}, {}, []
for p in mesh_paths:
    name = p.rsplit("/", 1)[1]
    m_ = MESH_NAME.match(name)
    if p.startswith(CAT + "/"):
        sm = eal.load_asset(p)
        tag = eal.get_metadata_tag(sm, "NWGeometria")
        if not tag:
            continue
        h, label = tag, name
    elif m_ and m_.group(1) in geo and int(m_.group(2)) < len(geo[m_.group(1)]):
        h, ext, label = geo[m_.group(1)][int(m_.group(2))]
        sm = eal.load_asset(p)
        b = sm.get_bounding_box()
        size = sorted(abs(x) for x in (b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z))
        want = sorted(e * 100 for e in ext)
        if any(abs(a - w) > 1 + 0.01 * w for a, w in zip(size, want)):
            bad.append(p)
            continue
    else:
        continue
    slots = "|".join(canon_path(M, s.material_interface) for s in sm.get_editor_property("static_materials"))
    nanite = sm.get_editor_property("nanite_settings").enabled
    bs = sm.get_editor_property("body_setup")
    flag = str(bs.get_editor_property("collision_trace_flag")) if bs else ""
    mesh_keys[p] = f"{h}|{slots}|{nanite}|{flag}"
    mesh_label[p] = (h, label)
S, mesh_groups = groups_to_remap(mesh_keys)
log(f"malhas: {len(mesh_paths)} no cenário, casadas {len(mesh_keys)}, extensão diferente {len(bad)} {bad[:3]}, "
    f"cópias {len(S)} ({sum(map(disk_size, S)) / 1e9:.2f} GB)")

if DRY:
    log("dry-run: nada mudou")
    raise SystemExit(0)

# ---------------------------------------------------------------- troca das referências
changed = set()
for p, mi in mats.items():
    hit = False
    for v in mi.get_editor_property("texture_parameter_values"):
        if v.parameter_value and pkg(v.parameter_value) in T:
            mel.set_material_instance_texture_parameter_value(mi, v.parameter_info.name, eal.load_asset(T[pkg(v.parameter_value)]),
                                                               v.parameter_info.association)
            hit = True
    if hit:
        mel.update_material_instance(mi)
        changed.add(p)
log(f"instâncias de material com textura trocada: {len(changed)}")

n = 0
for p in listed("StaticMesh"):
    sm = eal.load_asset(p)
    for i, s in enumerate(sm.get_editor_property("static_materials")):
        if s.material_interface and pkg(s.material_interface) in M:
            sm.set_material(i, eal.load_asset(M[pkg(s.material_interface)]))
            changed.add(p)
            n += 1
log(f"slots de malha com material trocado: {n}")
for p in changed:
    eal.save_asset(p, only_if_is_dirty=False)

for mp in MAPS:
    if not eal.does_asset_exist(mp):
        continue
    les.load_level(mp)
    meshes = materials = 0
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            if sm and pkg(sm) in S:
                c.set_static_mesh(eal.load_asset(S[pkg(sm)]))
                meshes += 1
            ov = list(c.get_editor_property("override_materials"))
            hit = False
            for i, m in enumerate(ov):
                if m and pkg(m) in M:
                    ov[i] = eal.load_asset(M[pkg(m)])
                    hit = True
            if hit:
                c.set_editor_property("override_materials", ov)
                materials += 1
    if meshes or materials:
        if not les.save_current_level():
            raise RuntimeError(f"não consegui salvar {mp}")
    log(f"{mp}: {meshes} componentes com malha trocada, {materials} com material trocado")
unreal.SystemLibrary.collect_garbage()

# ---------------------------------------------------------------- apagar cópias
reg.scan_paths_synchronous(["/Game/NW"], True)
hard = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
deleted = consolidated = 0
for remap in (S, M, T):  # de cima para baixo: quem referencia sai antes
    dups = set(remap)
    for p in sorted(remap):
        refs = [str(r) for r in reg.get_referencers(p, hard) or []]
        refs = [r for r in refs if r not in dups and r != p]
        if refs:
            eal.consolidate_assets(eal.load_asset(remap[p]), [eal.load_asset(p)])
            consolidated += 1
            if consolidated <= 10:
                log(f"consolidate {p} (referências {refs[:3]})")
        elif eal.delete_asset(p):
            deleted += 1
log(f"cópias apagadas {deleted}, consolidadas {consolidated}")

# ---------------------------------------------------------------- mover os originais para o catálogo
reg.scan_paths_synchronous(["/Game/NW"], True)
move = {p for by in (tex_groups, mat_groups, mesh_groups) for ps in by.values() if len(ps) > 1 for p in ps
        if not p.startswith(CAT + "/") and eal.does_asset_exist(p)}
# Fecho: o que vai para o catálogo leva junto o que referencia (malha → materiais → texturas).
queue = list(move)
while queue:
    p = queue.pop()
    for d in reg.get_dependencies(p, hard) or []:
        d = str(d)
        if d.startswith(tuple(r + "/" for r in SCOPE[:3])) and not any(s in d + "/" for s in SKIP) and d not in move:
            if not d.startswith("/Game/NW/Gatherables/") or not MESH_NAME.match(d.rsplit("/", 1)[1]):
                move.add(d)
                queue.append(d)

used = set()


def target(p):
    a = eal.find_asset_data(p)
    cls = str(a.asset_class_path.asset_name)
    name = p.rsplit("/", 1)[1]
    if cls == "StaticMesh":
        h, label = mesh_label.get(p, ("", name))
        folder, name = "Malhas", f"SM_{label or 'malha'}_{h[:8] or short(p)}"
    elif cls == "Texture2D":
        folder = "Texturas"
    else:
        folder, name = "Materiais", f"{name}_{short(p)}"
    dst = f"{CAT}/{folder}/{name}"
    if dst in used or eal.does_asset_exist(dst):
        dst = f"{dst}_{short(p)}"
    used.add(dst)
    return dst, cls


renames, tags = [], []
for p in sorted(move):
    dst, cls = target(p)
    if cls == "StaticMesh" and p in mesh_label:
        tags.append((dst, mesh_label[p][0]))
    renames.append(unreal.AssetRenameData(asset=eal.load_asset(p), new_package_path=dst.rsplit("/", 1)[0],
                                          new_name=dst.rsplit("/", 1)[1]))
for i in range(0, len(renames), 200):
    tools.rename_assets(renames[i:i + 200])
for dst, h in tags:  # a geometria vai junto: a próxima rodada casa as malhas novas com estas
    sm = eal.load_asset(dst)
    eal.set_metadata_tag(sm, "NWGeometria", h)
    eal.save_asset(dst, only_if_is_dirty=False)
log(f"movidos para o catálogo: {len(renames)}")

reg.scan_paths_synchronous(["/Game/NW"], True)
redirs = [eal.load_asset(str(a.package_name)) for a in reg.get_assets_by_path("/Game/NW", recursive=True)
          if str(a.asset_class_path.asset_name) == "ObjectRedirector"]
if redirs:
    tools.fixup_referencers(redirs)
for root in SCOPE[:3]:
    for d in sorted(eal.list_assets(root, recursive=True, include_folder=True), key=len, reverse=True):
        if d.endswith("/") and not eal.list_assets(d, recursive=True):
            eal.delete_directory(d)
log(f"redirecionadores resolvidos: {len(redirs)}; pronto")
