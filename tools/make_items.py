"""Catálogo de itens do servidor do clã, tirado das tabelas do jogo (subconjunto da F3).

    python3 tools/make_items.py

Entra: espadas, machadinhas e escudos de uma mão (tiers 1 a 5, sem nomeados/sazonais/teste), poções de vida e os
materiais que os Lost deixam. Lê extract/data (itemdefinitions_master_*, itemdefinitions_weapons,
itemdefinitions_consumables, statuseffects_item) e os textos de extract/loc/localization/<idioma>.

Também entram as ferramentas de coleta (picareta, foice, machado de lenhador, faca de esfolar, T1–T5) e todo item
alcançável pelo saque dos coletáveis de Guardaventos (Data/World/guardaventos_gatherables.json, de
tools/make_gatherables.py — rodar antes).

Gera unreal/NWPort/Data/Items/items.<idioma>.json (catálogo com nome e descrição), Data/Items/loot.json (saque
dos Lost), Data/Items/loottables.json (tabelas do jogo usadas pelos coletáveis) e extract/ui/item_textures.txt
(ícones a importar).
"""
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import LANGS, load_loc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "extract", "data")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Items")
ICONS = {os.path.basename(p)[:-4]: p for p in open(os.path.join(ROOT, "extract", "ui", "item_icons.txt")).read().split()
         if os.path.basename(p)[:-4]}

# Classe do jogo -> conjunto de arma do personagem (ANWCharacter::WeaponSets) ou escudo.
WEAPON_CLASSES = [("EquippableMainHand+Sword", "Sword"), ("EquippableMainHand+Hatchet", "Hatchet"), ("+Shield", "Shield")]
SKIP = re.compile(r"Dagger|_Shop|_FTUE|Covenant|Syndicate|Marauder|Winter|Named|Debug|Legacy|Crassus|Test|_S\d|Season|Artifact|Quest|Faction|PvP|Unique|Omega|Skin|AI_", re.I)
POTIONS = ["PotionHealthT5"]   # decisão do dono (2026-10-13): só a poção T5 existe no port
# Materiais básicos (minério, madeira, fibra, couro cru, pedra), tiers 1 e 2.
RESOURCES = ["OreT1", "WoodT1", "FiberT1", "RawhideT1", "StoneT1", "WoodT2"]
PER_TIER = 3   # armas por conjunto e tier no catálogo
# Ferramentas de coleta: conjunto (espaço de equipamento) e IDs do jogo.
TOOLS = [("Pick", ["2hPickT1", "2hPickT2", "2hPickT3", "2hPickT4", "2hPickT5"]),
         ("Sickle", ["1hSickleT1", "1hSickleT2", "1hSickleT3", "1hSickleT4", "1hSickleT5"]),
         ("LoggingAxe", ["1hHatchetT1", "2hAxeT2", "2hAxeT3", "2hAxeT4", "2hAxeT5"]),
         ("SkinningKnife", ["1hSkinningKnifeT1", "1hSkinningKnifeT2", "1hSkinningKnifeT3", "1hSkinningKnifeT4",
                            "1hSkinningKnifeT5"])]
# Itens de saque que não entram (partituras, pesquisa de temporada).
SKIP_LOOT = re.compile(r"_Page\d|^Gatherable_Research|_S\d+$")
GATHERABLES = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "guardaventos_gatherables.json")
# Receitas de refino (tools/make_crafting.py, rodar antes): saídas, ingredientes e itens das categorias entram no catálogo.
CRAFTING = os.path.join(ROOT, "unreal", "NWPort", "Data", "Crafting", "refining.json")


def rows(name):
    return json.load(open(glob.glob(os.path.join(DATA, "**", f"javelindata_{name}.datasheet.json"), recursive=True)[0]))["rows"]


# Aparência das armaduras (javelindata_itemappearancedefinitions): o ícone da armadura vem dela, não do item.
APPEARANCE = {}
# Espaço de equipamento do port pela classe do jogo (docs/armaduras.md).
ARMOR_SLOTS = (("EquippableHead", "Head"), ("EquippableChest", "Chest"), ("EquippableHands", "Hands"),
               ("EquippableLegs", "Legs"), ("EquippableFeet", "Feet"), ("EquippableAmulet", "Amulet"),
               ("EquippableRing", "Ring"), ("EquippableToken", "Earring"))


def icon_for(r):
    look = APPEARANCE.get(str(r.get("ArmorAppearanceM") or ""), {})
    for cand in (str(r.get("IconPath") or ""), str(look.get("IconPath") or ""), r["ItemID"]):
        base = os.path.splitext(os.path.basename(cand))[0].lower()
        if base and base in ICONS:
            return ICONS[base]
    return ""


def text(loc, key, fallback=""):
    key = str(key or "").lstrip("@").lower()
    t = loc.get(key, fallback)
    t = re.sub(r"<[^>]+>", "", t.replace("\\n", "\n")).strip()
    # Expressões que o jogo calcula ao mostrar: "{[0.100000 * 100]}" -> "10" (só números e + - * / ( )).
    def calc(m):
        expr = m.group(1)
        if not re.fullmatch(r"[\d.\s+\-*/()]+", expr):
            return m.group(0)
        v = eval(expr, {"__builtins__": {}})
        return f"{v:.2f}".rstrip("0").rstrip(".")
    return re.sub(r"\{\[([^\]]+)\]\}", calc, t)


