#!/usr/bin/env python3
"""Converts a city's NPC appearances (.cdf from Data/World/<city>_npcs.json, tools/make_npcs.py) to FBX.

    tools/linux/heavy.sh python3 tools/convert_npcs.py [city]   (default queda_eterna; needs `source tools/linux/env.sh`)

One nwbt call for all .cdf (the pak index loads once) and one Blender run to export.
Output in extract/ue/npcs/<SK_name>/ (Everfall) or extract/ue/npcs_<city>/<SK_name>/ (other cities): <SK_name>.fbx,
textures/, materials.json. Import: tools/ue/import_npcs.py with that folder. The nwbt glb cache is shared
(extract/ue/npcs/glb), and an appearance already exported for another city is copied instead of exported again.
The animations come from the set tools/sets/npc_<city>.json (build_set.py), on the same npc_skel skeleton.
"""
import glob
import json
import os
import shutil
import sys
import nwplatform
import nwprocess
import outputs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
nwplatform.configure()
GAME = nwplatform.game_directory()
CITY = sys.argv[1] if len(sys.argv) > 1 else "queda_eterna"
NPCS = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", f"{CITY}_npcs.json")
BASE = os.path.join(ROOT, "extract", "ue", "npcs")
OUT = BASE if CITY == "queda_eterna" else BASE + "_" + CITY
CDFS = os.path.join(ROOT, "extract", "npcs")  # extracted .cdf (attachment Flags); see docs/game/city.md


def main():
    npcs = json.load(open(NPCS))["npcs"]
    by_mesh = {}
    for n in npcs:
        by_mesh.setdefault(n["mesh"], n["cdf"])
    glb_dir = os.path.join(BASE, "glb")
    # Appearance already exported for another city: same .cdf, same FBX and textures.
    for m in by_mesh:
        if os.path.exists(os.path.join(OUT, m, m + ".fbx")):
            continue
        for done in glob.glob(os.path.join(BASE + "*", m, m + ".fbx")):
            shutil.copytree(os.path.dirname(done), os.path.join(OUT, m), dirs_exist_ok=True)
            break
    todo = [c for c in by_mesh.values() if not os.path.exists(os.path.join(glb_dir, c + ".glb"))]
    if todo:
        # List in a file: many .cdf exceed the Windows command-line limit.
        os.makedirs(OUT, exist_ok=True)
        arglist = os.path.join(OUT, "nwbt_files.txt")
        with open(arglist, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(todo) + "\n")
        outputs.require_nwbt()  # "@list" only in the nwbt from patches/nwbt.patch
        nwprocess.run([nwplatform.executable("nwbt"), "models", "files", "-g", GAME, "-o", glb_dir, "--binary", "--embed",
                       "--ior", "1.5", *nwplatform.nwbt_workers(), "@" + arglist], cwd=ROOT, min_memory=10 * nwprocess.GIB)
    jobs = [{"glb": os.path.join(glb_dir, c + ".glb"), "mesh": m, "cdf": os.path.join(CDFS, c)}
             for m, c in sorted(by_mesh.items())
             if not os.path.exists(os.path.join(OUT, m, m + ".fbx"))]
    missing = [x["glb"] for x in jobs if not os.path.exists(x["glb"])]
    if missing:
        raise SystemExit(f"nwbt did not produce: {missing}")
    if jobs:
        path = os.path.join(OUT, "lista.json")  # name kept: existing extractions have it (windows/check_migration.py)
        json.dump(jobs, open(path, "w"), indent=1)
        since = outputs.now()
        nwprocess.run(outputs.blender(os.path.join(ROOT, "tools", "export_npcs.py"), path),
                      cwd=ROOT, min_memory=10 * nwprocess.GIB)
        outputs.require([os.path.join(OUT, x["mesh"], x["mesh"] + ".fbx") for x in jobs], since, "Blender (export_npcs.py)")
    print(f"ok: {len(by_mesh)} appearances in {OUT}")


if __name__ == "__main__":
    with nwprocess.heavy_lock():
        main()
