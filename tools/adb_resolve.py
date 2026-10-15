#!/usr/bin/env python3
"""Resolve Mannequin (.adb) actions for a set of active tags.

For each fragment (Idle, Sprint, Attack_Primary...) picks the most specific
variant whose tags are all active, as Mannequin does, and resolves the layers'
animation names to game files (.caf, .bspace, .comb).

usage: adb_resolve.py <paths.txt> <adb>... --tags 1H_Melee,Shield_OH,Offhand [--json saida.json]
"""
import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET

ANIM_ROOT = "animations/gameplay/character/player/male/"
EXTS = (".caf", ".bspace", ".comb")


def index_animations(paths_file):
    """short name (lowercase, no extension) -> path in the pak, male player animations only."""
    idx = {}
    with open(paths_file) as f:
        for line in f:
            p = line.strip()
            if p.startswith(ANIM_ROOT) and p.endswith(EXTS):
                name = os.path.splitext(os.path.basename(p))[0].lower()
                # .caf takes precedence over a .bspace/.comb with the same name
                if name not in idx or p.endswith(".caf"):
                    idx[name] = p
    return idx


def fragments(adb_files):
    """List of (fragment, tags, layers, source). Layer = list of animations [{name, speed?}]."""
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
    """Per fragment, the variant with the most tags among those that fit the active tags.
    On a tie the first one wins, in the order of the files given."""
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
        print(f"\nno file ({len(missing)}): {', '.join(sorted(missing)[:30])}", file=sys.stderr)
    if a.json:
        with open(a.json, "w") as fp:
            json.dump(best, fp, indent=1)


if __name__ == "__main__":
    main()
