"""Single catalog: merges textures, materials and meshes repeated across layers, cities and gatherables.

    UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/catalog.py [--dry-run]"

Each import (region layer, city, gatherable) brings its own copies of what already exists in another: the same game
texture, the same material, the same rock. Here each group of equals keeps one original in /Game/NW/Catalogo
(Texturas, Materiais, Malhas), references move to it and the copies are deleted.

- Same texture: same asset name (the name is the .dds path in the game) and same compression settings.
- Same material: same parent and same parameters, with the textures already swapped for the catalog ones.
- Same mesh: same geometry in the glb (tools/catalog_meshes.py → extract/ue/catalogo_malhas.json) and same
  materials in the slots. Scenery only (Region, Cities); gatherable meshes stay in place (ANWGatherField
  loads them by path).
- Catalog rule: what is in it only references the catalog or /Game/NW/Materials. Deleting a layer's folder
  to reimport it does not break another.
- References swapped by hand in the material instances, mesh slots and map components. A copy is deleted
  only with no hard reference from outside; otherwise it goes through the engine's consolidate.

--dry-run: only counts groups and the space freed, without changing anything. Run again after importing a new region.
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
SKIP = ("/Terrain/", "/Region/Water/")  # terrain and water belong to a single map
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
    unreal.log_warning(f"CATALOG {msg} [{time.time() - t0:.0f}s]")


def pkg(obj_or_path):
    p = obj_or_path if isinstance(obj_or_path, str) else obj_or_path.get_path_name()
    return p.split(".")[0]


def disk_size(path):
    f = os.path.join(CONTENT_DIR, path[len("/Game/"):] + ".uasset")
    return os.path.getsize(f) if os.path.exists(f) else 0


def listed(cls):
    """Paths (package) of the class's assets within the scope, without loading them."""
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
    """Group original: the one already in the catalog; otherwise the highest rank; ties broken by path."""
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


# ---------------------------------------------------------------- textures
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
log(f"textures: {len(tex_paths)}, repeated names {sum(1 for v in by_name.values() if len(v) > 1)}, "
    f"copies {len(T)} ({sum(map(disk_size, T)) / 1e9:.2f} GB)")

# ---------------------------------------------------------------- materials
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
    except Exception as e:  # without the function: parent + overrides already tell the glTF cases apart
        parts.append(f"w:?{type(e).__name__}")
    return "|".join(parts)


mat_keys = {p: material_key(m) for p, m in mats.items()}
M, mat_groups = groups_to_remap(mat_keys)
log(f"materials: {len(mat_paths)}, copies {len(M)} ({sum(map(disk_size, M)) / 1e9:.2f} GB)")

# ---------------------------------------------------------------- meshes
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
log(f"meshes: {len(mesh_paths)} in the scenery, matched {len(mesh_keys)}, different extent {len(bad)} {bad[:3]}, "
    f"copies {len(S)} ({sum(map(disk_size, S)) / 1e9:.2f} GB)")

if DRY:
    log("dry-run: nothing changed")
    raise SystemExit(0)

# ---------------------------------------------------------------- reference swap
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
log(f"material instances with swapped texture: {len(changed)}")

n = 0
for p in listed("StaticMesh"):
    sm = eal.load_asset(p)
    for i, s in enumerate(sm.get_editor_property("static_materials")):
        if s.material_interface and pkg(s.material_interface) in M:
            sm.set_material(i, eal.load_asset(M[pkg(s.material_interface)]))
            changed.add(p)
            n += 1
log(f"mesh slots with swapped material: {n}")
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
            raise RuntimeError(f"could not save {mp}")
    log(f"{mp}: {meshes} components with swapped mesh, {materials} with swapped material")
unreal.SystemLibrary.collect_garbage()

# ---------------------------------------------------------------- delete copies
reg.scan_paths_synchronous(["/Game/NW"], True)
hard = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
deleted = consolidated = 0
for remap in (S, M, T):  # top down: referencers go first
    dups = set(remap)
    for p in sorted(remap):
        refs = [str(r) for r in reg.get_referencers(p, hard) or []]
        refs = [r for r in refs if r not in dups and r != p]
        if refs:
            eal.consolidate_assets(eal.load_asset(remap[p]), [eal.load_asset(p)])
            consolidated += 1
            if consolidated <= 10:
                log(f"consolidate {p} (references {refs[:3]})")
        elif eal.delete_asset(p):
            deleted += 1
log(f"copies deleted {deleted}, consolidated {consolidated}")

# ---------------------------------------------------------------- move the originals to the catalog
reg.scan_paths_synchronous(["/Game/NW"], True)
move = {p for by in (tex_groups, mat_groups, mesh_groups) for ps in by.values() if len(ps) > 1 for p in ps
        if not p.startswith(CAT + "/") and eal.does_asset_exist(p)}
# Closure: what goes to the catalog takes along what it references (mesh → materials → textures).
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
for dst, h in tags:  # the geometry goes along: the next run matches new meshes against these
    sm = eal.load_asset(dst)
    eal.set_metadata_tag(sm, "NWGeometria", h)
    eal.save_asset(dst, only_if_is_dirty=False)
log(f"moved to the catalog: {len(renames)}")

reg.scan_paths_synchronous(["/Game/NW"], True)
redirs = [eal.load_asset(str(a.package_name)) for a in reg.get_assets_by_path("/Game/NW", recursive=True)
          if str(a.asset_class_path.asset_name) == "ObjectRedirector"]
if redirs:
    tools.fixup_referencers(redirs)
for root in SCOPE[:3]:
    for d in sorted(eal.list_assets(root, recursive=True, include_folder=True), key=len, reverse=True):
        if d.endswith("/") and not eal.list_assets(d, recursive=True):
            eal.delete_directory(d)
log(f"redirectors fixed up: {len(redirs)}; done")
