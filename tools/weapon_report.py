#!/usr/bin/env python3
"""Report of everything the game defines about the combat of a player weapon: starting point for porting
a new weapon (docs/tech/weapon-porting.md).

    python3 tools/weapon_report.py bow                    # known weapon (WEAPONS below)
    python3 tools/weapon_report.py spear --tag Spear --adb player/player_spear.adb --base 2H_Melee \\
        --damage Spear_ --cost Spear --ability spear      # new weapon: give the tag, the .adb and the prefixes

Output: extract/research/<weapon>_report.md (markdown) and <weapon>_report.json. Only reads already extracted files
(tools/linux/build_all.sh 02 and 04): tables in extract/data, Mannequin recipes in extract/models/.../adb, strings in
extract/loc. Does not open Unreal or the game.

Sections:
1. Mannequin fragments with the weapon tags: clips per layer and windows (damage, chain, cancel, dodge,
   invulnerable, homing, sound, projectile...). Only fragments with the weapon's own variant (the generic ones, like
   jump, are left out: they already exist in the player set).
2. Damage rows (javelindata_damagetable) with the weapon prefix: the columns that change combat.
3. Stamina costs and cooldowns.
4. Abilities (Weapon Mastery tree): name and description in pt-BR, cooldown, tree position, whether active.
5. Spells (spelltable: areas, projectiles) and the weapon's status effects.
"""
import argparse
import glob
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adb_resolve  # noqa: E402
import adb_windows  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
ADB = os.path.join(ROOT, "extract", "models", "animations", "mannequin", "adb")
LOC = os.path.join(ROOT, "extract", "loc", "localization", "pt-br")
PATHS = os.path.join(ROOT, "extract", "manifest", "paths.txt")
OUT = os.path.join(ROOT, "extract", "research")

# Mannequin tag, own .adb, family tags (1H_Melee/2H_Melee/2H_Ranged), damage row prefix,
# stamina CostID substring, suffix of the abilities/spells/status table.
WEAPONS = {
    "sword": dict(tag="Sword", adb="player/player_sword.adb", base="1H_Melee", damage="1H_Sword_", cost="1H_Sword", ability="sword"),
    "hatchet": dict(tag="Axe", adb="player/player_axe.adb", base="1H_Melee", damage="Hatchet_", cost="1H_Axe", ability="hatchet"),
    "rapier": dict(tag="Rapier", adb="player/player_rapier.adb", base="1H_Melee", damage="1H_Rapier_", cost="1H_Rapier", ability="rapier"),
    "bow": dict(tag="Bow", adb="player/player_bow.adb", base="2H_Ranged", damage="Bow_", cost="2H_Bow", ability="bow"),
    "spear": dict(tag="Spear", adb="player/player_spear.adb", base="2H_Melee", damage="Spear_", cost="Spear", ability="spear"),
    "greataxe": dict(tag="GreatAxe", adb="player/player_greataxe.adb", base="2H_Melee", damage="GreatAxe_", cost="GreatAxe", ability="greataxe"),
    "warhammer": dict(tag="WarHammer", adb="player/player_warhammer.adb", base="2H_Melee", damage="WarHammer_", cost="WarHammer", ability="warhammer"),
    "musket": dict(tag="Rifle", adb="player/player_rifle.adb", base="2H_Ranged", damage="Musket_", cost="Musket", ability="musket"),
}
# Damage table columns that change combat (the others are runes, threat, durability...).
DAMAGE_COLS = ["AttackType", "DamageType", "DmgCoef", "DmgCoefHead", "DmgCoefCrit", "HitStun", "BlockHitStun", "CritHitStun",
               "PowerLevel", "BlockPowerLevel", "CritPowerLevel", "ImpactDistanceX", "ImpactDistanceY", "ImpactDistanceZ",
               "ImpactDecayRate", "Unblockable", "NoReaction", "NoBackstab", "NoHeadshot", "CanCrit", "IsRanged",
               "CameraShakeID", "TargetCameraShakeID", "BlockStaminaDmgMod", "StaggerDmgModifier"]