def loot_tables(items, roots):
    """Tabelas do jogo alcançáveis a partir de roots, no formato do C++ (FNWItemCatalog::RollGame).

    Cada tabela: and (todos os itens com limiar <= dado) ou or (um sorteado entre eles), max (dado 0..max) e
    entradas {item|table, q: [mín, máx], t: limiar}. Tabelas com Conditions (temporada, PvP, eventos) e
    baldes [LBID] ficam de fora.
    """
    lt = rows("loottables")
    by_id = {r["LootTableID"]: (r, lt[i + 1], lt[i + 2]) for i, r in enumerate(lt)
             if not r["LootTableID"].endswith(("_Qty", "_Probs")) and i + 2 < len(lt)}
    out, used = {}, set()

    def qty(v):
        a, _, b = str(v or "1").partition("-")
        return [int(float(a)), int(float(b or a))]

    def walk(t):
        if t in out:
            return out[t] is None or bool(out[t])  # None = em andamento (ciclo)
        if t not in by_id or by_id[t][0].get("Conditions"):
            return False
        r, q, p = by_id[t]
        out[t] = None
        entries = []
        for k, v in r.items():
            if not k.startswith("Item") or not v:
                continue
            e = {"q": qty(q.get(k)), "t": int(float(p.get(k) or 0))}
            if v.startswith("[LTID]"):
                if not walk(v[6:]):
                    continue
                e["table"] = v[6:]
            elif v.startswith("[LBID]") or v not in items or SKIP_LOOT.search(v):
                continue
            else:
                e["item"] = v
                used.add(v)
            entries.append(e)
        # RollBonusSetting: AddToRoll soma o bônus (sorte, nível do ofício) ao dado; ClampMax soma até o MaxRoll;
        # IgnoreBonus/None não usam.
        bonus = {"AddToRoll": "add", "ClampMax": "clamp"}.get(str(r.get("RollBonusSetting")), "")
        out[t] = {"and": r.get("AND/OR") == "AND", "max": int(float(p.get("MaxRoll") or 0)), "e": entries,
                  **({"bonus": bonus} if bonus else {})} if entries else {}
        return bool(entries)

    for t in roots:
        walk(t)
    return {k: v for k, v in out.items() if v}, used


