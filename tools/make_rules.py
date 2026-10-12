#!/usr/bin/env python3
"""Fatos da engenharia reversa (docs/analise-re/valores.json) → unreal/NWPort/Data/regras.json, lido pelo FNWRules.
Só os que têm valor numérico; as regras (valor null) entram no código com o id citado em comentário.
Conferir o que falta aplicar: python3 tools/re_check.py."""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    src = json.load(open(os.path.join(ROOT, "docs", "analise-re", "valores.json")))["valores"]
    out = {k: v["valor"] for k, v in src.items() if isinstance(v.get("valor"), (int, float))}
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "regras.json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(out, open(path, "w"), indent=1, sort_keys=True)
    print(f"{len(out)} valores em {os.path.relpath(path, ROOT)}")


if __name__ == "__main__":
    main()
