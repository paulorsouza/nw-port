#!/usr/bin/env python3
"""Fatos da engenharia reversa (re-local/valores/<área>.json) → unreal/NWPort/Data/regras.json, lido pelo
FNWRules. Só os que têm valor numérico; as regras (valor null) entram no código com o id citado em comentário.
Sem re-local/, mantém o regras.json que veio nos zips de assets (docs/sincronizar-assets.md) ou, sem ele, grava
um vazio e o C++ usa os padrões.
Conferir o que falta aplicar: python3 tools/re_check.py.

Cada arquivo: {"valores": {"<área>.<nome>": {"valor", "unidade", "prova" (C/R/H), "endereco", "doc", "o_que"}}}.
O id começa pela área do arquivo; o mesmo id em dois arquivos é erro.
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
                raise SystemExit(f"valor {k} em {owner[k]} e {name}")
            if not k.startswith(name[:-5] + "."):
                raise SystemExit(f"valor {k} fora da área do arquivo {name}")
            owner[k] = name
            out[k] = v
    return out


def main():
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "regras.json")
    if not os.path.isdir(VALUES) and os.path.exists(path):
        print(f"sem re-local/valores: mantido {os.path.relpath(path, ROOT)}")
        return
    out = {k: v["valor"] for k, v in load_values().items() if isinstance(v.get("valor"), (int, float))}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(out, open(path, "w"), indent=1, sort_keys=True)
    print(f"{len(out)} valores em {os.path.relpath(path, ROOT)}")


if __name__ == "__main__":
    main()
