#!/usr/bin/env python3
"""Checks which reverse-engineering findings (re-local) have already made it into the port.

    python3 tools/re_check.py

1. Values (re-local/valores/<area>.json): each id is searched for in the port code (C++ in unreal/NWPort/Source,
   Python in tools/ and tools/ue/). A numeric value goes in through FNWRules (Data/regras.json); a rule (null value)
   goes in by citing the id in a comment where it is applied. Unused = still to be applied.
2. Data/regras.json out of date relative to the values (run tools/make_rules.py).
3. Columns (re-local/colunas.json, taken from the executable by tools/re/colunas.py): damagetable columns the
   game client reads and the port does not export (make_datatables.DAMAGE_COLS), and those the port exports and the
   client does not read (server-side: the port, being both server and client, uses them anyway).
4. Questions (re-local/perguntas/<area>-NNN.md) answered and not applied yet.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
RE_DIR = os.path.join(ROOT, "re-local")
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
        warn = "  [hypothesis]" if v.get("prova") == "H" else ""
        print(f"  ok    {vid} = {fmt(v)}  ({', '.join(where)}){warn}")
    for vid, v, _ in pending:
        print(f"  todo  {vid} = {fmt(v)}  — {v.get('o_que', '')}")
    return valores


def fmt(v):
    val = v.get("valor")
    return "rule" if val is None else f"{val}{(' ' + v['unidade']) if v.get('unidade') else ''}"


def check_rules(valores):
    path = os.path.join(ROOT, "unreal", "NWPort", "Data", "regras.json")
    want = {k: v["valor"] for k, v in valores.items() if isinstance(v.get("valor"), (int, float))}
    have = json.load(open(path)) if os.path.exists(path) else None
    print("== Data/regras.json")
    if have == want:
        print(f"  up to date ({len(want)} values)")
    else:
        diff = sorted(k for k in set(want) | set(have or {}) if (have or {}).get(k) != want.get(k))
        print(f"  out of date ({', '.join(diff) if have is not None else 'missing'}): python3 tools/make_rules.py")


def base(col):
    """Name the game parser uses: without _AI and without the X/Y/Z axis (ImpactDistanceY_AI → ImpactDistance)."""
    c = col[:-3] if col.endswith("_AI") else col
    return c[:-1] if re.search(r"Distance[XYZ]$", c) else c


def check_columns():
    path = os.path.join(RE_DIR, "colunas.json")
    if not os.path.exists(path):
        print("== Columns: no re-local/colunas.json (python3 -I tools/re/colunas.py)")
        return
    import make_datatables as md
    for sheet, info in json.load(open(path)).items():
        if sheet != "javelindata_damagetable":
            print(f"== Columns of {sheet}: no known export in the port")
            continue
        game = set(info["colunas"])
        table_file = os.path.join(DT, sheet + ".datasheet.json")
        headers = list(json.load(open(table_file))["rows"][0]) if os.path.exists(table_file) else []
        exported = {md.DAMAGE_SOURCE.get(c, c) for c in md.DAMAGE_COLS}
        read = [h for h in headers if h in game or base(h) in game]
        missing = [h for h in read if h not in exported and h not in md.DAMAGE_IGNORED]
        server = sorted(c for c in exported if c in headers and c not in game and base(c) not in game)
        print(f"== Columns of {sheet} (parser {info['parser']}): {len(read)} read by the client, "
              f"{len(read) - len(missing)} in the port")
        for col, why in md.DAMAGE_IGNORED.items():
            print(f"  left out on purpose: {col} ({why})")
        print(f"  read by the client and not in the port ({len(missing)}): {' '.join(missing)}")
        if server:
            print(f"  in the port and not read by the client (server): {' '.join(server)}")


def check_questions():
    qdir = os.path.join(RE_DIR, "perguntas")
    states = {}
    for name in sorted(os.listdir(qdir)) if os.path.isdir(qdir) else []:
        m = re.search(r"^- Estado: (\w+)", open(os.path.join(qdir, name)).read(), re.M)
        states.setdefault(m.group(1) if m else "?", []).append(name[:-3])
    print("== Perguntas: " + ", ".join(f"{len(v)} {k}" for k, v in sorted(states.items())))
    if states.get("respondida"):
        print(f"  answered, still to be applied: {' '.join(states['respondida'])}")


def main():
    valores = check_values()
    check_rules(valores)
    check_columns()
    check_questions()


if __name__ == "__main__":
    main()
