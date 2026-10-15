#!/usr/bin/env python3
"""Converts the equipment appearances (tools/make_appearances.py plan) to FBX for Unreal.

    python3 tools/heavy.py python3 tools/convert_appearances.py [--dry-run]

Before: build_set.py of tools/sets/sword_shield.json (its player_male.cdf.glb is the skeleton of every skinned part)
and python3 tools/make_appearances.py (writes extract/ue/appearances/plan.json).

1. nwbt, in batches of 8 with -w 2 and --skip (resumable), cwd = repo root (.nwbt/ stays there):
   - armor: `nwbt models appearances --ids <IDs>` -> glb/armorappearances/<id>-skin1.glb (Skin1 + Material1 + the
     cloth of AppearanceCDF, on the male skeleton). The filter is a substring match: the female variants
     (F_<ID>) come along and are ignored.
   - weapons: `nwbt models files <MeshOverride .cdf>` -> glb/<cdf path>.glb (as the set's weapons).
2. One Blender run (tools/export_appearances.py) -> extract/ue/appearances/{Body,Armor,Weapons}/<name>/.
3. make_appearances.py again: Data/Items/appearances.json without the entries that failed.
Import: tools/ue/import_appearances.py (tools/steps/appearances.ue.sh).
"""
import argparse
import json
import os
import subprocess
import sys

import nwplatform
import nwprocess
import outputs

ROOT = nwplatform.ROOT
WORK = ROOT / "extract" / "ue" / "appearances"
GLB = WORK / "glb"
BATCH = 8


def nwbt(args, dry_run):
    command = [nwplatform.executable("nwbt"), "models", *args[:1], "-g", nwplatform.game_directory(),
               "-o", GLB, "--binary", "--embed", "--ior", "1.5", "-w", "2", "--skip", *args[1:]]
    if not dry_run:
        nwprocess.require_resources([WORK], memory=16000 * 1024 ** 2, disk=10 * nwprocess.GIB)
    nwprocess.run(command, cwd=ROOT, log=WORK / "nwbt.log", min_memory=10 * nwprocess.GIB, dry_run=dry_run)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    nwplatform.configure()
    subprocess.run([sys.executable, str(ROOT / "tools" / "make_appearances.py")], cwd=ROOT, check=True)
    plan = json.load(open(WORK / "plan.json"))
    if not os.path.exists(plan["player_glb"]):
        sys.exit(f"missing {plan['player_glb']}: run build_set.py tools/sets/sword_shield.json first")
    if not args.dry_run:
        outputs.require_nwbt()
        GLB.mkdir(parents=True, exist_ok=True)

    armor_glb = {app: GLB / "armorappearances" / f"{app.lower()}-skin1.glb" for app in plan["armor"]}
    todo = [app for app, glb in armor_glb.items() if not glb.exists()]
    for i in range(0, len(todo), BATCH):
        nwbt(["appearances", "--ids", ",".join(todo[i:i + BATCH])], args.dry_run)
    weapon_glb = {name: GLB / (w["cdf"] + ".glb") for name, w in plan["weapons"].items()}
    todo = sorted({plan["weapons"][n]["cdf"] for n, glb in weapon_glb.items() if not glb.exists()})
    for i in range(0, len(todo), BATCH):
        nwbt(["files", *todo[i:i + BATCH]], args.dry_run)

    jobs = []
    for b in plan["body"]:
        jobs.append({"kind": "body", "name": b["name"], "part": b["part"], "player_glb": plan["player_glb"],
                     "out": str(WORK / "Body" / b["name"])})
    for app, a in plan["armor"].items():
        jobs.append({"kind": "armor", "name": a["name"], "glb": str(armor_glb[app]), "player_glb": plan["player_glb"],
                     "out": str(WORK / "Armor" / a["name"])})
    for name in plan["weapons"]:
        jobs.append({"kind": "weapon", "name": name, "glb": str(weapon_glb[name]), "out": str(WORK / "Weapons" / name)})
    missing = [j["glb"] for j in jobs if j.get("glb") and not os.path.exists(j["glb"])]
    if missing and not args.dry_run:
        print(f"warning: nwbt did not produce {len(missing)} glb:\n  " + "\n  ".join(missing))
        jobs = [j for j in jobs if not j.get("glb") or os.path.exists(j["glb"])]
    # Only what is not exported yet (delete a folder to export it again).
    jobs = [j for j in jobs if not os.path.exists(os.path.join(j["out"], j["name"] + ".fbx"))]
    if jobs:
        path = WORK / "jobs.json"
        json.dump(jobs, open(path, "w"), indent=1)
        since = outputs.now()
        nwprocess.run(outputs.blender(ROOT / "tools" / "export_appearances.py", path), cwd=ROOT,
                      log=WORK / "blender.log", min_memory=10 * nwprocess.GIB, dry_run=args.dry_run)
        if not args.dry_run:
            late = outputs.stale([os.path.join(j["out"], j["name"] + ".fbx") for j in jobs], since)
            if late:
                print(f"warning: Blender did not write {len(late)} FBX (see {WORK / 'blender.log'}):\n  "
                      + "\n  ".join(late))
    if not args.dry_run:
        subprocess.run([sys.executable, str(ROOT / "tools" / "make_appearances.py")], cwd=ROOT, check=True)


if __name__ == "__main__":
    with nwprocess.heavy_lock():
        main()
