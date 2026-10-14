#!/usr/bin/env python3
"""Estações de refino de Queda Eterna e receitas de refino do jogo -> unreal/NWPort/Data/Crafting/refining.json.

    python3 tools/make_crafting.py        (antes de tools/make_items.py: o catálogo inclui os itens das receitas)

Fontes:
- estações: PrefabSpawnerComponent do slice da cidade (extract/slicejson, etapa sistemas-02 do build_all), slice
  `Settlement_Station_<Tipo>_T5` / `settlement_station_<tipo>_master`, posição em m_worldtm (referencial da cidade);
- receitas: javelindata_crafting_refining, ofícios de refino (Smelting, Woodworking, Leatherworking, Weaving,
  Stonecutting), conhecidas por padrão (bKnownByDefault), sem a estação de runas (RuneglassStation1);
- categorias de ingrediente (Type = Category_Only): coluna IngredientCategories das javelindata_itemdefinitions_master_*;
- XP de ofício por receita: javelindata_gameevents (GameEventID -> CategoricalProgressionReward) e XP de personagem
  (UniversalExpAmount).

Tempo de refino: o jogo não tem (docs/sistemas.md, "Sem fonte"); o port refina na hora.
"""
import glob
import json
import xml.etree.ElementTree as ET
import math
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
CITY = os.path.join(ROOT, "extract", "slicejson", "slices", "structures", "settlements", "v2_settlements",
                    "settlement_northwindsward_complete.dynamicslice.json")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Crafting", "refining.json")
# Tipo da estação no slice -> StationType das receitas e ofício. As despensas (settlement_storage_*) entram como
# estações do tipo "storage" (armazém da cidade).
STATIONS = {"smelting": ("smelting", "Smelting"), "carpentry": ("carpentry", "Woodworking"),
            "tanning": ("tanning", "Leatherworking"), "weaving": ("weaving", "Weaving"),
            "masonry": ("masonry", "Stonecutting"),
            # Mesas de fabricar (docs/oficio.md): ferraria, engenharia e alquimia. Cozinha e alfaiataria entram quando
            # houver comida com efeito e armadura.
            "blacksmithing": ("blacksmith", "Weaponsmithing"), "engineering": ("engineering", "Engineering"),
            "alchemy": ("alchemy", "Arcana"),
            # Joalheria (gemas lapidadas; docs/itens.md "Slot"). A armoraria entra na mesma estação com as armaduras.
            "outfitting": ("outfitting", "Jewelcrafting")}
# Receitas de fabricar do port (docs/oficio.md): armas, escudos e ferramentas T2–T5, poção de vida T5 e reagentes de
# alquimia T2.
CRAFT_SOURCES = ("javelindata_crafting_weapon", "javelindata_crafting_engineer", "javelindata_crafting_arcana",
                 "javelindata_crafting_jeweler")
CRAFT_GEAR = re.compile(r"^Procedural_(Longsword|ShieldRound|ShieldKite|ShieldTower|Hatchet|LoggingAxe|MiningPick|"
                        r"SkinningKnife|Sickle)T[2-5]$")
CRAFT_INSTANT = re.compile(r"^(Procedural_PotionHealthT5|Alchemy[A-Za-z]+T2|"
                           r"(Onyx|Opal|Diamond|Sapphire|Jasper|Emerald|Malachite)CutT[1-4]|"
                           r"Runeglass_Case_Empty_(Melee|Arcane))$")