def main():
    items, weights = {}, {}
    for name in ("itemdefinitions_master_common", "itemdefinitions_master_crafting", "itemdefinitions_master_loot"):
        for r in rows(name):
            items.setdefault(r["ItemID"], r)
            if r.get("Weight") not in (None, ""):
                weights.setdefault(r["ItemID"], float(r["Weight"]) / 10)  # o jogo guarda em décimos
    weapons = {r["WeaponID"]: r for r in rows("itemdefinitions_weapons")}
    APPEARANCE.update({r["ItemID"]: r for r in rows("itemappearancedefinitions")})
    consumables = {r["ConsumableID"]: r for r in rows("itemdefinitions_consumables")}
    effects = {r["StatusID"]: r for r in rows("statuseffects_item")}

    chosen = []
    for cls, weapon_set in WEAPON_CLASSES:
        found = []
        for r in items.values():
            if r.get("ItemType") != "Weapon" or cls not in str(r.get("ItemClass")) or SKIP.search(r["ItemID"] + str(r.get("ItemClass"))):
                continue
            if not 1 <= int(r.get("Tier") or 0) <= 5 or not icon_for(r) or not str(r.get("Name") or "").startswith("@"):
                continue
            found.append(r)
        # Os mais "básicos" primeiro (IDs curtos: 1hSwordT2 antes de 1hSwordDropT2_Variant).
        found.sort(key=lambda r: (int(r["Tier"]), len(r["ItemID"]), r["ItemID"]))
        per_tier = {}
        for r in found:
            t = int(r["Tier"])
            if per_tier.get(t, 0) < PER_TIER:
                per_tier[t] = per_tier.get(t, 0) + 1
                chosen.append((r, weapon_set))
    for weapon_set, ids in TOOLS:
        for tid in ids:
            chosen.append((items[tid], weapon_set))
    for pid in POTIONS:
        chosen.append((items[pid], ""))
    for rid in RESOURCES:
        if rid in items:
            chosen.append((items[rid], ""))
        else:
            print("material ausente:", rid)

    gv = json.load(open(GATHERABLES))["variants"] if os.path.exists(GATHERABLES) else {}
    # Saque das carcaças das criaturas (esfolar; tools/make_spawns.py).
    spawns = os.path.join(os.path.dirname(GATHERABLES), "guardaventos_spawns.json")
    skins = {c["skin"]["loot"] for c in json.load(open(spawns))["creatures"].values() if c.get("skin")} if os.path.exists(spawns) else set()
    game_loot, loot_items = loot_tables(items, sorted({v["loot"] for v in gv.values() if v["loot"]} | skins))
    have = {r["ItemID"] for r, _ in chosen}
    for iid in sorted(loot_items - have):
        chosen.append((items[iid], ""))
    if os.path.exists(CRAFTING):
        cr = json.load(open(CRAFTING))
        need = {r["out"] for r in cr["recipes"]} | {i[0] for r in cr["recipes"] for i in r["ing"] if not i[2]}
        need |= {i for members in cr["categories"].values() for i in members}
        have = {r["ItemID"] for r, _ in chosen}
        for iid in sorted(need - have):
            if iid in items:
                chosen.append((items[iid], ""))
            else:
                print("item de receita ausente:", iid)

    catalog = []
    for r, weapon_set in chosen:
        e = {
            "id": r["ItemID"], "type": str(r.get("ItemType")).lower(), "set": weapon_set, "tier": int(r.get("Tier") or 0),
            "stack": int(r.get("MaxStackSize") or 1), "icon": icon_for(r),
            "gs_min": int(r.get("MinGearScore") or 0), "gs_max": int(r.get("MaxGearScore") or 0),
            "name_key": str(r.get("Name") or ""), "desc_key": str(r.get("Description") or ""),
            "type_key": str(r.get("ItemTypeDisplayName") or ""),
            "weight": weights.get(r["ItemID"], 0.0),
            # Seção do inventário (dynamicitemlist.lua: eItemClass_UI_<classe>) e forma da peça (itemlayout.lua
            # SetLayoutByItemType: redonda para Resource, Lore, Dye, HousingItem; quadrada no resto).
            "ui_class": str(r.get("UiItemClass") or "").removeprefix("UI_"),
            "round": str(r.get("ItemType")) in ("Resource", "Lore", "Dye", "HousingItem"),
        }
        if r.get("ItemType") == "Armor":
            cls = str(r.get("ItemClass"))
            e["set"] = next((slot for key, slot in ARMOR_SLOTS if key in cls), weapon_set)
            # Classe de peso da peça (Heavy, Medium, Light); joias não têm.
            e["armor"] = next((w for w in ("Heavy", "Medium", "Light") if w in cls.split("+")), "")
        if r.get("ItemType") == "Weapon":
            w = weapons.get(r.get("ItemStatsRef"), {})
            e["damage"] = float(w.get("BaseDamage") or 0)
            if w.get("GatheringTypes") not in (None, "", "None"):
                e["gather"] = str(w["GatheringTypes"])
                e["efficiency"] = float(w.get("GatheringEfficiency") or 1)
        if r["ItemID"] in consumables:
            eff = effects.get(consumables[r["ItemID"]].get("AddStatusEffects"), {})
            e["heal"] = float(eff.get("Health") or 0)
            e["heal_pct"] = float(eff.get("HealthModifierPercent") or 0)
            e["cooldown"] = float(consumables[r["ItemID"]].get("CooldownDuration") or 0)
        catalog.append(e)

    os.makedirs(OUT, exist_ok=True)
    for lang in LANGS:
        loc = load_loc(lang)
        out = [dict(e, name=text(loc, e["name_key"], e["id"]), description=text(loc, e["desc_key"]),
                    type_name=text(loc, e["type_key"])) for e in catalog]
        json.dump(out, open(os.path.join(OUT, f"items.{lang}.json"), "w"), ensure_ascii=False, indent=1)

    # Saque dos Lost: moedas sempre, 1-2 materiais tier 1, poção às vezes, arma tier 2-3 rara.
    tier = lambda e, t: e["tier"] == t
    loot = {
        "lost": {
            "coins": [5, 25],
            "rolls": [
                {"chance": 1.0, "pick": [e["id"] for e in catalog if e["type"] == "resource" and tier(e, 1)], "qty": [1, 3]},
                {"chance": 0.5, "pick": [e["id"] for e in catalog if e["type"] == "resource" and tier(e, 1)], "qty": [1, 2]},
                {"chance": 0.25, "pick": ["PotionHealthT5"], "qty": [1, 1]},
                {"chance": 0.08, "pick": [e["id"] for e in catalog if e["type"] == "weapon" and e["tier"] in (2, 3)], "qty": [1, 1]},
                {"chance": 0.02, "pick": [e["id"] for e in catalog if e["type"] == "weapon" and e["tier"] in (4, 5)], "qty": [1, 1]},
            ],
        }
    }
    json.dump(loot, open(os.path.join(OUT, "loot.json"), "w"), indent=1)
    json.dump(game_loot, open(os.path.join(OUT, "loottables.json"), "w"), separators=(",", ":"))
    open(os.path.join(ROOT, "extract", "ui", "item_textures.txt"), "w").write("\n".join(sorted({e["icon"] for e in catalog})))
    counts = {}
    for e in catalog:
        counts[e["set"] or e["type"]] = counts.get(e["set"] or e["type"], 0) + 1
    print(f"{len(catalog)} itens {counts}, {len(game_loot)} tabelas de saque do jogo -> {OUT}")


if __name__ == "__main__":
    main()
