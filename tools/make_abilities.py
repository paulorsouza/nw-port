"""Habilidades por arma para a HUD e a tela de maestria (dados do jogo, textos em pt-BR e en-US).

    python3 tools/make_abilities.py

Lê extract/data (javelindata_ability_<arma>, javelindata_cooldowns_player) e extract/loc/localization/<idioma>,
grava unreal/NWPort/Data/UI/abilities.<idioma>.json. Na descrição, a marcação do jogo vira texto rico do Slate:
destaque dourado (<font color="#edc63e">) -> <hl>..</>, nota em itálico cinza -> <dim>..</>; {[expr]} é calculado.
"""
import glob
import html
import json
import os
import re
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
LOC_ROOT = os.path.join(ROOT, "extract", "loc", "localization")
LANGS = ["pt-br", "en-us"]
DECIMAL = {"pt-br": ",", "en-us": "."}
lang = "pt-br"   # idioma em geração (calc usa o separador decimal dele)
ICONS = "lyshineui/images/icons/abilities/"

# Habilidade do jogo -> ação da tabela DT_NWActions.
WEAPONS = {
    "sword": {
        "name_key": "ui_straightsword_mastery", "trees": ["ui_sword", "ui_shield"],
        "actions": {
            "Ability_Sword_ReverseStab": "SwipeStab",
            "Ability_Sword_WhirlingBlade": "SpinAttack",
            "Ability_Sword_LeapingStrike": "LeapingStrike",
            "Ability_Sword_Bash": "ShieldBash",
            "Ability_Sword_Rush": "ShieldRush",
            "Ability_Sword_FinalStand": "FinalStand",
        },
        "default": ["ShieldBash", "SpinAttack", "SwipeStab"],
        "hud_icon": "lyshineui/images/icons/items/drawing/1hsword.dds",
    },
    "hatchet": {
        "name_key": "ui_hatchet_mastery", "trees": ["ui_hatchet_tree1", "ui_hatchet_tree2"],
        "actions": {f"Ability_Hatchet_{a}": a for a in
                    ("Berserk", "FeralRush", "RagingTorrent", "RendingThrow", "SocialDistancing", "InfectedThrow")},
        "default": ["Berserk", "RagingTorrent", "RendingThrow"],
        "hud_icon": "lyshineui/images/icons/items/drawing/1hhatchet.dds",
    },
    "bow": {
        "name_key": "ui_bow_mastery", "trees": ["ui_bow_tree1", "ui_bow_tree2"],
        "actions": {"Ability_Bow_RapidShot": "RapidShot", "Ability_Bow_EvadeShot": "EvadeShot",
                    "Ability_Bow_PenetratingShot": "PenetratingShot", "Ability_Bow_PoisonShot": "PoisonShot",
                    "Ability_Bow_SplinterShot": "ExplosiveArrow", "Ability_Bow_RainofArrows": "RainOfArrows"},
        "default": ["RapidShot", "EvadeShot", "PenetratingShot"],
        "hud_icon": "lyshineui/images/icons/items/drawing/bowa.dds",
    },
    "rapier": {
        "name_key": "ui_rapier_mastery", "trees": ["ui_blood", "ui_evade"],
        "actions": {"Ability_Rapier_Slash": "Tondo", "Ability_Rapier_Flurry": "Flurry", "Ability_Rapier_Flourish": "Flourish",
                    "Ability_Rapier_Riposte": "Riposte", "Ability_Rapier_Aerial_Lunge_Intro": "Fleche",
                    "Ability_Rapier_Evasion": "Evade"},
        "default": ["Tondo", "Riposte", "Evade"],
        "hud_icon": "lyshineui/images/icons/items/drawing/1hrapier.dds",
    },
}
# Fundo do ícone pela categoria (AbilitiesCommon.backgroundPathByCategory do jogo).
BACKGROUND = {"Attack": "abilities_bg2.dds", "Defense": "abilities_bg3.dds"}


def load_loc(language):
    out = {}
    for f in glob.glob(os.path.join(LOC_ROOT, language, "*.loc.xml")):
        try:
            for s in ET.parse(f).getroot().iter("string"):
                out[s.get("key", "").lower()] = s.text or ""
        except ET.ParseError:
            continue
    return out


def rows(name):
    f = glob.glob(os.path.join(DT, "**", name + ".datasheet.json"), recursive=True)[0]
    return json.load(open(f))["rows"]


