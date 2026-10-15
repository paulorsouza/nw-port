#!/usr/bin/env python3
"""City NPCs: position on the city's map, appearance, idle animation and name.

    python3 tools/make_npcs.py [city ...]        (default: every city in CITIES)

Cities (CITIES): `queda_eterna` (Everfall, map L_QuedaEterna, capital `ef_persistence`,
settlement_northwindsward_complete) and `guardaventos` (Windsward, map L_SouthWindsward, capital
`06swindsward_persistence`, settlement_southwindsward_complete). Each city writes its own list and animation set.

Sources:
- capitals of regions r_+04_+01 and r_+04_+02 within the city's radius of its settlement capital;
- `AmbientNPC_MasterVariant`: the variant comes in the capital itself (`variantName`);
- `CoatGen/<capital>/MasterNpc_*Spawner_*.dynamicslice` (extract/slicejson, step sistemas-02 of build_all): the
  variant is the `m_slicevariant`;
- `CoatGen/<capital>/NPC_*.dynamicslice` and `Characters/Mission/NPC_*` (slices/characters/mission): the slice
  already has the `.cdf`, the name (`m_name`) and the timelines;
- `javelindata_variations_npcs` (+ `_walkaway`): `.cdf`, IdleTimeline and Name of each variant;
- strings in extract/loc/localization/<language>.

Without the game's quests (maintainer decision, docs/game/city.md): the NPCs of the initial state stay, with no
achievement. `MasterNpc_SwapHidden_*` shows (disappears after an achievement) and `MasterNpc_SwapShown_*` does not
(only shows after).

Output per city: unreal/NWPort/Data/World/<city>_npcs.json and tools/sets/npc_<city>.json
    {"city", "npcs": [{"variant", "npc_id", "name": {"pt-br", "en-us"}, "cdf", "idle", "pos": [x, y, z], "yaw"}],
     "dummies"}
"""
import collections
import glob
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
REGIONS = os.path.join(ROOT, "extract", "terrain", "sharedassets", "coatlicue", "newworld_vitaeeterna", "regions")
SLICEJSON = os.path.join(ROOT, "extract", "slicejson")
LOC = os.path.join(ROOT, "extract", "loc", "localization")
TIMELINES = os.path.join(ROOT, "extract", "timelines")   # step sistemas-03 of build_all
CDFS = os.path.join(ROOT, "extract", "npcs")             # NPC .cdf (tools/linux/heavy.sh nwbt unpack; docs/game/city.md)
WORLD = os.path.join(ROOT, "unreal", "NWPort", "Data", "World")
REWARDS = os.path.join(ROOT, "tools", "npc_rewards.json")              # gifts per trade skill (docs/game/city.md)
REWARDS_OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "npc_rewards.json")
# Skeletons the port animates: the common NPC one and the male player one (same bones). Others (Grace O'Malley has
# her own) stay out until they have a set.
SKELETONS = ("objects/characters/npc/npc_skel.chr", "objects/characters/player/male/player_male.chr")
FALLBACK_IDLE = "settle_stand_idle"   # generic NPC idle (also the set's "Idle" loop)
# City id -> settlement slice (capital sliceName), radius in m around it, set name (asset root /Game/NW/<set>).
# The list file is Data/World/<id>_npcs.json and the set tools/sets/npc_<id>.json.
# Guardaventos: 250 m holds the town's NPCs (the next ones, from 268 m on, are on the roads outside the walls).
CITIES = {
    "queda_eterna": {"slice": "settlement_northwindsward_complete", "radius": 300.0, "set": "NpcQuedaEterna",
                     "label": "Everfall"},
    "guardaventos": {"slice": "settlement_southwindsward_complete", "radius": 250.0, "set": "NpcGuardaventos",
                     "label": "Windsward"},
}


def rows(name):
    return json.load(open(os.path.join(DT, name + ".datasheet.json")))["rows"]


def load_loc(language):
    out = {}
    for f in glob.glob(os.path.join(LOC, language, "*.loc.xml")):
        try:
            for s in ET.parse(f).getroot().iter("string"):
                out[s.get("key", "").lower()] = s.text or ""
        except ET.ParseError:
            continue
    return out


def text(loc, key):
    return re.sub(r"<[^>]+>", "", loc.get(str(key or "").lstrip("@").lower(), "")).strip()


def norm(p):
    return str(p or "").replace("\\\\", "/").replace("\\", "/").lower()


def slice_json(name):
    """Extracted slice of a capital: CoatGen/<hash>/<name>.dynamicslice or Characters/Mission/<name> (slices/...)."""
    for f in (os.path.join(SLICEJSON, name + ".json"), os.path.join(SLICEJSON, "slices", name + ".dynamicslice.json")):
        if os.path.exists(f):
            return f
    return None


