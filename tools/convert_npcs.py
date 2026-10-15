#!/usr/bin/env python3
"""Converts the Everfall NPC appearances (.cdf from Data/World/queda_eterna_npcs.json) to FBX.

    tools/linux/pesado.sh python3 tools/convert_npcs.py        (needs `source tools/linux/env.sh`: nwbt and blender)

One nwbt call for all .cdf (the pak index loads once) and one Blender run to export.
Output in extract/ue/npcs/<SK_name>/: <SK_name>.fbx, textures/, materials.json. Import: tools/ue/import_npcs.py.
The animations come from the set tools/sets/npc_queda_eterna.json (build_set.py), on the same npc_skel skeleton.
"""
import json
import os
import nwplatform
import nwprocess
import saidas

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
nwplatform.configure()
GAME = nwplatform.game_directory()
NPCS = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "queda_eterna_npcs.json")
OUT = os.path.join(ROOT, "extract", "ue", "npcs")
CDFS = os.path.join(ROOT, "extract", "npcs")  # extracted .cdf (attachment Flags); see docs/game/city.md


def main():
    npcs = json.load(open(NPCS))["npcs"]
    by_mesh = {}
    for n in npcs:
        by_mesh.setdefault(n["mesh"], n["cdf"])
    glb_dir = os.path.join(OUT, "glb")
    todo = [c for c in by_mesh.values() if not os.path.exists(os.path.join(glb_dir, c + ".glb"))]
    if todo:
        # List in a file: many .cdf exceed the Windows command-line limit.
        os.makedirs(OUT, exist_ok=True)
        arglist = os.path.join(OUT, "nwbt_files.txt")
        with open(arglist, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(todo) + "\n")
        saidas.require_nwbt()  # "@list" only in the nwbt from patches/nwbt.patch
        nwprocess.run([nwplatform.executable("nwbt"), "models", "files", "-g", GAME, "-o", glb_dir, "--binary", "--embed",
                       "--ior", "1.5", *nwplatform.nwbt_workers(), "@" + arglist], cwd=ROOT, min_memory=10 * nwprocess.GIB)
    lista = [{"glb": os.path.join(glb_dir, c + ".glb"), "mesh": m, "cdf": os.path.join(CDFS, c)}
             for m, c in sorted(by_mesh.items())
             if not os.path.exists(os.path.join(OUT, m, m + ".fbx"))]
    missing = [x["glb"] for x in lista if not os.path.exists(x["glb"])]
    if missing:
        raise SystemExit(f"nwbt did not produce: {missing}")
    if lista:
        path = os.path.join(OUT, "lista.json")
        json.dump(lista, open(path, "w"), indent=1)
        since = saidas.now()
        nwprocess.run(saidas.blender(os.path.join(ROOT, "tools", "export_npcs.py"), path),
                      cwd=ROOT, min_memory=10 * nwprocess.GIB)
        saidas.require([os.path.join(OUT, x["mesh"], x["mesh"] + ".fbx") for x in lista], since, "Blender (export_npcs.py)")
    print(f"ok: {len(by_mesh)} appearances in {OUT}")


if __name__ == "__main__":
    with nwprocess.heavy_lock():
        main()
