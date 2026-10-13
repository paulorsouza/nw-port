#!/usr/bin/env python3
"""Regras de item do port -> unreal/NWPort/Data/Items/regras_itens.json (docs/itens.md, docs/oficio.md).

    python3 tools/make_itens.py

Fontes:
- tools/itens.json: regras e perks novas do port (números de balanceamento provisórios);
- tools/perks_decisoes.json: decisão de cada perk do jogo (página Corte de Perks); as que ficam entram com nome e
  efeito do jogo (javelindata_perks, textos pt-br/en-us, valores com perkMultiplier 1);
- perks de habilidade ficam por arma (PerkID_Ability_<Arma>_*), só na arma daquela habilidade.
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
# Arma da habilidade no id do jogo -> conjunto do catálogo do port (vazio: arma ainda sem conjunto no port).
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
    """Texto do jogo com as expressões {[...]} resolvidas no multiplicador 1."""
    t = re.sub(r"<[^>]+>", "", loc.get(str(key or "").lstrip("@").lower(), "")).replace("\xa0", " ")

    def rep(m):
        expr = m.group(1).replace("{perkMultiplier}", "1").replace("[", "").replace("]", "")
        try:
            v = _eval(ast.parse(expr, mode="eval"))
            return ("%.2f" % v).rstrip("0").rstrip(".")
        except Exception:
            return m.group(0)
    # Expressão entre {[ e ]}, que pode ter colchetes e {perkMultiplier} dentro.
    return re.sub(r"\{\[((?:[^{}]|\{perkMultiplier\})*?)\]\}", rep, t).strip()


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
    missing = [p for pool in rules["equipamento"].values() for p in pool if p not in perks]
    if missing:
        raise SystemExit(f"perks do equipamento sem definição: {missing}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(rules, open(OUT, "w"), ensure_ascii=False, indent=1)
    print(f"{OUT}: {len(perks)} perks, habilidade por arma {[(k, len(v)) for k, v in sorted(ability.items())]}")


if __name__ == "__main__":
    main()
