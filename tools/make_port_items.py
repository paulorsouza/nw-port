#!/usr/bin/env python3
"""The port's item rules -> unreal/NWPort/Data/Items/regras_itens.json (docs/game/items.md, docs/game/crafting.md).

    python3 tools/make_port_items.py

Sources:
- tools/itens.json: the port's rules and new perks (provisional balance numbers);
- tools/perks_decisoes.json: decision for each game perk (the Corte de Perks page); the kept ones come in with the
  game's name and effect (javelindata_perks, pt-br/en-us strings, values with perkMultiplier 1);
- ability perks are per weapon (PerkID_Ability_<Weapon>_*), only on the weapon of that ability.
"""
import ast
import glob
import json
import operator
import os
import re
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "Items", "regras_itens.json")
LANGS = ("pt-br", "en-us")
# Ability weapon in the game id -> set in the port catalog (empty: weapon with no set in the port yet).
WEAPON = {"Sword": "Sword", "Hatchet": "Hatchet", "Shield": "Shield", "Bow": "Bow", "Rapier": "Rapier",
          "Spear": "Spear", "GreatAxe": "GreatAxe", "Greatsword": "GreatSword", "Hammer": "WarHammer",
          "Musket": "Musket", "Blunderbuss": "Blunderbuss", "FireStaff": "FireStaff", "LifeStaff": "LifeStaff",
          "IceGauntlet": "IceGauntlet", "VoidGauntlet": "VoidGauntlet", "Flail": "Flail"}


def load_loc(lang):
    out = {}
    for f in glob.glob(os.path.join(ROOT, "extract", "loc", "localization", lang, "*.loc.xml")):
        try:
            for s in ET.parse(f).getroot().iter("string"):
                out[s.get("key", "").lower()] = s.text or ""
        except ET.ParseError:
            continue
    return out


_OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: operator.truediv,
        ast.USub: operator.neg, ast.UAdd: operator.pos}


def _eval(n):
    if isinstance(n, ast.Expression):
        return _eval(n.body)
    if isinstance(n, ast.Constant):
        return n.value
    if isinstance(n, ast.BinOp):
        return _OPS[type(n.op)](_eval(n.left), _eval(n.right))
    if isinstance(n, ast.UnaryOp):
        return _OPS[type(n.op)](_eval(n.operand))
    raise ValueError


def text(loc, key):
    """Game text with the {[...]} expressions resolved at multiplier 1."""
    t = re.sub(r"<[^>]+>", "", loc.get(str(key or "").lstrip("@").lower(), "")).replace("\xa0", " ")

    def rep(m):
        expr = m.group(1).replace("{perkMultiplier}", "1").replace("[", "").replace("]", "")
        try:
            v = _eval(ast.parse(expr, mode="eval"))
            return ("%.2f" % v).rstrip("0").rstrip(".")
        except Exception:
            return m.group(0)
    # Expression between {[ and ]}, which may contain brackets and {perkMultiplier}.
    return re.sub(r"\{\[((?:[^{}]|\{perkMultiplier\})*?)\]\}", rep, t).strip()


def gem_items(rules):
    """Gem and runeglass item -> class, effects (%) and raw gem of the removal cost (docs/game/items.md "Slot and gems")."""
    game = {}
    for f in glob.glob(os.path.join(DT, "javelindata_itemdefinitions_master_*.datasheet.json")):
        for r in json.load(open(f))["rows"]:
            game.setdefault(r["ItemID"], r)
    pct = {int(k): v for k, v in rules["gema_pct_por_tier"].items()}
    out = {}
    for gem, g in rules["gemas"].items():
        for cut in range(1, 5):
            iid = f"{gem}CutT{cut}"
            if iid not in game:
                raise SystemExit(f"gem without an item in the game: {iid}")
            total = pct[int(game[iid]["Tier"])]
            out[iid] = {"classe": g["classe"], "bruta": f"{gem}T{cut}",
                        "efeitos": {e: round(total * f, 2) for e, f in g["efeito"].items()}}
        pure = out[f"{gem}CutT4"]
        for case, amp in rules["vidro_capsulas"].items():
            iid = f"Runeglass_Gem_{gem}_{case}"
            if iid not in game:
                raise SystemExit(f"runeglass without an item in the game: {iid}")
            out[iid] = {"classe": g["classe"], "bruta": pure["bruta"], "vidro": True,
                        "efeitos": {**pure["efeitos"], amp: rules["vidro_amplificacao"]}}
    return out


def main():
    rules = json.load(open(os.path.join(ROOT, "tools", "itens.json")))
    rules.pop("_comment", None)
    decisions = json.load(open(os.path.join(ROOT, "tools", "perks_decisoes.json")))["decisoes"]
    game = {p["PerkID"]: p for p in json.load(open(os.path.join(DT, "javelindata_perks.datasheet.json")))["rows"]}
    locs = {lang: load_loc(lang) for lang in LANGS}

    perks = rules["perks"]
    ability = {}
    for pid, d in sorted(decisions.items()):
        if d.get("v") != "fica" or pid not in game:
            continue
        p = game[pid]
        entry = {"nome": {lang: re.sub(r"<[^>]+>", "", locs[lang].get(p["DisplayName"].lstrip("@").lower(), pid)) for lang in LANGS},
                 "efeito": {lang: text(locs[lang], p.get("Description")) for lang in LANGS},
                 "icone": str(p.get("IconPath") or "").replace("\\", "/").lower().replace(".png", ".dds")}
        if d.get("nota"):
            entry["nota"] = d["nota"]
        perks[pid] = entry
        m = re.match(r"PerkID_Ability_([A-Za-z]+)_", pid)
        if m:
            ability.setdefault(WEAPON.get(m.group(1), m.group(1)), []).append(pid)
    rules["habilidade"] = ability
    rules["gema_itens"] = gem_items(rules)
    missing = [p for pool in rules["equipamento"].values() for p in pool if p not in perks]
    if missing:
        raise SystemExit(f"equipment perks without a definition: {missing}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(rules, open(OUT, "w"), ensure_ascii=False, indent=1)
    print(f"{OUT}: {len(perks)} perks, ability perks per weapon {[(k, len(v)) for k, v in sorted(ability.items())]}")


if __name__ == "__main__":
    main()
