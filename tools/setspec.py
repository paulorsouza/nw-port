"""Reads a set (tools/sets/<x>.json) merging the parts listed in "parts" (paths relative to tools/sets/).

The player set is split by area (docs/workflow.md): tools/sets/sword_shield.json (character, jump,
fall, death) + tools/sets/player/{weapons,weapons_ice-gauntlet,gathering,mount}.json. Merge rules: dicts merge key
by key, lists add without repeats, plain values must be equal. The same key in two parts with different values is an error
(two areas defining the same montage).
"""
import json
import os

SETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sets")


def _merge(dst, src, where):
    for k, v in src.items():
        if k == "_comment":
            continue
        if k not in dst:
            dst[k] = v
        elif isinstance(dst[k], dict) and isinstance(v, dict):
            for kk, vv in v.items():
                if kk in dst[k] and dst[k][kk] != vv:
                    raise SystemExit(f"{where}: '{k}.{kk}' already defined in another part with another value")
                dst[k][kk] = vv
        elif isinstance(dst[k], list) and isinstance(v, list):
            dst[k] += [x for x in v if x not in dst[k]]
        elif dst[k] != v:
            raise SystemExit(f"{where}: '{k}' already defined in another part with another value")


def load(path):
    spec = json.load(open(path))
    for part in spec.pop("parts", []):
        p = os.path.join(SETS, part)
        _merge(spec, json.load(open(p)), os.path.relpath(p, os.path.dirname(SETS)))
    return spec
