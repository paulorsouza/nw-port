"""Windsward creatures (G3): spawners of the region's _ai layers → Data/World/guardaventos_spawns.json.

    python3 tools/make_spawns.py

Each capital of the `06sw0*_ai` layers points to a slice CoatGen/PointSpawner_*.dynamicslice (extract/spawners,
nwbt --x-objects json, tools/linux/build_all.sh 12). From the slice come:
  - AIVariantProviderComponent: javelindata_vitals row (m_vitalstablerowid), category and level;
  - entities wp1..wpN (AIWaypointComponent): position local to the spawner, game task (POI_Graze_LOOP...) and duration.
From the tables: basevitals_common (family, type, HealthMod, DamageMod, loot, scale, name) and levelvariantvitals_common
(level). Positions in the map frame (UE = (-x, y, z) × 100 around the city, like the gatherables).

Port scale (same number of hits as the game): the player's light attack (WeaponBaseDamage 100) equals the BaseDamage of
the creature's level in javelindata_vitalsleveldata and the player's health (1000) equals that level's BaseMaxHealth. So
  health = BaseMaxHealth · HealthMod · 100 / BaseDamage      damage = BaseDamage · DamageMod · 1000 / BaseMaxHealth
(the DmgCoef of each attack comes from the creature's damage table, in the actions DT).

Set (tools/sets/<set>.json) by category: Boar → boar, Wolf → wolf (the others have no set: they do not spawn).
Carcass: the game character becomes the gatherable with the family's name (GatherableControllerComponent "Boar",
"Wolf"): skin it with the knife, loot and trade skill XP from the javelindata_gatherables row.

Output: {"creatures": {vitals_id: {...}}, "spawns": [[creature, x, y, z, yaw, [[x, y, z, task, min, max], ...]], ...]}
"""
SETS = {"Boar": ("boar", "Boar"), "Wolf": ("wolf", "Wolf")}   # category -> (set, carcass gatherable)
import collections
import glob
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGION = glob.glob(os.path.join(ROOT, "extract", "terrain", "**", "r_+04_+01", "capitals"), recursive=True)[0]
DATA = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
SPAWNERS = os.path.join(ROOT, "extract", "spawners")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "guardaventos_spawns.json")
CITY = "Settlement_SouthWindsward_Complete"


def rows(path):
    d = json.load(open(path))
    return d["rows"] if isinstance(d, dict) else d


def find(o, t, out):
    if isinstance(o, dict):
        if o.get("__type") == t:
            out.append(o)
        for v in o.values():
            find(v, t, out)
    elif isinstance(o, list):
        for v in o:
            find(v, t, out)
    return out


def read_spawner(name):
    """(vitals, category, level, local waypoints) of the slice, or None."""
    f = os.path.join(SPAWNERS, name + ".json")
    if not os.path.exists(f):
        return None
    d = json.load(open(f))
    av = find(d, "AIVariantProviderComponentServerFacet", [])
    if not av:
        return None
    wps = []
    for e in find(d, "AZ::Entity", []):
        if not e.get("name", "").lower().startswith("wp"):
            continue
        tr = find(e, "GameTransformComponent", [])
        wf = find(e, "AIWaypointComponentServerFacet", [])
        if not tr or not wf:
            continue
        m = tr[0]["m_worldtm"]["data"]
        wps.append((e["name"].lower(), m[9], m[10], m[11], wf[0].get("m_taskname", ""),
                    float(wf[0].get("m_minduration") or 0), float(wf[0].get("m_maxduration") or 0)))
    wps.sort(key=lambda w: (len(w[0]), w[0]))  # wp1, wp2, ..., wp10
    return av[0]["m_vitalstablerowid"], av[0]["m_vitalscategorytablerowid"], av[0].get("m_vitalslevel", 0), wps


