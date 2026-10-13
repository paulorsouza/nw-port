"""Mapa das classes CAGE (atividades e condições) do NewWorld.exe: nome -> vtable -> slots.

Padrão (C): cada classe tem um "GetName" no slot 1 da vtable que termina em `lea rax,[nome]; ret` (direto, ou
depois de `test rdx,rdx; je; jmp [rax+8]`). O script acha esses `lea; ret` no .text, sobe ao início da função pela
.pdata, acha os ponteiros para ela no .rdata (vtable = ponteiro - 8) e lista os slots. Fica com as funções da faixa
CAGE, com os nomes `ActionCondition*` e com os tipos usados nas actionlists de extract/serverpak/client.
Funções vazias compartilhadas: ret = 0x..., falso = 0x..., verdadeiro = 0x..., zero = 0x....

    python3 -I tools/re/cage_classes.py [0xINI 0xFIM] > saida.tsv
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import xref
from xref import mm

LO, HI = (int(sys.argv[1], 16), int(sys.argv[2], 16)) if len(sys.argv) > 2 else (0x..., 0x...)
STUB = {0x...: "ret", 0x...: "falso", 0x...: "verdadeiro", 0x...: "zero"}
NSLOTS = 24

def q(v):
    o = xref.va2off(v)
    return None if o is None else struct.unpack_from("<Q", mm, o)[0]

rva, rsz = xref.SECS[".rdata"][0], xref.SECS[".rdata"][3]
rraw = xref.SECS[".rdata"][2]
rdata = mm[rraw:rraw + rsz]

import glob
import xml.etree.ElementTree as ET
USED = set()
for fn in glob.glob("extract/serverpak/client/**/*.actionlist", recursive=True):
    try:
        USED.update(x.tag for x in ET.parse(fn).getroot().iter())
    except ET.ParseError:
        pass

tva, _, traw, tsz = xref.SECS[".text"]
code = mm[traw:traw + tsz]
rows = []
seen = set()
for m in re.finditer(rb"\x48\x8d\x05(....)\xc3", code, re.S):
    lea = tva + m.start()
    s = lea + 7 + struct.unpack("<i", m.group(1))[0]
    name = xref.cstr(s, 80)
    if not name or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:<>]*", name):
        continue
    fr = xref.func_of(lea)
    f = fr[0] if fr else lea
    if lea - f > 0x10 or f in seen:
        continue
    if not (LO <= f < HI or name.startswith("ActionCondition") or name in USED):
        continue
    seen.add(f)
    # ponteiros para a função no .rdata
    pat = struct.pack("<Q", f)
    i = rdata.find(pat)
    while i >= 0:
        if i % 8 == 0:
            vt = rva + i - 8
            slots = [q(vt + 8 * k) for k in range(NSLOTS)]
            rows.append((name, vt, f, slots))
        i = rdata.find(pat, i + 1)

print("nome\tvtable\tgetname\t" + "\t".join(f"s{k}" for k in range(NSLOTS)))
for name, vt, f, slots in sorted(rows, key=lambda r: (r[0], r[1])):
    cells = [STUB.get(x, hex(x)) if x else "-" for x in slots]
    print(f"{name}\t{vt:#x}\t{f:#x}\t" + "\t".join(cells))
