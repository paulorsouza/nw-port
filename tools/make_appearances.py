#!/usr/bin/env python3
"""Equipment appearances: which mesh each catalog item puts on the player (light step, no nwbt).

    python3 tools/make_appearances.py

Reads Data/Items/items.pt-br.json (tools/make_items.py) and the game tables:
- armor (set Head/Chest/Hands/Legs/Feet): javelindata_itemdefinitions_master_* `ArmorAppearanceM` ->
  javelindata_itemappearancedefinitions (`Skin1`, `Material1`, `AppearanceCDF` with cloth, `HideHair`, `HideHead`);
- weapons, shields and tools: `WeaponAppearanceOverride` -> javelindata_itemdefinitions_weaponappearances
  `MeshOverride` (.cdf). Ice Gauntlet: the glove stays the set's SK_icegauntlet; the item only changes the palm
  crystal, objects/weapons/magic/gauntletcore/1h/<gauntlet folder>/wep_mag_gauntcore_1h_<gauntlet folder>.cdf
  (the same kind of mesh as the set's SM_icegauntlet_core).

Writes:
- extract/ue/appearances/plan.json: what tools/convert_appearances.py converts;
- unreal/NWPort/Data/Items/appearances.json: item -> asset, read by the C++. When the conversion already ran
  (extract/ue/appearances/<Kind>/<name>/<name>.fbx), entries whose FBX is missing are left out.
"""
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "extract", "data")
PATHS = os.path.join(ROOT, "extract", "manifest", "paths.txt")
CATALOG = os.path.join(ROOT, "unreal", "NWPort", "Data", "Items", "items.pt-br.json")
OUT_JSON = os.path.join(ROOT, "unreal", "NWPort", "Data", "Items", "appearances.json")
WORK = os.path.join(ROOT, "extract", "ue", "appearances")
PLAYER_GLB = os.path.join(ROOT, "extract", "ue", "sword_shield", "glb", "objects", "characters", "player", "male",
                          "player_male.cdf.glb")
ASSET_ROOT = "/Game/NW/Appearances"

ARMOR_SETS = ("Head", "Chest", "Hands", "Legs", "Feet")
WEAPON_SETS = ("Sword", "Hatchet", "Shield", "Bow", "Rapier", "IceGauntlet", "Pick", "Sickle", "LoggingAxe",
               "SkinningKnife")
# Base body parts (player_male.cdf attachments) -> asset name. The keys are what "covers" lists.
BODY = {"head": "head1", "shirt1": "shirt1", "gloves1": "gloves1", "pants1": "pants1", "boots1": "boots1"}
# Armor slot -> base part it replaces (same mapping as tools/nwchar.py SLOT_OF: chest/forearms/thighs/calves).
COVERS = {"Chest": ["shirt1"], "Hands": ["gloves1"], "Legs": ["pants1"], "Feet": ["boots1"], "Head": []}


def rows(name):
    return json.load(open(glob.glob(os.path.join(DATA, "**", f"javelindata_{name}.datasheet.json"),
                                    recursive=True)[0]))["rows"]


def master_rows():
    out = {}
    for p in sorted(glob.glob(os.path.join(DATA, "**", "javelindata_itemdefinitions_master_*.datasheet.json"),
                              recursive=True)):
        for r in json.load(open(p))["rows"]:
            out.setdefault(r["ItemID"].lower(), r)
    return out


def asset_name(text):
    return re.sub(r"[^A-Za-z0-9_]", "_", text.strip())


def mesh_path(kind, name):
    return f"{ASSET_ROOT}/{kind}/{name}.{name}"


def fbx_of(kind, name):
    return os.path.join(WORK, kind, name, name + ".fbx")