def main():
    caps = json.load(open(glob.glob(os.path.join(REGION, "06swindsward_persistence", "*.capitals.json"))[0]))["Capitals"]
    city = next(c for c in caps if c["sliceName"].endswith(CITY))
    T = city["worldPosition"]
    ang = 2 * math.atan2(city["rotation"]["z"], city["rotation"]["w"])
    ca, sa = math.cos(-ang), math.sin(-ang)

    def to_map(wx, wy, wz):
        dx, dy = wx - T["x"], wy - T["y"]
        lx, ly = ca * dx - sa * dy, sa * dx + ca * dy
        return round(-lx * 100, 1), round(ly * 100, 1), round((wz - T["z"]) * 100, 1)

    base = {r["VitalsID"]: r for r in rows(os.path.join(DATA, "vitalstables", "javelindata_basevitals_common.datasheet.json"))}
    level = {r["VitalsID"]: r for r in rows(os.path.join(DATA, "vitalstables", "javelindata_levelvariantvitals_common.datasheet.json"))}
    by_level = {int(r["Level"]): r for r in rows(os.path.join(DATA, "javelindata_vitalsleveldata.datasheet.json"))}
    gatherables = {r["GatherableID"]: r for r in rows(os.path.join(DATA, "javelindata_gatherables.datasheet.json"))}
    events = {r["EventID"]: r for r in rows(os.path.join(DATA, "javelindata_gameevents.datasheet.json"))}

    cache, creatures, spawns, missing = {}, {}, [], collections.Counter()
    for f in sorted(glob.glob(os.path.join(REGION, "06sw0*_ai", "*.capitals.json"))):
        for cap in json.load(open(f))["Capitals"]:
            name = cap.get("sliceName", "").replace("\\\\", "/").replace("\\", "/").lower()
            if not name.startswith("coatgen/"):
                continue
            if name not in cache:
                cache[name] = read_spawner(name)
            sp = cache[name]
            if not sp:
                missing[os.path.basename(name).split("_")[1]] += 1
                continue
            vid, cat, lvl, wps = sp
            b = base.get(vid)
            if not b:
                missing[vid] += 1
                continue
            if vid not in creatures:
                lv = level.get(vid, {})
                n = int(lv.get("Level") or lvl or 1)
                L = by_level.get(n) or by_level[min(by_level, key=lambda k: abs(k - n))]
                cats = (b.get("VitalsCategories") or "").split(",")
                kind = next((k for k in SETS if k in cats), None)
                corpse = gatherables.get(SETS[kind][1], {}) if kind else {}
                ev = events.get(corpse.get("GameEventID"), {})
                creatures[vid] = {
                    "set": SETS[kind][0] if kind else "",
                    "health": round(L["BaseMaxHealth"] * float(b.get("HealthMod") or 1) * 100 / L["BaseDamage"]),
                    "damage": round(L["BaseDamage"] * float(b.get("DamageMod") or 1) * 1000 / L["BaseMaxHealth"], 1),
                    "hostile": "Hostile" in cats,
                    "skin": {"loot": corpse.get("FinalLootTable") or "", "time": float(corpse.get("BaseGatherTime") or 0),
                             "tradeskill": corpse.get("Tradeskill") or "", "trade_xp": float(ev.get("CategoricalProgressionReward") or 0),
                             "action": corpse.get("GatheringAction") or "", "name_key": str(corpse.get("DisplayName") or "").lstrip("@").lower()}
                    if corpse else None,
                    "name_key": str(b.get("DisplayName") or "").lstrip("@").lower(),
                    "family": b.get("Family") or "", "type": b.get("CreatureType") or "",
                    "categories": b.get("VitalsCategories") or "", "category": cat,
                    "level": n,
                    "health_mod": float(b.get("HealthMod") or 1), "damage_mod": float(b.get("DamageMod") or 1),
                    "mitigation": float(b.get("PhysicalMitigation") or 0),
                    "loot": b.get("LootTableId") or "", "loot_tags": b.get("LootTags") or "",
                    "scale": float(b.get("CharacterScale") or 1),
                }
            p, q = cap["worldPosition"], cap["rotation"]
            yaw = 2 * math.atan2(q["z"], q["w"])
            cy, sy = math.cos(yaw), math.sin(yaw)
            x, y, z = to_map(p["x"], p["y"], p["z"])
            path = []
            for _, wx, wy, wz, task, tmin, tmax in wps:
                path.append([*to_map(p["x"] + cy * wx - sy * wy, p["y"] + sy * wx + cy * wy, p["z"] + wz), task, tmin, tmax])
            # Map rotation: the world one minus the city one, mirrored in X (Cry → UE).
            spawns.append([vid, x, y, z, round(180 - math.degrees(yaw - ang), 1), path])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({"creatures": creatures, "spawns": spawns}, open(OUT, "w"), separators=(",", ":"))
    fam = collections.Counter(creatures[s[0]]["family"] for s in spawns)
    with_set = collections.Counter(creatures[s[0]]["set"] for s in spawns if creatures[s[0]]["set"])
    print(f"with set: {dict(with_set)}")
    print(f"{len(spawns)} spawns, {len(creatures)} creatures {dict(fam)}; without variant: {sum(missing.values())} {dict(missing)}")


if __name__ == "__main__":
    main()
