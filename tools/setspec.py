"""Lê um conjunto (tools/sets/<x>.json) juntando as partes listadas em "parts" (caminhos relativos a tools/sets/).

O conjunto do jogador é dividido por esteira (docs/workflow.md): tools/sets/sword_shield.json (personagem, pulo,
queda, morte) + tools/sets/player/{armas,coleta,montaria}.json. Junta assim: dicionário funde chave a chave, lista
soma sem repetir, valor simples tem que ser igual. A mesma chave em duas partes com valores diferentes é erro
(duas esteiras definindo a mesma montagem).
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
                    raise SystemExit(f"{where}: '{k}.{kk}' já definido em outra parte com outro valor")
                dst[k][kk] = vv
        elif isinstance(dst[k], list) and isinstance(v, list):
            dst[k] += [x for x in v if x not in dst[k]]
        elif dst[k] != v:
            raise SystemExit(f"{where}: '{k}' já definido em outra parte com outro valor")


def load(path):
    spec = json.load(open(path))
    for part in spec.pop("parts", []):
        p = os.path.join(SETS, part)
        _merge(spec, json.load(open(p)), os.path.relpath(p, os.path.dirname(SETS)))
    return spec