# Windows that matter in the port (the others are visual effects, look poses...).
WINDOW_TYPES = {"CAGE-Damage", "CAGE-Named", "CAGE-TransitionBuffered", "CAGE-TransitionInstant", "Audio", "PayStaminaCost",
                "Homing", "Invulnerability", "DisableCollision", "CharacterEvent", "SpawnProjectile", "FireProjectile"}


def rows(name):
    for p in glob.glob(os.path.join(DT, "**", name + ".datasheet.json"), recursive=True):
        return json.load(open(p))["rows"]
    return []


def load_loc():
    out = {}
    for f in glob.glob(os.path.join(LOC, "*.loc.xml")):
        try:
            for s in ET.parse(f).getroot().iter("string"):
                out[s.get("key", "").lower()] = s.text or ""
        except ET.ParseError:
            continue
    return out


def text(loc, key):
    if not isinstance(key, str) or not key.startswith("@"):
        return key or ""
    t = loc.get(key[1:].lower(), key)
    return re.sub(r"<[^>]+>", "", t).replace("\n", " ").strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("weapon")
    for k in ("tag", "adb", "base", "damage", "cost", "ability"):
        ap.add_argument(f"--{k}")
    a = ap.parse_args()
    w = dict(WEAPONS.get(a.weapon, {}))
    for k in ("tag", "adb", "base", "damage", "cost", "ability"):
        if getattr(a, k):
            w[k] = getattr(a, k)
    missing = [k for k in ("tag", "adb", "base", "damage", "cost", "ability") if k not in w]
    if missing:
        sys.exit(f"unknown weapon: pass {', '.join('--' + k for k in missing)}")
    family = "player/player_" + w["base"].lower() + ".adb"
    adbs = [os.path.join(ADB, x) for x in (w["adb"], family, "player/player.adb") if os.path.exists(os.path.join(ADB, x))]
    tags = {w["tag"], w["base"]}
    report = {"weapon": a.weapon, "tags": sorted(tags), "adbs": [os.path.relpath(x, ADB) for x in adbs]}
    md = [f"# Game report: {a.weapon}", "",
          f"Generated by `tools/weapon_report.py {a.weapon}`. Mannequin tags: `{','.join(sorted(tags))}`; "
          f".adb: {', '.join('`' + x + '`' for x in report['adbs'])}.", ""]

    # 1. Fragments with a weapon variant.
    idx = adb_resolve.index_animations(PATHS) if os.path.exists(PATHS) else {}
    best = adb_resolve.resolve(adb_resolve.fragments(adbs), tags)
    frags = {}
    md += ["## 1. Mannequin fragments", "", "Only those with a variant with the weapon tag. Times in seconds.", ""]
    for name in sorted(best):
        f = best[name]
        if w["tag"] not in f["tags"]:
            continue
        layers = [[x["name"] for x in layer] for layer in f["layers"]]
        frag = adb_windows.best_fragment(adbs, tags, name)
        wins = [x for x in (adb_windows.windows(frag) if frag is not None else []) if x["type"] in WINDOW_TYPES]
        frags[name] = {"tags": f["tags"], "adb": f["adb"], "layers": layers,
                       "files": [[idx.get(n.lower(), "") for n in layer] for layer in layers], "windows": wins}
        md.append(f"### {name}  `[{'+'.join(f['tags'])}]`")
        md.append("")
        md.append("Clips: " + " + ".join("/".join(layer) for layer in layers))
        if wins:
            md.append("")
            md.append("| start | end | type | name |")
            md.append("|---:|---:|---|---|")
            for x in wins:
                end = f"{x['end']:.3f}" if x["end"] is not None else "end"
                md.append(f"| {x['start']:.3f} | {end} | {x['type']} | {x['name']} |")
        md.append("")
    report["fragments"] = frags

    # 2. Damage.
    dmg = [r for r in rows("javelindata_damagetable") if str(r.get("DamageID", "")).startswith(w["damage"])]
    report["damage"] = {r["DamageID"]: {c: r.get(c) for c in DAMAGE_COLS if r.get(c) not in (None, "", 0, False)} for r in dmg}
    md += ["## 2. Damage rows", "", f"`javelindata_damagetable`, prefix `{w['damage']}` ({len(dmg)} rows). Empty = 0.", ""]
    cols = [c for c in DAMAGE_COLS if any(r.get(c) not in (None, "", 0, False) for r in dmg)]
    md.append("| DamageID | " + " | ".join(cols) + " |")
    md.append("|---|" + "---|" * len(cols))
    for r in dmg:
        md.append(f"| {r['DamageID']} | " + " | ".join(str(r.get(c, "")) if r.get(c) not in (None, 0, False) else "" for c in cols) + " |")
    md.append("")

    # 3. Stamina and cooldowns.
    cost = [r for r in rows("javelindata_staminacosts_player") if w["cost"].lower() in str(r.get("CostID", "")).lower()]
    cds = [r for r in rows("javelindata_cooldowns_player") if str(r.get("WeaponTag", "")).lower() == w["tag"].lower()
           or w["ability"].lower() in str(r.get("AbilityID", "")).lower()]
    report["stamina"] = {r["CostID"]: r.get("StaminaCost", 0) for r in cost}
    report["cooldowns"] = {r["AbilityID"]: r.get("Time") for r in cds}
    md += ["## 3. Stamina and cooldowns", "", "| CostID | stamina |", "|---|---:|"]
    md += [f"| {k} | {v} |" for k, v in report["stamina"].items()]
    md += ["", "| AbilityID | cooldown (s) |", "|---|---:|"]
    md += [f"| {k} | {v} |" for k, v in report["cooldowns"].items()]
    md.append("")

    # 4. Abilities.
    loc = load_loc()
    abil = rows(f"javelindata_ability_{w['ability']}")
    report["abilities"] = {}
    md += ["## 4. Abilities", "", "Actives first; then the tree passives.", ""]
    for r in sorted(abil, key=lambda r: (not r.get("IsActiveAbility"), r.get("TreeId") or 0, r.get("TreeRowPosition") or 0)):
        name, desc = text(loc, r.get("DisplayName")), text(loc, r.get("Description"))
        info = {"name": name, "description": desc, "active": bool(r.get("IsActiveAbility")),
                "tree": r.get("TreeId"), "row": r.get("TreeRowPosition"), "col": r.get("TreeColumnPosition"),
                "cooldown": report["cooldowns"].get(r.get("CooldownId") or r["AbilityID"]),
                "fields": {k: v for k, v in r.items() if v not in ("", None, 0, False) and k not in
                           ("AbilityID", "DisplayName", "Description", "Icon", "Sound")}}
        report["abilities"][r["AbilityID"]] = info
        kind = "active" if info["active"] else "passive"
        cd = f", cooldown {info['cooldown']} s" if info["cooldown"] else ""
        md.append(f"- **{name}** (`{r['AbilityID']}`, {kind}{cd}, tree {info['tree']} row {info['row']}): {desc}")
    md.append("")

    # 5. Spells and status.
    spells = rows(f"javelindata_spelltable_{w['ability']}")
    status = rows(f"javelindata_statuseffects_{w['ability']}")
    report["spells"] = {r["SpellID"]: {k: v for k, v in r.items() if v not in ("", None, 0, False)} for r in spells}
    report["status"] = {r["StatusID"]: {k: v for k, v in r.items() if v not in ("", None, 0, False)} for r in status}
    md += ["## 5. Spells (areas, projectiles) and status effects", ""]
    for k, v in report["spells"].items():
        md.append(f"- `{k}`: " + ", ".join(f"{a}={b}" for a, b in v.items() if a != "SpellID"))
    md.append("")
    for k, v in report["status"].items():
        md.append(f"- `{k}` ({text(loc, v.get('DisplayName'))}): " + ", ".join(
            f"{a}={b}" for a, b in v.items() if a not in ("StatusID", "DisplayName", "Description", "PlaceholderIcon")))
    md.append("")

    os.makedirs(OUT, exist_ok=True)
    base = os.path.join(OUT, f"{a.weapon}_report")
    json.dump(report, open(base + ".json", "w"), indent=1, ensure_ascii=False)
    open(base + ".md", "w").write("\n".join(md))
    print(f"{len(frags)} fragments, {len(dmg)} damage rows, {len(abil)} abilities -> {base}.md")


if __name__ == "__main__":
    main()
