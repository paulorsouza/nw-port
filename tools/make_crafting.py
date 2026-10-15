#!/usr/bin/env python3
"""City stations (Everfall and Windsward) and the game's refining recipes -> unreal/NWPort/Data/Crafting/refining.json.

    python3 tools/make_crafting.py        (before tools/make_items.py: the catalog includes the recipe items)

Sources:
- stations: PrefabSpawnerComponent of each city slice (CITIES; extract/slicejson, step sistemas-02 of build_all),
  slice `Settlement_Station_<Type>_T5` / `settlement_station_<type>_master`, position in m_worldtm. The slice frame is
  the city map's frame (both maps are town-local, like make_npcs.py and make_gatherables.py), so each station keeps
  its slice position and gets the `city` field; ANWNpcField spawns only its city's stations;
- recipes: javelindata_crafting_refining, refining trade skills (Smelting, Woodworking, Leatherworking, Weaving,
  Stonecutting), known by default (bKnownByDefault), without the rune station (RuneglassStation1);
- ingredient categories (Type = Category_Only): IngredientCategories column of javelindata_itemdefinitions_master_*;
- trade skill XP per recipe: javelindata_gameevents (GameEventID -> CategoricalProgressionReward) and character XP
  (UniversalExpAmount).

Refining time: the game has none (docs/game/city.md, "No source"); the port refines instantly.
"""
import glob
import json
import xml.etree.ElementTree as ET
import math
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
SETTLEMENTS = os.path.join(ROOT, "extract", "slicejson", "slices", "structures", "settlements", "v2_settlements")
# City id (Data/World/<city>_npcs.json, governance) -> settlement slice. Map: L_QuedaEterna and L_SouthWindsward.
CITIES = {"queda_eterna": "settlement_northwindsward_complete", "guardaventos": "settlement_southwindsward_complete"}
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Crafting", "refining.json")
# Station type in the slice -> recipe StationType and trade skill. The storage sheds (settlement_storage_*) come in as
# stations of type "storage" (city Storage).
STATIONS = {"smelting": ("smelting", "Smelting"), "carpentry": ("carpentry", "Woodworking"),
            "tanning": ("tanning", "Leatherworking"), "weaving": ("weaving", "Weaving"),
            "masonry": ("masonry", "Stonecutting"),
            # Crafting stations (docs/game/crafting.md): blacksmithing, engineering and alchemy. Kitchen and
            # tailoring come in once there is food with effects and armor.
            "blacksmithing": ("blacksmith", "Weaponsmithing"), "engineering": ("engineering", "Engineering"),
            "alchemy": ("alchemy", "Arcana"),
            # Jewelcrafting (cut gems; docs/game/items.md "Slot and gems"). Armoring joins the same station with armor.
            "outfitting": ("outfitting", "Jewelcrafting")}
# The port's crafting recipes (docs/game/crafting.md): weapons, shields and tools T2–T5, T5 health potion and T2
# alchemy reagents.
CRAFT_SOURCES = ("javelindata_crafting_weapon", "javelindata_crafting_engineer", "javelindata_crafting_arcana",
                 "javelindata_crafting_jeweler", "javelindata_crafting_armorer")
# Trade skills with no station of their own in the slice: armoring (plate at the forge, leather/cloth at outfitting).
EXTRA_SKILLS = {"Armoring"}
CRAFT_GEAR = re.compile(r"^Procedural_(Longsword|ShieldRound|ShieldKite|ShieldTower|Hatchet|LoggingAxe|MiningPick|GauntletIce|"
                        r"SkinningKnife|Sickle|"
                        # Armor (plate, leather, cloth; without the "dress" line) and jewelry (docs/game/items.md).
                        r"(Plate|Leather|Cloth)(Head|Chest|Hands|Legs|Feet)|Amulet|Ring|Earring)T[2-5]$")
# Jewelry in the port (docs/game/items.md "Armor and jewelry"): no components; the precious ingot of each component
# (chain, band, hook, setting) goes in directly, added to the recipe's precious metal.
TRINKET_COMPONENT = re.compile(r"^Trinket(Chain|Band|Hook|Setting)(T\d+)?$")
# Bags (Armoring at the outfitting station; HoldingRune from the merchant) and the arcane waters of the Ice Gauntlet.
CRAFT_INSTANT = re.compile(r"^(Procedural_PotionHealthT5|Alchemy[A-Za-z]+T2|Procedural_BagAT[2-5]|AlchemyWaterT3|EssenceWaterT1|"
                           r"(Onyx|Opal|Diamond|Sapphire|Jasper|Emerald|Malachite)CutT[1-4]|"
                           r"Runeglass_Case_Empty_(Melee|Arcane))$")
