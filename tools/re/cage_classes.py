"""Map of the CAGE classes (activities and conditions) in NewWorld.exe: name -> vtable -> slots.

Pattern (C): each class has a "GetName" in vtable slot 1 that ends in `lea rax,[name]; ret` (directly, or
after `test rdx,rdx; je; jmp [rax+8]`). The script finds those `lea; ret` in .text, walks up to the function start via
.pdata, finds the pointers to it in .rdata (vtable = pointer - 8) and lists the slots. Keeps the functions in the CAGE
range, the `ActionCondition*` names and the types used in the actionlists of extract/serverpak/client.
The CAGE range and the shared empty functions (ret, false, true, zero) come from re-local/enderecos.json
("cage_classes": {"faixa": [start, end], "stubs": {address: name}}), found by the nw-re agent for the current build.

    python3 -I tools/re/cage_classes.py [0xSTART 0xEND] > out.tsv
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import xref
from xref import mm

_CFG = xref.local_addresses().get("cage_classes", {})
if len(sys.argv) > 2:
    LO, HI = int(sys.argv[1], 16), int(sys.argv[2], 16)
elif "faixa" in _CFG:
    LO, HI = (int(x, 16) for x in _CFG["faixa"])
else:
    sys.exit("CAGE range: pass 0xSTART 0xEND or write re-local/enderecos.json")
STUB = {int(k, 16): v for k, v in _CFG.get("stubs", {}).items()}
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
    # pointers to the function in .rdata
    pat = struct.pack("<Q", f)
    i = rdata.find(pat)
    while i >= 0:
        if i % 8 == 0:
            vt = rva + i - 8
            slots = [q(vt + 8 * k) for k in range(NSLOTS)]
            rows.append((name, vt, f, slots))
        i = rdata.find(pat, i + 1)

print("name\tvtable\tgetname\t" + "\t".join(f"s{k}" for k in range(NSLOTS)))
for name, vt, f, slots in sorted(rows, key=lambda r: (r[0], r[1])):
    cells = [STUB.get(x, hex(x)) if x else "-" for x in slots]
    print(f"{name}\t{vt:#x}\t{f:#x}\t" + "\t".join(cells))
