"""Inventário dos tipos CAGE usados nas actionlists: quantas vezes cada atividade e condição aparece, se é usada por
NPC e/ou jogador e se o nome existe como string no NewWorld.exe (atividade pelo nome; condição como
ActionCondition<nome>).

    python3 -I tools/re/cage_inventory.py [raiz]   # raiz padrão: extract/serverpak/client
"""
import collections, glob, os, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(__file__))
from xref import mm

root = sys.argv[1] if len(sys.argv) > 1 else "extract/serverpak/client"
acts, conds = collections.Counter(), collections.Counter()
who = collections.defaultdict(set)
for f in glob.glob(os.path.join(root, "**/*.actionlist"), recursive=True):
    side = "player" if "/playerdata/" in f else "npc"
    try:
        r = ET.parse(f).getroot()
    except ET.ParseError:
        continue
    for a in r.iter("Action"):
        for sec in a:
            if sec.tag == "Activities":
                for x in sec:
                    acts[x.tag] += 1; who[x.tag].add(side)
            elif sec.tag.endswith("Condition"):
                for x in sec.iter():
                    if x is sec or x.tag in ("Default", "And", "Or", "Not"):
                        continue
                    conds[x.tag] += 1; who[x.tag].add(side)

def present(name, prefix=""):
    if name.startswith(prefix):
        prefix = ""
    return "sim" if mm.find(b"\0" + (prefix + name).encode() + b"\0") >= 0 else "-"

for n, c in acts.most_common():
    print(c, n, ",".join(sorted(who[n])), present(n))
print("== CONDICOES")
for n, c in conds.most_common():
    print(c, n, ",".join(sorted(who[n])), present(n, "ActionCondition"))