# Runeglass (docs/game/items.md "Slot and gems"): the game uses the rune station (RuneglassStation1), which Everfall
# lacks. In the port the case comes from the forge (Weaponsmithing), the glass (case + Pure gem) from stonecutting, T5.
RUNEGLASS = re.compile(r"^Runeglass_(Case_Empty_(Melee|Arcane)|Gem_(Onyx|Opal|Diamond|Sapphire|Jasper|Emerald|Malachite)_(Melee|Arcane))$")
RUNEGLASS_STATION = {"Weaponsmithing": "blacksmith5", "Stonecutting": "masonry5"}


def load_loc(language):
    out = {}
    for f in glob.glob(os.path.join(ROOT, "extract", "loc", "localization", language, "*.loc.xml")):
        try:
            for s in ET.parse(f).getroot().iter("string"):
                out[s.get("key", "").lower()] = re.sub(r"<[^>]+>", "", s.text or "")
        except ET.ParseError:
            continue
    return out


def rows(name):
    return json.load(open(os.path.join(DT, name + ".datasheet.json")))["rows"]


def stations():
    """Stations of every extracted city slice (city without an extracted slice: left out, with a message)."""
    out = []
    for city, name in CITIES.items():
        path = os.path.join(SETTLEMENTS, name + ".dynamicslice.json")
        if not os.path.exists(path):
            print(f"== no slice {name}: city {city} without stations (step sistemas-02 of build_all)")
            continue
        found = city_stations(path)
        for s in found:
            s["city"] = city
        out += found
    return out


