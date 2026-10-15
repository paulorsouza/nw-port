"""Colunas que os parsers de datasheet do NewWorld.exe leem → re-local/colunas.json (usado por tools/re_check.py).

    python3 -I tools/re/colunas.py

Lista as strings que cada parser carrega (lea com rip) e fica com as que parecem nome de coluna. Variantes _AI e
X/Y/Z não aparecem como string: o parser monta o nome com sufixo (ex.: "ImpactDistance" lê ImpactDistanceX/Y/Z).
Para outro datasheet, achar o parser pela string de uma coluna (xref.py str "Coluna") e acrescentar em PARSERS.
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xref  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# datasheet → início do parser de linha (simbolos.md).
PARSERS = {"javelindata_damagetable": 0x...}
NOT_COLUMNS = {"Default", "none"}


def strings(start):
    b, e = xref.func_range(start)
    out = subprocess.run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn", f"--start-address={hex(b)}",
                          f"--stop-address={hex(e)}", xref.EXE], capture_output=True, text=True).stdout
    names = []
    for m in re.finditer(r"lea\s+\w+,\[rip\+0x[0-9a-f]+\]\s+# 0x([0-9a-f]+)", out):
        s = xref.cstr(int(m.group(1), 16), 60)
        if re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", s or "") and s not in NOT_COLUMNS and s not in names:
            names.append(s)
    return names


def main():
    res = {name: {"parser": hex(start), "colunas": strings(start)} for name, start in PARSERS.items()}
    path = os.path.join(ROOT, "re-local", "colunas.json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(res, open(path, "w"), indent=1)
    for name, r in res.items():
        print(f"{name}: {len(r['colunas'])} colunas ({r['parser']})")


if __name__ == "__main__":
    main()
