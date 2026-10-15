#!/usr/bin/env python3
"""Efeito de partículas do jogo (biblioteca CryEngine libs/particles/*.xml) → JSON para o UNWNiagaraTools::BuildEffect.

    python3 tools/make_particles.py extract/fx_study/libs/particles/wfxo_icegaunt_tempest.xml Blizzard_AOE

Saída: extract/fx/<Biblioteca>.<Efeito>.json com os emissores ativos do efeito (o raiz e os filhos, em ordem), cada
um com o renderizador (sprite, malha ou decalque), os números e as curvas pela idade da partícula, e a lista de
texturas, malhas e materiais que o efeito usa (docs/tech/particles.md).

Formato dos valores do CryEngine (ParticleParams): "base,Random=r,ParticleAge=(t=..,v=..;t=..,v=..)". Chave sem t é
t=0, sem v é v=0. Vetores "x=..,y=..,z=..", cores "(x=..,y=..,z=..)". Curvas EmitterStrength e as inclinações (d, s,
flags) ficam de fora por enquanto.
"""
import json
import os
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "extract", "fx")


def split_top(s, sep=","):
    """Divide pelos separadores fora de parênteses."""
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == sep and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return [x.strip() for x in out]


def num(s, default=0.0):
    try:
        return float(s)
    except (TypeError, ValueError):
        return default


def vec(s):
    """'x=1,y=2' ou '(x=1,y=2,z=3)' → [x, y, z] (faltando = 0)."""
    s = s.strip()
    if s.startswith("(") and s.endswith(")"):
        s = s[1:-1]
    d = {}
    for part in split_top(s):
        if "=" in part:
            k, v = part.split("=", 1)
            d[k.strip().lower()] = num(v)
    return [d.get("x", 0.0), d.get("y", 0.0), d.get("z", 0.0)]


def curve(s, is_vec=False):
    """'(t=0.5,v=1;t=1)' → [[t, v], ...] (v vetor nas cores)."""
    s = s.strip()
    if s.startswith("(") and s.endswith(")"):
        s = s[1:-1]
    keys = []
    for k in split_top(s, ";"):
        t, v = 0.0, ([0.0, 0.0, 0.0] if is_vec else 0.0)
        for part in split_top(k):
            if "=" not in part:
                continue
            name, val = part.split("=", 1)
            if name == "t":
                t = num(val)
            elif name == "v":
                v = vec(val) if is_vec else num(val)
        keys.append([t, v])
    return keys


def value(s, is_vec=False):
    """Valor de um parâmetro: base, acaso, curva pela idade (e o resto como texto)."""
    out = {}
    parts = split_top(s)
    first = parts[0] if parts else ""
    rest = parts
    if first and ("=" not in first or first.startswith("(")):
        out["base"] = vec(first) if is_vec else (num(first) if first.replace(".", "", 1).replace("-", "", 1).isdigit() else first)
        rest = parts[1:]
    for p in rest:
        if "=" not in p:
            continue
        k, v = p.split("=", 1)
        if k == "Random":
            out["random"] = num(v)
        elif k == "ParticleAge":
            out["age"] = curve(v, is_vec)
        elif k in ("EmitterStrength",):
            out["emitter_strength"] = v
        else:
            out[k] = v
    return out


VECTOR_PARAMS = {"Color"}
SKIP = {"CameraNonFacingFade"}


def emitter(node, path):
    p = node.find("Params")
    a = dict(p.attrib) if p is not None else {}
    params = {k: value(v, k in VECTOR_PARAMS) for k, v in a.items() if k not in SKIP}
    facing = a.get("Facing", "Camera")
    kind = "decal" if facing == "Decal" else "mesh" if a.get("Geometry") else "sprite"
    return {"name": path, "kind": kind, "enabled": a.get("Enabled", "true") != "false", "gpu": a.get("EmitterType") == "GPU",
            "texture": a.get("Texture"), "material": a.get("Material"), "geometry": a.get("Geometry"),
            "blend": a.get("BlendType", "AlphaBased"), "facing": facing, "params": params, "raw": a}


def find(root, dotted):
    node, path = root, []
    for name in dotted.split("."):
        nxt = None
        kids = node.find("Childs") if node is not root else node
        for c in (kids if kids is not None else []):
            if c.tag == "Particles" and c.get("Name") == name:
                nxt = c
                break
        if nxt is None:
            sys.exit(f"efeito não encontrado: {dotted}")
        node = nxt
        path.append(name)
    return node