def city_stations(path):
    """Stations of one city slice: type, tier and map position (UE = (-x, y, z) * 100, like make_npcs.py)."""
    out = []
    data = json.load(open(path))

    def walk(o):
        if isinstance(o, dict):
            comps = o.get("components") or o.get("Components")
            if isinstance(comps, dict):
                comps = comps.get("element")
            if isinstance(comps, list):
                tm, hint = None, ""
                for c in comps:
                    if not isinstance(c, dict):
                        continue
                    if "m_worldtm" in c:
                        tm = c["m_worldtm"]["data"]
                    sa = c.get("m_sliceasset")
                    if isinstance(sa, dict):
                        hint = sa.get("hint", "")
                if tm and "settlement_trading_post" in hint.lower():
                    # Trading Post (market, docs/game/city.md "Trading Post"): station of type "tradingpost".
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))
                    out.append({"type": "tradingpost", "skill": "", "tier": 0,
                                "pos": [round(-tm[9] * 100, 1), round(tm[10] * 100, 1), round(tm[11] * 100, 1)],
                                "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
                # City boards (governance, docs/game/economy.md): town project = city counter
                # (sell raw materials, buy the list); territory planning = governor's panel.
                for board, kind in (("town_project_board", "cityboard"), ("territory_planning_board", "governance")):
                    if tm and board in hint.lower():
                        yaw = math.degrees(math.atan2(tm[1], tm[0]))
                        out.append({"type": kind, "skill": "", "tier": 0,
                                    "pos": [round(-tm[9] * 100, 1), round(tm[10] * 100, 1), round(tm[11] * 100, 1)],
                                    "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
                # Settlement teleporter (Settlement_Ancient_Teleporter_A): travel to the other town (docs/game/city.md
                # "Teleporter").
                if tm and "teleporters/settlement_ancient_teleporter" in hint.lower():
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))
                    out.append({"type": "teleporter", "skill": "", "tier": 0,
                                "pos": [round(-tm[9] * 100, 1), round(tm[10] * 100, 1), round(tm[11] * 100, 1)],
                                "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
                if tm and "settlement_storage" in hint.lower():
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))
                    out.append({"type": "storage", "skill": "", "tier": 0,
                                "pos": [round(-tm[9] * 100, 1), round(tm[10] * 100, 1), round(tm[11] * 100, 1)],
                                "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
                m = re.search(r"settlement_station_([a-z]+)_(t\d|master)", hint.lower())
                if tm and m and m.group(1) in STATIONS:
                    x, y, z = tm[9], tm[10], tm[11]
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))   # X column of the rotation
                    out.append({"type": STATIONS[m.group(1)][0], "skill": STATIONS[m.group(1)][1],
                                "tier": 5 if m.group(2) in ("t5", "master") else int(m.group(2)[1]),
                                "pos": [round(-x * 100, 1), round(y * 100, 1), round(z * 100, 1)],
                                "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)
    walk(data)
    return out


def main():
    skills = {s for _, s in STATIONS.values()} | EXTRA_SKILLS
    jeweler = {r["RecipeID"]: r for r in rows("javelindata_crafting_jeweler")}

    def component_ingot(category, tier):
        """Ingot and quantity of the jewelry component (the game's Trinket_<Part>T<tier>)."""
        m = TRINKET_COMPONENT.match(category)
        rec = jeweler.get(f"Trinket_{m.group(1)}T{tier}")
        return (rec["Ingredient1"], int(rec["Qty1"])) if rec else (None, 0)
    events = {r["EventID"]: r for r in rows("javelindata_gameevents")}
    categories = {}
    for f in glob.glob(os.path.join(DT, "javelindata_itemdefinitions_master_*.datasheet.json")):
        for r in json.load(open(f))["rows"]:
            for c in str(r.get("IngredientCategories") or "").split(","):
                if c.strip():
                    categories.setdefault(c.strip(), set()).add(r["ItemID"])

    # Screen tree names (family and group): DisplayText of javelindata_craftingcategories, else <Group>_GroupName.
    locs = {lang: load_loc(lang) for lang in ("pt-br", "en-us")}
    cat_text = {r["CategoryID"]: str(r.get("DisplayText") or "") for r in rows("javelindata_craftingcategories")}

    def tree_name(cid):
        keys = [cat_text.get(cid, "").lstrip("@").lower(), f"{cid}_groupname".lower(), f"{cid}_categoryname".lower()]
        return {lang: next((loc[k] for k in keys if k and k in loc), cid) for lang, loc in locs.items()}

    # Tree order: CategorySortOrder and GroupSortOrder from lyshineui/crafting/craftingsortinfo.lua; what is not in
    # the list comes after (craftingtreeview.lua sorts by sortingIndex, those without an index last).
    sort_lua = open(os.path.join(ROOT, "extract/lua/lyshineui/crafting/craftingsortinfo.lua")).read()
    cut = sort_lua.index("L0_1.CategorySortOrder")
    cat_order = {c: i for i, c in enumerate(re.findall(r'"([^"]+)"', sort_lua[:cut]))}
    grp_part = sort_lua[cut:sort_lua.index("L0_1.GroupSortOrder")]
    grp_order = {g: i for i, g in reversed(list(enumerate(re.findall(r'"([^"]+)"', grp_part))))}

    recipes, used_cats = [], set()
    source = [(r, False) for r in rows("javelindata_crafting_refining")]
    for name in CRAFT_SOURCES:
        source += [(r, True) for r in rows(name) if CRAFT_GEAR.match(r["RecipeID"]) or CRAFT_INSTANT.match(r["RecipeID"])]
    refining_skills = {"Smelting", "Woodworking", "Leatherworking", "Weaving", "Stonecutting"}
    for r, crafted in source:
        if r.get("Tradeskill") not in (skills if crafted else refining_skills):
            continue
        station_type = str(r.get("StationType1", ""))
        if station_type.startswith("Runeglass"):
            if not RUNEGLASS.match(r["RecipeID"]):
                continue
            station_type = RUNEGLASS_STATION[r["Tradeskill"]]
        if not r.get("bKnownByDefault"):
            continue   # event/achievement variants (e.g. CharcoalT1_WoodenCoin)
        if r["RecipeID"].startswith("TerritoryStandingSupplies"):
            continue   # territory project supplies (need azoth), not refining
        ing = []
        # Fixed refined material of the tier: gear, and bags (an item with no GS of its own in the port).
        gear = crafted and (CRAFT_GEAR.match(r["RecipeID"]) or r["RecipeID"].startswith("Procedural_Bag"))
        for i in range(1, 8):
            iid, qty = r.get(f"Ingredient{i}"), r.get(f"Qty{i}")
            if not iid or not qty:
                continue
            cat = r.get(f"Type{i}") == "Category_Only"
            if gear and cat:
                # Crafting in the port (docs/game/crafting.md): only refined material, one per slot, of the item tier
                # (the game lets you choose among several of the category, rare ones included).
                tier = int(r.get("BaseTier") or 2)
                precious = f"IngotPreciousT{tier - 1}" if tier < 5 else "IngotT5"   # silver, gold, platinum, orichalcum
                if TRINKET_COMPONENT.match(iid):
                    iid, qty = component_ingot(iid, tier)
                elif iid == "MetalPrecious":
                    iid = precious
                else:
                    # ArcanaWater: the water of the item tier, as in the game's named gauntlets of each tier
                    # (javelindata_crafting_arcana 1hGauntletIce_*: T2 AlchemyWaterT1 ... T5 EssenceWaterT1).
                    iid = {"Timber": f"TimberT{tier}", "Leather": f"LeatherT{tier}", "Cloth": f"ClothT{tier}",
                           "Metal": f"IngotT{tier}",
                           "ArcanaWater": {2: "AlchemyWaterT1", 3: "AlchemyWaterT2", 4: "AlchemyWaterT3"}.get(tier, "EssenceWaterT1"),
                           }.get(iid, re.sub(r"^MetalT(\d)$", r"IngotT\1", iid))
                cat = False
                same = next((x for x in ing if x[0] == iid), None)
                if same:
                    same[1] += int(qty)   # jewelry: components and precious metal become a single ingot
                    continue
            if cat:
                used_cats.add(iid)
            ing.append([iid, int(qty), cat])
        ev = events.get(r.get("GameEventID") or "", {})
        station = re.match(r"([a-z]+)(\d)", station_type.lower())
        recipes.append({
            "id": r["RecipeID"], "skill": r["Tradeskill"], "level": int(r.get("RecipeLevel") or 0),
            "station": station.group(1) if station else "", "tier": int(station.group(2)) if station else 0,
            "out": r["ItemID"], "qty": int(r.get("OutputQty") or 1), "ing": ing,
            "fee": int(r.get("CraftingFee") or 0), "group": r.get("CraftingGroup") or "",
            "category": r.get("CraftingCategory") or "",
            "category_name": tree_name(r.get("CraftingCategory") or ""), "group_name": tree_name(r.get("CraftingGroup") or ""),
            "category_order": cat_order.get(r.get("CraftingCategory") or "", 9999),
            "group_order": grp_order.get(r.get("CraftingGroup") or "", 9999),
            # First-craft bonus only for recipes with an achievement (crafting.primeira_confeccao_xp_mult).
            "first_craft": bool(r.get("FirstCraftAchievementId")),
            "xp": int(ev.get("CategoricalProgressionReward") or 0), "char_xp": int(ev.get("UniversalExpAmount") or 0),
            # Bonus item chance (BonusItemChance* columns).
            "bonus": float(r.get("BonusItemChance") or 0),
            "inc": [float(x) for x in str(r.get("BonusItemChanceIncrease") or "").split(",") if x.strip()],
            "dec": [float(x) for x in str(r.get("BonusItemChanceDecrease") or "").split(",") if x.strip()],
            # Weapon, shield, tool (item with GS, quality and perks; docs/game/crafting.md): item tier and base GS.
            "gear": bool(crafted and CRAFT_GEAR.match(r["RecipeID"])),
            "item_tier": int(r.get("BaseTier") or 0),
            "gs_base": int(r.get("BaseGearScore") or 0),
        })
    st = stations()
    # Station name in the game: <Type>T<tier>_MasterName (e.g. SmeltingT5_MasterName = "Fundição Escala 5").
    for s in st:
        # Storage shed: ui_global_storage ("Despensa" / "Storage Shed").
        # Trading Post: ui_tradingpost ("Entreposto Comercial" / "Trading Post").
        key = {"storage": "ui_global_storage", "tradingpost": "ui_tradingpost",
               "cityboard": "ui_territory_board_town_project",
               "governance": "ui_territory_planning_board", "teleporter": "ui_fast_travel"}.get(s["type"]) or f"{s['type'].capitalize()}T{s['tier']}_MasterName".lower()
        s["name"] = {lang: loc.get(key, s["type"]) for lang, loc in locs.items()}
    out = {"stations": st, "recipes": recipes,
           "categories": {c: sorted(categories.get(c, [])) for c in sorted(used_cats)}}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(out, open(OUT, "w"), ensure_ascii=False, indent=1)
    # The port's shops (tools/shops.json); the items sold must be in the catalog (make_items.py).
    shops = json.load(open(os.path.join(ROOT, "tools/shops.json")))
    shops.pop("_comment", None)
    world = os.path.join(ROOT, "unreal/NWPort/Data/World")
    os.makedirs(world, exist_ok=True)
    json.dump(shops, open(os.path.join(world, "shops.json"), "w"), ensure_ascii=False, indent=1)
    # City governance: rules, price ranges and reference prices (nw-core reads the same files).
    for name in ("governanca.json", "precos_cidade.json", "precos_referencia.json"):
        data = json.load(open(os.path.join(ROOT, "tools", name)))
        json.dump(data, open(os.path.join(world, name), "w"), ensure_ascii=False, indent=1)
    by_skill = {}
    for r in recipes:
        by_skill[r["skill"]] = by_skill.get(r["skill"], 0) + 1
    per_city = {c: [s["type"] for s in st if s["city"] == c] for c in CITIES}
    print(f"{OUT}: {len(st)} stations {per_city}, recipes {by_skill}, "
          f"categories {[(c, len(v)) for c, v in out['categories'].items()]}")


if __name__ == "__main__":
    main()