def main():
    wanted = sys.argv[1:] or list(CITIES)
    unknown = [c for c in wanted if c not in CITIES]
    if unknown:
        raise SystemExit(f"unknown city {unknown}; known: {list(CITIES)}")
    caps = []
    for f in sorted(glob.glob(os.path.join(REGIONS, "r_+04_+0[12]", "capitals", "*", "*.capitals.json"))):
        caps += json.load(open(f)).get("Capitals", [])
    for city_id in wanted:
        make_city(city_id, CITIES[city_id], caps)


def make_city(CITY_ID, cfg, caps):
    CITY, RADIUS = cfg["slice"], cfg["radius"]
    OUT = os.path.join(WORLD, f"{CITY_ID}_npcs.json")
    SET = os.path.join(ROOT, "tools", "sets", f"npc_{CITY_ID}.json")
    city = next(c for c in caps if norm(c.get("sliceName")).endswith("/" + CITY) or norm(c.get("sliceName")) == CITY)
    T = city["worldPosition"]
    ang = 2 * math.atan2(city["rotation"]["z"], city["rotation"]["w"])
    ca, sa = math.cos(-ang), math.sin(-ang)

    def to_map(wx, wy, wz):
        dx, dy = wx - T["x"], wy - T["y"]
        lx, ly = ca * dx - sa * dy, sa * dx + ca * dy
        return [round(-lx * 100, 1), round(ly * 100, 1), round((wz - T["z"]) * 100, 1)]

    variants = {}
    for t in ("javelindata_variations_npcs", "javelindata_variations_npcs_walkaway"):
        for r in rows(t):
            variants.setdefault(r["VariantID"], r)
    pt, en = load_loc("pt-br"), load_loc("en-us")

    def idle_anim(timeline):
        """Alias of the idle animation (PlayAnim layer; the *_look_poses_* are look additives)."""
        f = os.path.join(TIMELINES, timeline + ".json")
        if not os.path.exists(f):
            return ""
        names = re.findall(r'"animationalias"\s*:\s*"([^"]+)"', open(f).read())
        return next((n.lower() for n in names if "look_poses" not in n.lower()), "")

    def skeleton(cdf):
        f = os.path.join(CDFS, cdf)
        m = re.search(r'File="([^"]+\.chr)"', open(f).read()) if os.path.exists(f) else None
        return norm(m.group(1)) if m else ""

    npcs, skipped = [], collections.Counter()
    for c in caps:
        p = c["worldPosition"]
        if math.hypot(p["x"] - T["x"], p["y"] - T["y"]) > RADIUS:
            continue
        name = norm(c.get("sliceName"))
        base = os.path.basename(name)
        variant = cdf = idle = key = None
        if base == "ambientnpc_mastervariant":
            variant = c.get("variantName")
        elif base.startswith("masternpc_swapshown"):
            skipped["swapshown"] += 1
            continue
        elif base.startswith(("masternpc_", "npc_")):
            f = slice_json(name)
            if not f:
                skipped["no slice: " + base] += 1
                continue
            s = open(f).read()
            m = re.search(r'"m_slicevariant"\s*:\s*"([^"]+)"', s)
            if m:
                variant = m.group(1)
            else:
                cdfs = re.findall(r'"([^"]+\.cdf)"', s)
                if not cdfs:
                    skipped["no npc: " + base] += 1
                    continue
                cdf = norm(cdfs[0])
                timelines = [norm(t) for t in re.findall(r'"([^"]+\.timeline)"', s)]
                idle = next((t for t in timelines if "ambient" in t or "idle" in t), timelines[0] if timelines else "")
                m = re.search(r'"m_name"\s*:\s*"(@[^"]+)"', s)
                key = m.group(1) if m else ""
        else:
            continue
        if variant:
            v = variants.get(variant)
            if not v:
                skipped["variant without row: " + variant] += 1
                continue
            cdf, idle, key = norm(v.get("CharacterDefinition")), norm(v.get("IdleTimeline")), v.get("Name") or ""
        name_pt = text(pt, key)
        if name_pt.upper() == "DEPRECATED":
            skipped["DEPRECATED: " + (variant or base)] += 1
            continue
        skel = skeleton(cdf)
        if skel not in SKELETONS:
            skipped[f"skeleton {skel or '?'}: {variant or base}"] += 1
            continue
        q = c["rotation"]
        yaw = 2 * math.atan2(q["z"], q["w"])
        npcs.append({
            # Slice name without the coatgen hash (npc_1225_giacomo_ef_2256178722.dynamicslice -> npc_1225_giacomo_ef).
            "variant": variant or re.sub(r"_\d+\.dynamicslice$", "", base),
            "npc_id": (variants.get(variant) or {}).get("NPCId", "") if variant else "",
            "name": {"pt-br": name_pt, "en-us": text(en, key)},
            "cdf": cdf,
            "mesh": "SK_" + os.path.splitext(os.path.basename(cdf))[0],
            "idle": idle,
            "anim": idle_anim(idle),
            "pos": to_map(p["x"], p["y"], p["z"]),
            # Map rotation: the world one minus the city one, mirrored in X (Cry → UE), as in make_spawns.py.
            "yaw": round(180 - math.degrees(yaw - ang), 1),
        })
    # One NPC per name: the game keeps several copies of a quest NPC in the town (variants _03, _03A, _03B, ... of
    # MasterNpc_SwapHidden_*), each hidden by a different quest step. Without quests only the base one stays (lowest
    # VariantID: 06_LeovixSilva_03 before 06_LeovixSilva_03a and 06_LeovixSilva_04). Unnamed ambient NPCs all stay.
    kept = {}
    for n in sorted(npcs, key=lambda n: n["variant"].lower()):
        k = n["name"]["pt-br"] or ("#" + n["variant"] + str(n["pos"]))
        if k in kept:
            skipped["copy of " + kept[k]["variant"] + ": " + n["variant"]] += 1
            continue
        kept[k] = n
    npcs = [n for n in npcs if kept.get(n["name"]["pt-br"] or ("#" + n["variant"] + str(n["pos"]))) is n]
    # build_set.py takes the clips from player/male only. An idle outside it (Aelstrom: npc/gestures/male/
    # g_male_loopidle) falls back to the generic NPC idle.
    paths = os.path.join(ROOT, "extract", "manifest", "paths.txt")
    if os.path.exists(paths):
        root = "animations/gameplay/character/player/male/"
        cafs = {os.path.basename(l.strip())[:-4].lower() for l in open(paths) if l.startswith(root) and l.strip().endswith(".caf")}
        for n in npcs:
            if n["anim"] and n["anim"] not in cafs:
                skipped[f"idle {n['anim']} outside player/male -> {FALLBACK_IDLE}: {n['variant']}"] += 1
                n["anim"] = FALLBACK_IDLE
    # Animation set (build_set.py): base character with npc_skel, one loop per idle animation.
    anims = sorted({n["anim"] for n in npcs if n["anim"]})
    base = next(n["cdf"] for n in npcs if skeleton(n["cdf"]) == SKELETONS[0])
    spec = {
        "name": cfg["set"],
        "mesh_name": "SK_" + cfg["set"],
        "character": base,
        "_comment": f"Generated by tools/make_npcs.py: idle animations of the {cfg['label']} NPCs (IdleTimeline timelines).",
        "weapons": {}, "blendspaces": {}, "montages": {},
        # export_ue.py requires an "Idle" loop (reference pose of the montages): the generic NPC idle.
        "loops": {"Idle": [FALLBACK_IDLE], **{a: [a] for a in anims}},
    }
    # Versioned file: LF on Windows too, so regenerating does not dirty the diff.
    json.dump(spec, open(SET, "w", newline="\n"), indent=1)
    # Trade skill of each NPC (weekly draw group) and copy of the gifts for the game.
    rewards = json.load(open(REWARDS))
    for n in npcs:
        n["tipo"] = rewards["npcs"].get(n["variant"], "")
    untyped = [n["variant"] for n in npcs if n["name"]["pt-br"] and not n["tipo"]]
    if untyped:
        raise SystemExit(f"named NPC without a trade skill in {REWARDS}: {untyped}")
    json.dump({k: v for k, v in rewards.items() if k != "_comment"}, open(REWARDS_OUT, "w"), ensure_ascii=False, indent=1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    # Training ground of the weapon tutorials (tools/tutorials.json, "bonecos").
    dummies = json.load(open(os.path.join(ROOT, "tools", "tutorials.json"))).get("bonecos", {}).get(CITY_ID, [])
    json.dump({"city": CITY_ID, "npcs": npcs, "dummies": dummies}, open(OUT, "w"), ensure_ascii=False, indent=1)
    cdfs = collections.Counter(n["cdf"] for n in npcs)
    print(f"{SET}: base {base}, {len(anims)} animations")
    print(f"{OUT}: {len(npcs)} NPCs, {len(cdfs)} appearances, {len(set(n['idle'] for n in npcs))} idle timelines")
    for k, v in sorted(skipped.items()):
        print(f"  skipped {v}: {k}")


if __name__ == "__main__":
    main()
