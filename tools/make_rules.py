#!/usr/bin/env python3
"""Reverse-engineering facts (re-local/valores/<area>.json) → unreal/NWPort/Data/regras.json, read by
FNWRules. Only those with a numeric value; the rules (null value) go into the code with the id cited in a comment.
Without re-local/, keeps the regras.json that came in the asset zips (docs/tech/asset-sync.md) or, without it, writes
an empty one and the C++ uses the defaults.
To check what is still to be applied: python3 tools/re_check.py.

Each file: {"valores": {"<area>.<name>": {"valor", "unidade", "prova" (C/R/H), "endereco", "doc", "o_que"}}}.
The id starts with the file's area; the same id in two files is an error.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VALUES = os.path.join(ROOT, "re-local", "valores")


def load_values():
    out, owner = {}, {}
    if not os.path.isdir(VALUES):
        return out
    for name in sorted(os.listdir(VALUES)):
        if not name.endswith(".json"):
            continue
        for k, v in json.load(open(os.path.join(VALUES, name)))["valores"].items():
            if k in owner:
                raise SystemExit(f"value {k} in {owner[k]} and {name}")
            if not k.startswith(name[:-5] + "."):
                raise SystemExit(f"value {k} outside the area of file {name}")
            owner[k] = name
            out[k] = v
    return out


def main():
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "regras.json")
    if not os.path.isdir(VALUES) and os.path.exists(path):
        print(f"no re-local/valores: kept {os.path.relpath(path, ROOT)}")
        return
    out = {k: v["valor"] for k, v in load_values().items() if isinstance(v.get("valor"), (int, float))}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(out, open(path, "w"), indent=1, sort_keys=True)
    print(f"{len(out)} values in {os.path.relpath(path, ROOT)}")


if __name__ == "__main__":
    main()
