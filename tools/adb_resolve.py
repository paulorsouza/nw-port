#!/usr/bin/env python3
"""Resolve as ações do Mannequin (.adb) para um conjunto de tags ativas.

Para cada fragmento (Idle, Sprint, Attack_Primary...) escolhe a variante mais
específica cujas tags estão todas ativas, como o Mannequin faz, e resolve os
nomes de animação das camadas para arquivos do jogo (.caf, .bspace, .comb).

uso: adb_resolve.py <paths.txt> <adb>... --tags 1H_Melee,Shield_OH,Offhand [--json saida.json]
"""
import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET

ANIM_ROOT = "animations/gameplay/character/player/male/"
EXTS = (".caf", ".bspace", ".comb")


def index_animations(paths_file):
    """nome curto (minúsculo, sem extensão) -> caminho no pak, só animações do jogador masculino."""
    idx = {}
    with open(paths_file) as f:
        for line in f:
            p = line.strip()
            if p.startswith(ANIM_ROOT) and p.endswith(EXTS):
                name = os.path.splitext(os.path.basename(p))[0].lower()
                # .caf tem precedência sobre .bspace/.comb de mesmo nome
                if name not in idx or p.endswith(".caf"):
                    idx[name] = p
    return idx


def fragments(adb_files):
    """Lista (fragmento, tags, camadas, origem). Camada = lista de animações [{name, speed?}]."""
    out = []
    for adb in adb_files:
        root = ET.parse(adb).getroot()
        for frag in root.find("FragmentList"):
            for f in frag.findall("Fragment"):
                tags = {t for t in f.get("Tags", "").split("+") if t}
                layers = []
                for layer in f.findall("AnimLayer"):
                    anims = []
                    for a in layer.findall("Animation"):
                        if a.get("name"):
                            anims.append({k: v for k, v in a.attrib.items()})
                    if anims:
                        layers.append(anims)
                out.append((frag.tag, tags, layers, os.path.basename(adb)))
    return out


def resolve(frags, active):
    """Por fragmento, a variante com mais tags dentre as que cabem nas tags ativas.
    Em empate vale a primeira, na ordem dos arquivos passados."""
    best = {}
    for name, tags, layers, src in frags:
        if not tags <= active or not layers:
            continue
        cur = best.get(name)
        if cur is None or len(tags) > len(cur["tags"]):
            best[name] = {"tags": sorted(tags), "layers": layers, "adb": src}
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths")
    ap.add_argument("adb", nargs="+")
    ap.add_argument("--tags", required=True)
    ap.add_argument("--json")
    a = ap.parse_args()

    idx = index_animations(a.paths)
    active = set(a.tags.split(","))
    best = resolve(fragments(a.adb), active)

    missing = set()
    for frag in best.values():
        for layer in frag["layers"]:
            for anim in layer:
                anim["file"] = idx.get(anim["name"].lower())
                if not anim["file"]:
                    missing.add(anim["name"])

    for name in sorted(best):
        f = best[name]
        desc = " + ".join("/".join(os.path.basename(x["file"] or "?" + x["name"]) for x in layer) for layer in f["layers"])
        print(f"{name:34} [{'+'.join(f['tags'])}] {desc}")
    if missing:
        print(f"\nsem arquivo ({len(missing)}): {', '.join(sorted(missing)[:30])}", file=sys.stderr)
    if a.json:
        with open(a.json, "w") as fp:
            json.dump(best, fp, indent=1)


if __name__ == "__main__":
    main()
