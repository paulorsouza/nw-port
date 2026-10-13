#!/usr/bin/env python3
"""Converte as aparências dos NPCs de Queda Eterna (.cdf de Data/World/queda_eterna_npcs.json) para FBX.

    tools/pesado.sh python3 tools/convert_npcs.py        (precisa de `source tools/env.sh`: nwbt e blender)

Uma chamada do nwbt para todos os .cdf (o índice dos paks carrega uma vez) e um Blender para exportar.
Saída em extract/ue/npcs/<SK_nome>/: <SK_nome>.fbx, textures/, materials.json. Import: tools/ue/import_npcs.py.
As animações vêm do conjunto tools/sets/npc_queda_eterna.json (build_set.py), no mesmo esqueleto npc_skel.
"""
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get("GAME", os.environ.get("NW_GAME", "/mnt/jogos/Steam/steamapps/common/New World"))
NPCS = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "queda_eterna_npcs.json")
OUT = os.path.join(ROOT, "extract", "ue", "npcs")
CDFS = os.path.join(ROOT, "extract", "npcs")  # .cdf extraídos (Flags dos encaixes); ver docs/sistemas.md


def main():
    npcs = json.load(open(NPCS))["npcs"]
    by_mesh = {}
    for n in npcs:
        by_mesh.setdefault(n["mesh"], n["cdf"])
    glb_dir = os.path.join(OUT, "glb")
    todo = [c for c in by_mesh.values() if not os.path.exists(os.path.join(glb_dir, c + ".glb"))]
    if todo:
        subprocess.run(["nice", "nwbt", "models", "files", "-g", GAME, "-o", glb_dir, "--binary", "--embed",
                        "--ior", "1.5", *todo], check=True, cwd=ROOT)
    lista = [{"glb": os.path.join(glb_dir, c + ".glb"), "mesh": m, "cdf": os.path.join(CDFS, c)}
             for m, c in sorted(by_mesh.items())
             if not os.path.exists(os.path.join(OUT, m, m + ".fbx"))]
    missing = [x["glb"] for x in lista if not os.path.exists(x["glb"])]
    if missing:
        raise SystemExit(f"nwbt não gerou: {missing}")
    if lista:
        path = os.path.join(OUT, "lista.json")
        json.dump(lista, open(path, "w"), indent=1)
        subprocess.run(["nice", "blender", "-b", "-P", os.path.join(ROOT, "tools", "export_npcs.py"), "--", path],
                       check=True)
    print(f"ok: {len(by_mesh)} aparências em {OUT}")


if __name__ == "__main__":
    main()
