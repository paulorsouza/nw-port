#!/usr/bin/env python3
"""Confere o que a engenharia reversa (docs/analise-re) já entrou no port.

    python3 tools/re_check.py

1. Valores (docs/analise-re/valores/<área>.json): cada id é procurado no código do port (C++ em unreal/NWPort/Source,
   Python em tools/ e tools/ue/). Valor numérico entra pelo FNWRules (Data/regras.json); regra (valor null) entra
   citando o id em comentário onde é aplicada. Sem uso = falta aplicar.
2. Data/regras.json desatualizado em relação aos valores (rodar tools/make_rules.py).
4. Perguntas (docs/analise-re/perguntas/<esteira>-NNN.md) respondidas e ainda não aplicadas.
3. Colunas (docs/analise-re/colunas.json, tirado do executável por tools/re/colunas.py): colunas da damagetable que o
   cliente do jogo lê e o port não exporta (make_datatables.DAMAGE_COLS), e as que o port exporta e o cliente não lê
   (de servidor: o port, que é servidor e cliente, usa mesmo assim).
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
RE_DIR = os.path.join(ROOT, "docs", "analise-re")
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")


def port_files():
    src = os.path.join(ROOT, "unreal", "NWPort", "Source")
    for d, _, files in os.walk(src):
        for f in files:
            if f.endswith((".h", ".cpp")):
                yield os.path.join(d, f)
    for d in (os.path.join(ROOT, "tools"), os.path.join(ROOT, "tools", "ue")):
        for f in os.listdir(d):
            if f.endswith((".py", ".sh")) and f not in ("re_check.py", "make_rules.py"):
                yield os.path.join(d, f)


def check_values():
    from make_rules import load_values
    valores = load_values()
    texts = {os.path.relpath(p, ROOT): open(p, encoding="utf-8", errors="replace").read() for p in port_files()}
    applied, pending = [], []
    for vid, v in valores.items():
        where = sorted({os.path.basename(p) for p, t in texts.items() if re.search(rf"(?<![\w.]){re.escape(vid)}(?![\w.])", t)})
        (applied if where else pending).append((vid, v, where))
    print(f"== Valores do RE: {len(applied)} aplicados, {len(pending)} sem uso no port")
    for vid, v, where in applied:
        warn = "  [hipótese]" if v.get("prova") == "H" else ""
        print(f"  ok    {vid} = {fmt(v)}  ({', '.join(where)}){warn}")
    for vid, v, _ in pending:
        print(f"  falta {vid} = {fmt(v)}  — {v.get('o_que', '')}")
    return valores


def fmt(v):
    val = v.get("valor")
    return "regra" if val is None else f"{val}{(' ' + v['unidade']) if v.get('unidade') else ''}"


def check_rules(valores):
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "regras.json")
    want = {k: v["valor"] for k, v in valores.items() if isinstance(v.get("valor"), (int, float))}
    have = json.load(open(path)) if os.path.exists(path) else None
    print("== Data/regras.json")
    if have == want:
        print(f"  em dia ({len(want)} valores)")
    else:
        diff = sorted(k for k in set(want) | set(have or {}) if (have or {}).get(k) != want.get(k))
        print(f"  desatualizado ({', '.join(diff) if have is not None else 'ausente'}): python3 tools/make_rules.py")


def base(col):
    """Nome que o parser do jogo usa: sem _AI e sem o eixo X/Y/Z (ImpactDistanceY_AI → ImpactDistance)."""
    c = col[:-3] if col.endswith("_AI") else col
    return c[:-1] if re.search(r"Distance[XYZ]$", c) else c


def check_columns():
    path = os.path.join(RE_DIR, "colunas.json")
    if not os.path.exists(path):
        print("== Colunas: sem docs/analise-re/colunas.json (python3 -I tools/re/colunas.py)")
        return
    import make_datatables as md
    for sheet, info in json.load(open(path)).items():
        if sheet != "javelindata_damagetable":
            print(f"== Colunas de {sheet}: sem exportação conhecida no port")
            continue
        game = set(info["colunas"])
        table_file = os.path.join(DT, sheet + ".datasheet.json")
        headers = list(json.load(open(table_file))["rows"][0]) if os.path.exists(table_file) else []
        exported = {md.DAMAGE_SOURCE.get(c, c) for c in md.DAMAGE_COLS}
        read = [h for h in headers if h in game or base(h) in game]
        missing = [h for h in read if h not in exported and h not in md.DAMAGE_IGNORED]
        server = sorted(c for c in exported if c in headers and c not in game and base(c) not in game)
        print(f"== Colunas de {sheet} (parser {info['parser']}): {len(read)} lidas pelo cliente, "
              f"{len(read) - len(missing)} no port")
        for col, why in md.DAMAGE_IGNORED.items():
            print(f"  fora de propósito: {col} ({why})")
        print(f"  lidas pelo cliente e fora do port ({len(missing)}): {' '.join(missing)}")
        if server:
            print(f"  no port e não lidas pelo cliente (servidor): {' '.join(server)}")


def check_questions():
    qdir = os.path.join(RE_DIR, "perguntas")
    states = {}
    for name in sorted(os.listdir(qdir)) if os.path.isdir(qdir) else []:
        m = re.search(r"^- Estado: (\w+)", open(os.path.join(qdir, name)).read(), re.M)
        states.setdefault(m.group(1) if m else "?", []).append(name[:-3])
    print("== Perguntas: " + ", ".join(f"{len(v)} {k}" for k, v in sorted(states.items())))
    if states.get("respondida"):
        print(f"  respondidas, falta aplicar: {' '.join(states['respondida'])}")


def main():
    valores = check_values()
    check_rules(valores)
    check_columns()
    check_questions()


if __name__ == "__main__":
    main()
