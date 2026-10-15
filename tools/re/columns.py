"""Columns that the NewWorld.exe datasheet parsers read → re-local/colunas.json (used by tools/re_check.py).

    python3 -I tools/re/columns.py

Lists the strings each parser loads (rip-relative lea) and keeps the ones that look like column names. _AI and X/Y/Z
variants do not appear as strings: the parser builds the name with a suffix (e.g. "ImpactDistance" reads ImpactDistanceX/Y/Z).
For another datasheet, find the parser by a column's string (xref.py str "Column") and write it to re-local/enderecos.json ("colunas": {datasheet: address}).
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xref  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# datasheet → start of the row parser (re-local/enderecos.json, build from tools/game.version).
PARSERS = {k: int(v, 16) for k, v in xref.local_addresses().get("colunas", {}).items()}
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
        print(f"{name}: {len(r['colunas'])} columns ({r['parser']})")


if __name__ == "__main__":
    main()
