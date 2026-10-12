"""Nível do personagem e atributos, das tabelas do jogo.

    python3 tools/make_progression.py

Lê extract/data/.../javelindata_xpamountsbylevel e javelindata_attribute<atributo> e grava
unreal/NWPort/Data/Progression/progression.json:
  levels[n]       = {"xp": XP total para chegar ao nível n+1, "points": pontos de atributo ganhos ao chegar nele}
  attributes[a][v] = {"mod": ModifierValueSum, "health": vida extra (só constituição)} para o valor v (5..500)
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLES = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Progression")
ATTRS = ["strength", "dexterity", "intelligence", "focus", "constitution"]


def rows(name):
    d = json.load(open(os.path.join(TABLES, f"javelindata_{name}.datasheet.json")))
    return d["rows"] if isinstance(d, dict) else d


def main():
    levels = [{"xp": int(r["XPToLevel"]), "points": int(r["AttributePoints"] or 0)}
              for r in sorted(rows("xpamountsbylevel"), key=lambda r: r["Level Number"])]
    attributes = {}
    for a in ATTRS:
        attributes[a] = {str(r["Level"]): {"mod": float(r.get("ModifierValueSum") or 0), "health": float(r.get("Health") or 0)}
                         for r in rows(f"attribute{a}")}
    os.makedirs(OUT, exist_ok=True)
    json.dump({"base_attribute": 5, "levels": levels, "attributes": attributes},
              open(os.path.join(OUT, "progression.json"), "w"), separators=(",", ":"))
    print(f"{len(levels)} níveis, {sum(l['points'] for l in levels)} pontos de atributo; {len(ATTRS)} atributos")


if __name__ == "__main__":
    main()