def calc(m):
    expr = m.group(1)
    if not re.fullmatch(r"[0-9.\s*/+\-()]+", expr):
        return expr
    v = eval(expr)  # só números e operadores (conferido acima)
    return str(int(round(v))) if abs(v - round(v)) < 1e-6 else f"{v:.1f}".replace(".", DECIMAL[lang])


def rich(text):
    t = text.replace("\\n", "\n")
    t = re.sub(r"\{\[(.*?)\]\}", calc, t)
    t = re.sub(r"<img[^>]*>(</img>)?", "", t)
    t = re.sub(r'<font[^>]*color="#edc63e"[^>]*>(.*?)</font>', r"<hl>\1</>", t, flags=re.S)
    t = re.sub(r"<font[^>]*>(.*?)</font>", r"<dim>\1</>", t, flags=re.S)
    t = re.sub(r"<(?!hl>|dim>|/>)[^>]+>", "", t)
    return html.unescape(t).strip()


def build(language):
    global lang
    lang = language
    loc = load_loc(language)
    cooldown = {r["AbilityID"]: r["Time"] for r in rows("javelindata_cooldowns_player")}
    out = {}
    for weapon, cfg in WEAPONS.items():
        entries = []
        for r in rows(f"javelindata_ability_{weapon}"):
            aid = r["AbilityID"]
            if aid not in cfg["actions"]:
                continue
            icon = ICONS + os.path.basename(r["Icon"]).lower().replace(".png", ".dds")
            entries.append({
                "action": cfg["actions"][aid], "ability_id": aid, "tree": int(r.get("TreeId") or 0),
                # Chaves de texto nas colunas do datasheet (@chave); a espada usa o próprio AbilityID.
                "name": rich(loc.get(str(r.get("DisplayName", "")).lstrip("@").lower(), loc.get(aid.lower(), aid))),
                "description": rich(loc.get(str(r.get("Description", "")).lstrip("@").lower(), loc.get(aid.lower() + "_description", ""))),
                "category": r.get("UICategory", ""), "icon": icon,
                "background": ICONS + BACKGROUND.get(r.get("UICategory", ""), "abilities_bg1.dds"),
                "cooldown": cooldown.get(aid, 0),
            })
        # Árvore inteira para a tela de maestria: ativas, melhorias, passivas e ultimate, com linha/coluna do jogo.
        nodes = []
        all_rows = rows(f"javelindata_ability_{weapon}")
        for r in all_rows:
            cat = r.get("UICategory") or ""
            if cat in ("", "None") or r.get("TreeRowPosition") in (None, ""):
                continue
            aid = r["AbilityID"]
            kind = "active" if aid in cfg["actions"] else "ultimate" if aid.startswith("Ultimate_") else "passive"
            icon = ICONS + os.path.basename(str(r.get("Icon") or "")).lower().replace(".png", ".dds")
            nodes.append({
                "id": aid, "action": cfg["actions"].get(aid, ""), "kind": kind, "tree": int(r.get("TreeId") or 0),
                "row": int(r["TreeRowPosition"]), "col": int(r["TreeColumnPosition"]), "category": cat, "icon": icon,
                "background": ICONS + BACKGROUND.get(cat, "abilities_bg1.dds") if kind == "active" else "",
                "name": rich(loc.get(str(r.get("DisplayName", "")).lstrip("@").lower(), loc.get(aid.lower(), aid))),
                "description": rich(loc.get(str(r.get("Description", "")).lstrip("@").lower(), loc.get(aid.lower() + "_description", ""))),
                "cooldown": cooldown.get(aid, 0),
            })
        # Melhoria liga na habilidade de cima, na mesma coluna (setas da árvore).
        for n in nodes:
            above = [m for m in nodes if m["tree"] == n["tree"] and m["col"] == n["col"] and m["row"] < n["row"]]
            n["parent"] = max(above, key=lambda m: m["row"])["id"] if n["id"].startswith("Upgrade_") and above else ""
        out[weapon] = {"title": loc.get(cfg["name_key"], weapon), "trees": [loc.get(k, k) for k in cfg["trees"]],
                       "abilities": entries, "default": cfg["default"], "nodes": nodes,
                       "hud_icon": cfg.get("hud_icon", "")}
        print(language, weapon, [(e["action"], e["name"]) for e in entries])
    dst = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"abilities.{language}.json")
    json.dump(out, open(dst, "w"), ensure_ascii=False, indent=1)
    print("->", dst)


def main():
    for language in LANGS:
        build(language)


if __name__ == "__main__":
    main()
