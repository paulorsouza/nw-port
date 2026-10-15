"""Abilities per weapon for the HUD and the Weapon Mastery screen (game data, text in pt-BR and en-US).

    python3 tools/make_abilities.py

Reads extract/data (javelindata_ability_<weapon>, javelindata_cooldowns_player) and extract/loc/localization/<language>,
writes unreal/NWPort/Data/UI/abilities.<language>.json. In the description, the game markup becomes Slate rich text:
gold highlight (<font color="#edc63e">) -> <hl>..</>, grey italic note -> <dim>..</>; {[expr]} is evaluated.
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
lang = "pt-br"   # language being generated (calc uses its decimal separator)
ICONS = "lyshineui/images/icons/abilities/"

# Game ability -> action in the DT_NWActions table.
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
    # Ice Gauntlet (sub-area 1a): table javelindata_ability_icemagic; ability actions in part 2.
    "icegauntlet": {
        "table": "icemagic", "name_key": "ui_icemagic_mastery", "trees": ["ui_icemagic_tree1", "ui_icemagic_tree2"],
        "actions": {"IceMagic_Spikes": "Spikes", "IceMagic_IceStorm": "IceStorm", "IceMagic_WindChill": "WindChill",
                    "IceMagic_IcePylon": "IcePylon", "IceMagic_IceShower": "IceShower", "IceMagic_Entombed": "Entombed"},
        "default": ["IceStorm", "WindChill", "Spikes"],
        "hud_icon": "lyshineui/images/icons/items/drawing/icegauntlet.dds",
    },
}
# Icon background by category (the game's AbilitiesCommon.backgroundPathByCategory).
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
    v = eval(expr)  # only numbers and operators (checked above)
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
        for r in rows(f"javelindata_ability_{cfg.get('table', weapon)}"):
            aid = r["AbilityID"]
            if aid not in cfg["actions"]:
                continue
            icon = ICONS + os.path.basename(r["Icon"]).lower().replace(".png", ".dds")
            entries.append({
                "action": cfg["actions"][aid], "ability_id": aid, "tree": int(r.get("TreeId") or 0),
                # String keys in the datasheet columns (@key); the sword uses the AbilityID itself.
                "name": rich(loc.get(str(r.get("DisplayName", "")).lstrip("@").lower(), loc.get(aid.lower(), aid))),
                "description": rich(loc.get(str(r.get("Description", "")).lstrip("@").lower(), loc.get(aid.lower() + "_description", ""))),
                "category": r.get("UICategory", ""), "icon": icon,
                "background": ICONS + BACKGROUND.get(r.get("UICategory", ""), "abilities_bg1.dds"),
                "cooldown": cooldown.get(aid, 0),
            })
        # Whole tree for the Weapon Mastery screen: actives, upgrades, passives, ultimate, with the game's row/column.
        nodes = []
        all_rows = rows(f"javelindata_ability_{cfg.get('table', weapon)}")
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
        # An upgrade links to the ability above it, in the same column (tree arrows).
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