# Vidro rúnico (docs/itens.md "Slot"): o jogo usa a estação de runas (RuneglassStation1), que Queda Eterna não tem. No
# port a cápsula sai da forja (Weaponsmithing) e o vidro (cápsula + gema Pura) da cantaria (Stonecutting), tier 5.
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
    """Estações de refino do slice: tipo, tier e posição no mapa (UE = (-x, y, z) * 100, como make_npcs.py)."""
    out = []
    data = json.load(open(CITY))

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
                if tm and "settlement_storage" in hint.lower():
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))
                    out.append({"type": "storage", "skill": "", "tier": 0,
                                "pos": [round(-tm[9] * 100, 1), round(tm[10] * 100, 1), round(tm[11] * 100, 1)],
                                "yaw": round(180 - yaw, 1), "slice": os.path.basename(hint)})
                m = re.search(r"settlement_station_([a-z]+)_(t\d|master)", hint.lower())
                if tm and m and m.group(1) in STATIONS:
                    x, y, z = tm[9], tm[10], tm[11]
                    yaw = math.degrees(math.atan2(tm[1], tm[0]))   # coluna X da rotação
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
    skills = {s for _, s in STATIONS.values()}
    events = {r["EventID"]: r for r in rows("javelindata_gameevents")}
    categories = {}
    for f in glob.glob(os.path.join(DT, "javelindata_itemdefinitions_master_*.datasheet.json")):
        for r in json.load(open(f))["rows"]:
            for c in str(r.get("IngredientCategories") or "").split(","):
                if c.strip():
                    categories.setdefault(c.strip(), set()).add(r["ItemID"])

    # Nomes da árvore da tela (família e grupo): DisplayText de javelindata_craftingcategories, senão <Grupo>_GroupName.
    locs = {lang: load_loc(lang) for lang in ("pt-br", "en-us")}
    cat_text = {r["CategoryID"]: str(r.get("DisplayText") or "") for r in rows("javelindata_craftingcategories")}

    def tree_name(cid):
        keys = [cat_text.get(cid, "").lstrip("@").lower(), f"{cid}_groupname".lower(), f"{cid}_categoryname".lower()]
        return {lang: next((loc[k] for k in keys if k and k in loc), cid) for lang, loc in locs.items()}

    # Ordem da árvore: CategorySortOrder e GroupSortOrder de lyshineui/crafting/craftingsortinfo.lua; o que não está
    # na lista vem depois (craftingtreeview.lua ordena por sortingIndex, sem índice por último).
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
            continue   # variantes de evento/conquista (ex. CharcoalT1_WoodenCoin)
        if r["RecipeID"].startswith("TerritoryStandingSupplies"):
            continue   # suprimentos de projeto do território (pedem azoth), não refino
        ing = []
        gear = crafted and CRAFT_GEAR.match(r["RecipeID"])
        for i in range(1, 8):
            iid, qty = r.get(f"Ingredient{i}"), r.get(f"Qty{i}")
            if not iid or not qty:
                continue
            cat = r.get(f"Type{i}") == "Category_Only"
            if gear and cat:
                # Fabricar no port (docs/oficio.md): só material refinado, um por espaço, do tier do item (o jogo deixa
                # escolher entre vários da categoria, inclusive raros).
                tier = int(r.get("BaseTier") or 2)
                iid = {"Timber": f"TimberT{tier}", "Leather": f"LeatherT{tier}"}.get(iid, re.sub(r"^MetalT(\d)$", r"IngotT\1", iid))
                cat = False
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
            # Bônus de primeira confecção só para receita com conquista (crafting.primeira_confeccao_xp_mult).
            "first_craft": bool(r.get("FirstCraftAchievementId")),
            "xp": int(ev.get("CategoricalProgressionReward") or 0), "char_xp": int(ev.get("UniversalExpAmount") or 0),
            # Chance de item extra (docs/analise-re/crafting-refino.md, ids crafting.bonus_*).
            "bonus": float(r.get("BonusItemChance") or 0),
            "inc": [float(x) for x in str(r.get("BonusItemChanceIncrease") or "").split(",") if x.strip()],
            "dec": [float(x) for x in str(r.get("BonusItemChanceDecrease") or "").split(",") if x.strip()],
            # Arma, escudo, ferramenta (item com GS, qualidade e perks; docs/oficio.md): tier do item e GS base.
            "gear": bool(crafted and CRAFT_GEAR.match(r["RecipeID"])),
            "item_tier": int(r.get("BaseTier") or 0),
            "gs_base": int(r.get("BaseGearScore") or 0),
        })
    st = stations()
    # Nome da estação no jogo: <Tipo>T<tier>_MasterName (ex. SmeltingT5_MasterName = "Fundição Escala 5").
    for s in st:
        # Despensa: ui_global_storage ("Despensa" / "Storage Shed").
        key = "ui_global_storage" if s["type"] == "storage" else f"{s['type'].capitalize()}T{s['tier']}_MasterName".lower()
        s["name"] = {lang: loc.get(key, s["type"]) for lang, loc in locs.items()}
    out = {"stations": st, "recipes": recipes,
           "categories": {c: sorted(categories.get(c, [])) for c in sorted(used_cats)}}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(out, open(OUT, "w"), ensure_ascii=False, indent=1)
    # Lojas do port (tools/shops.json); os itens vendidos têm que estar no catálogo (make_items.py).
    shops = json.load(open(os.path.join(ROOT, "tools/shops.json")))
    shops.pop("_comment", None)
    world = os.path.join(ROOT, "unreal/NWPort/Data/World")
    os.makedirs(world, exist_ok=True)
    json.dump(shops, open(os.path.join(world, "shops.json"), "w"), ensure_ascii=False, indent=1)
    by_skill = {}
    for r in recipes:
        by_skill[r["skill"]] = by_skill.get(r["skill"], 0) + 1
    print(f"{OUT}: {len(st)} estações {[s['type'] for s in st]}, receitas {by_skill}, "
          f"categorias {[(c, len(v)) for c, v in out['categories'].items()]}")


if __name__ == "__main__":
    main()
