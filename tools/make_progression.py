"""Nível do personagem e atributos, das tabelas do jogo.

    python3 tools/make_progression.py

Lê extract/data/.../javelindata_xpamountsbylevel e javelindata_attribute<atributo> e grava
unreal/NWPort/Data/Progression/progression.json:
  levels[n]       = {"xp": XP total para chegar ao nível n+1, "points": pontos de atributo ganhos ao chegar nele}
  attributes[a][v] = {"mod": ModifierValueSum, "health": vida extra (só constituição)} para o valor v (5..500)
  tradeskills[o][n] = {"xp": XP total para chegar ao nível n, "eff": GatheringEfficiency, "roll": RollBonus}
                     para os ofícios de coleta (javelindata_tradeskill<ofício>; InfluenceCost = XP do nível
                     anterior até este), até o Expansion01MaxLevel de javelindata_categoricalprogression (250)
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLES = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Progression")
ATTRS = ["strength", "dexterity", "intelligence", "focus", "constitution"]
TRADESKILLS = ["Mining", "Logging", "Harvesting", "Skinning"]


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
    caps = {r["CategoricalProgressionId"]: r for r in rows("categoricalprogression")}
    tradeskills = {}
    for t in TRADESKILLS:
        cap = int(caps[t].get("Expansion01MaxLevel") or caps[t].get("MaxLevel") or 200)
        total, table = 0, []
        for r in sorted(rows(f"tradeskill{t.lower()}"), key=lambda r: r.get("Level") or 0):
            lv = int(r.get("Level") or 0)
            if lv > cap:
                break
            total += int(r.get("InfluenceCost") or 0)
            table.append({"xp": total, "eff": float(r.get("GatheringEfficiency") or 0), "roll": int(r.get("RollBonus") or 0)})
        tradeskills[t] = table
    os.makedirs(OUT, exist_ok=True)
    json.dump({"base_attribute": 5, "levels": levels, "attributes": attributes, "tradeskills": tradeskills},
              open(os.path.join(OUT, "progression.json"), "w"), separators=(",", ":"))
    print(f"{len(levels)} níveis, {sum(l['points'] for l in levels)} pontos de atributo; {len(ATTRS)} atributos; "
          + ", ".join(f"{t} até {len(v) - 1}" for t, v in tradeskills.items()))


if __name__ == "__main__":
    main()