def main():
    items = json.load(open(CATALOG))
    master = master_rows()
    armor_rows = {r["ItemID"].lower(): r for r in rows("itemappearancedefinitions")}
    weapon_rows = {r["WeaponAppearanceID"].strip().lower(): r for r in rows("itemdefinitions_weaponappearances")}
    pak = set(open(PATHS, encoding="utf-8").read().lower().split("\n")) if os.path.exists(PATHS) else None

    plan = {"player_glb": PLAYER_GLB,
            "body": [{"part": part, "name": f"SK_body_{key}"} for key, part in BODY.items()],
            "armor": {}, "weapons": {}}
    armor, weapons, problems = {}, {}, []
    # MeshOverride (lowercase) -> SM name: appearances that only change the dye share the mesh. The name comes from
    # the item whose appearance has its own ID (2hBowT5), base items before the Lost/Drop variants (2hBowLostT4 uses
    # the T5 bow), else from the first item.
    by_mesh = {}
    for it in sorted(items, key=lambda i: bool(re.search(r"lost|drop", i["id"], re.I))):
        row = master.get(it["id"].lower()) or {}
        app_id = (row.get("WeaponAppearanceOverride") or "").strip()
        app = weapon_rows.get(app_id.lower())
        if it.get("set") in WEAPON_SETS and app and app.get("MeshOverride") and app_id.lower() == it["id"].lower():
            by_mesh.setdefault(app["MeshOverride"].lower(), "SM_" + asset_name(app_id))

    for it in items:
        s = it.get("set")
        row = master.get(it["id"].lower())
        if s in ARMOR_SETS:
            app_id = (row or {}).get("ArmorAppearanceM") or ""
            app = armor_rows.get(app_id.lower())
            if not app or not app.get("Skin1"):
                problems.append(f"{it['id']}: no male appearance ({app_id!r})")
                continue
            name = "SK_" + asset_name(app["ItemID"])
            plan["armor"].setdefault(app["ItemID"], {"name": name, "slot": s, "skin": app["Skin1"],
                                                     "cdf": app.get("AppearanceCDF") or ""})
            covers = list(COVERS[s])
            if s == "Head" and app.get("HideHead"):
                covers.append("head")
            armor[it["id"]] = {"slot": s, "mesh": mesh_path("Armor", name), "covers": covers,
                               "hide_hair": bool(app.get("HideHair")), "appearance": app["ItemID"],
                               "_fbx": fbx_of("Armor", name)}
        elif s in WEAPON_SETS:
            app_id = ((row or {}).get("WeaponAppearanceOverride") or "").strip()
            app = weapon_rows.get(app_id.lower())
            cdf = (app or {}).get("MeshOverride") or ""
            if not cdf:
                problems.append(f"{it['id']}: no MeshOverride ({app_id!r})")
                continue
            if s == "IceGauntlet":
                folder = cdf.replace("\\", "/").split("/")[-2].lower()
                cdf = f"objects/weapons/magic/gauntletcore/1h/{folder}/wep_mag_gauntcore_1h_{folder}.cdf"
            key = cdf.lower()
            if s == "IceGauntlet" and app["MeshOverride"].lower() in by_mesh:
                by_mesh.setdefault(key, by_mesh[app["MeshOverride"].lower()])
            if pak is not None and key not in pak:
                problems.append(f"{it['id']}: {cdf} is not in the pak")
                continue
            name = by_mesh.setdefault(key, "SM_" + asset_name(app["WeaponAppearanceID"]))
            plan["weapons"].setdefault(name, {"name": name, "cdf": key})
            weapons[it["id"]] = {"mesh": mesh_path("Weapons", name), "set": s, "appearance": app_id,
                                 "_fbx": fbx_of("Weapons", name)}

    os.makedirs(WORK, exist_ok=True)
    json.dump(plan, open(os.path.join(WORK, "plan.json"), "w"), indent=1)

    # After the conversion: keep only what has an FBX, so the C++ never points to an asset that was not made.
    converted = os.path.isdir(os.path.join(WORK, "Armor")) or os.path.isdir(os.path.join(WORK, "Weapons"))

    def keep(entries):
        out = {}
        for k, v in entries.items():
            if converted and not os.path.exists(v["_fbx"]):
                problems.append(f"{k}: FBX missing ({v['_fbx']})")
                continue
            out[k] = {kk: vv for kk, vv in v.items() if not kk.startswith("_")}
        return out

    data = {"body": {key: mesh_path("Body", f"SK_body_{key}") for key in BODY},
            "armor": keep(armor), "weapons": keep(weapons)}
    json.dump(data, open(OUT_JSON, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    for p in problems:
        print("warning:", p)
    print(f"appearances: {len(data['armor'])} armor items ({len(plan['armor'])} meshes), "
          f"{len(data['weapons'])} weapon items ({len(plan['weapons'])} meshes) -> {OUT_JSON}")


if __name__ == "__main__":
    main()