def walk(node, path, out):
    out.append(emitter(node, path))
    childs = node.find("Childs")
    for c in (childs if childs is not None else []):
        if c.tag == "Particles":
            walk(c, path + "." + c.get("Name"), out)


def build(lib_path, effect, src):
    root = ET.parse(lib_path).getroot()
    lib = root.get("Name")
    emitters = []
    walk(find(root, effect), effect, emitters)
    active = [e for e in emitters if e["enabled"]]
    assets = {"textures": sorted({e["texture"] for e in active if e["texture"]}),
              "materials": sorted({e["material"] for e in active if e["material"]}),
              "meshes": sorted({e["geometry"] for e in active if e["geometry"]})}
    # Materiais (.mtl extraídos em extract/fx/src): shader e texturas por mapa.
    assets["material_info"] = {}
    for m in assets["materials"]:
        f = os.path.join(src, m.lower() + ".mtl")
        if not os.path.exists(f):
            continue
        mr = ET.parse(f).getroot()
        mat = mr if mr.get("Shader") else (mr.find(".//Material") if mr.find(".//Material") is not None else mr)
        maps = {t.get("Map"): t.get("File") for t in mat.iter("Texture") if t.get("Map") and t.get("File")}
        pub = mat.find("PublicParams")
        assets["material_info"][m] = {"shader": mat.get("Shader"), "maps": maps, "gen": mat.get("StringGenMask", ""),
                                      "params": dict(pub.attrib) if pub is not None else {}}
    tex = set(assets["textures"])
    for info in assets["material_info"].values():
        tex |= set(info["maps"].values())
    assets["all_textures"] = sorted(t for t in tex if t)
    os.makedirs(OUT, exist_ok=True)
    dst = os.path.join(OUT, f"{lib}.{effect}.json")
    json.dump({"library": lib, "effect": effect, "emitters": active, "assets": assets}, open(dst, "w"), indent=1)
    print(f"{lib}.{effect}: {len(active)} emissores ativos de {len(emitters)} -> {os.path.relpath(dst, ROOT)}")
    return assets


def nwbt(*args, sub=1):
    """nwbt pela fila de trabalho pesado (docs/workflow.md), com o jogo de NW_GAME."""
    import subprocess
    game = os.environ.get("NW_GAME", "/mnt/jogos/Steam/steamapps/common/New World")
    subprocess.run([os.path.join(ROOT, "tools", "pesado.sh"), os.path.join(ROOT, "bin", "nwbt"), *args[:sub], "-g", game, *args[sub:]],
                   check=True, capture_output=True)


def regex(paths, ext):
    import re
    return "^(" + "|".join(re.escape(os.path.splitext(p.lower())[0]) + r"\." + ext for p in paths) + ")$"


def main():
    """make_particles.py [--extract] <biblioteca.xml> <Efeito> [<Efeito>...] (vários efeitos da mesma biblioteca)."""
    args = sys.argv[1:]
    extract = "--extract" in args
    args = [a for a in args if a != "--extract"]
    lib_path, effects = args[0], args[1:]
    src = os.path.join(OUT, "src")
    found = [build(lib_path, e, src) for e in effects]
    if not extract:
        return
    # 1) materiais; 2) refaz (texturas dos materiais); 3) texturas em png; 4) malhas em glb.
    mats = sorted({m for a in found for m in a["materials"]} - {m for m in []})
    if mats:
        nwbt("unpack", "-o", src, "-w", "2", "-r", "2", "-e", regex(mats, "mtl"))
    found = [build(lib_path, e, src) for e in effects]
    tex = sorted({t for a in found for t in a["all_textures"]})
    missing = [t for t in tex if not os.path.exists(os.path.join(src, os.path.splitext(t.lower())[0] + ".png"))]
    if missing:
        nwbt("unpack", "-o", src, "-w", "2", "-r", "2", "--x-dds", "png", "-e", regex(missing, "dds"))
    meshes = sorted({m for a in found for m in a["meshes"]})
    todo = [m for m in meshes if not os.path.exists(os.path.join(OUT, "glb", m.lower() + ".glb"))]
    if todo:
        nwbt("models", "files", "-o", os.path.join(OUT, "glb"), "--binary", "--embed", "--ior", "1.5", *[m.lower() for m in todo], sub=2)
    print(f"extraído: {len(mats)} materiais, {len(missing)} texturas novas, {len(todo)} malhas novas")


if __name__ == "__main__":
    main()
