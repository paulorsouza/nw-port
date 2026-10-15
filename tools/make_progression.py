"""Character level and attributes, from the game tables.

    python3 tools/make_progression.py

Reads extract/data/.../javelindata_xpamountsbylevel and javelindata_attribute<attribute> and writes
unreal/NWPort/Data/Progression/progression.json:
  levels[n]       = {"xp": total XP to reach level n+1, "points": attribute points gained on reaching it}
  attributes[a][v] = {"mod": ModifierValueSum, "health": extra health (constitution only)} for value v (5..500)
  tradeskills[o][n] = {"xp": total XP to reach level n, "eff": GatheringEfficiency, "roll": RollBonus}
                     for the gathering trade skills (javelindata_tradeskill<skill>; InfluenceCost = XP from the
                     previous level to this one), up to Expansion01MaxLevel of javelindata_categoricalprogression (250)
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLES = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Progression")
ATTRS = ["strength", "dexterity", "intelligence", "focus", "constitution"]
# Gathering (0..3), refining (4..8), crafting (9..11; area 4: tools/make_crafting.py, docs/game/crafting.md),
# jewelcrafting (12) and armoring (13; docs/game/items.md); same order as NWTradeskill in C++.
TRADESKILLS = ["Mining", "Logging", "Harvesting", "Skinning", "Smelting", "Woodworking", "Leatherworking", "Weaving",
               "Stonecutting", "Weaponsmithing", "Engineering", "Arcana", "Jewelcrafting", "Armoring"]


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
            row = {"xp": total, "eff": float(r.get("GatheringEfficiency") or 0), "roll": int(r.get("RollBonus") or 0)}
            if r.get("MaxGearScoreBonusT5") is not None:
                # Crafter GS bonus per item tier (T2..T5), minimum and maximum (docs/game/items.md).
                row["gs"] = [[int(r.get(f"MinGearScoreBonusT{n}") or 0), int(r.get(f"MaxGearScoreBonusT{n}") or 0)] for n in range(2, 6)]
            table.append(row)
        tradeskills[t] = table
    os.makedirs(OUT, exist_ok=True)
    # Level tutorials (area 4, tools/tutorials.json).
    tutorials = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "tutorials.json")))
    tutorials.pop("_comment", None)
    json.dump({"base_attribute": 5, "levels": levels, "attributes": attributes, "tradeskills": tradeskills,
               "tutorials": tutorials},
              open(os.path.join(OUT, "progression.json"), "w"), separators=(",", ":"))
    print(f"{len(levels)} levels, {sum(l['points'] for l in levels)} attribute points; {len(ATTRS)} attributes; "
          + ", ".join(f"{t} up to {len(v) - 1}" for t, v in tradeskills.items()))


if __name__ == "__main__":
    main()
