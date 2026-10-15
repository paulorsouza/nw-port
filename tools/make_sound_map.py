#!/usr/bin/env python3
"""Links the extracted audio (extract/audio) to the set's animations.

Sources, per composite animation:
  - ProcLayer Audio windows (.adb): Play_* and the effort voices (Set_Switch_Vocals_*);
  - game .animevents: footsteps in each layer's clips, with the foot bone;
  - fixed reactions: impact and pain voice on Hit_*, shield sound on block, guard break, death.

usage: make_sound_map.py extract/ue/sword_shield/manifest.json [--surface grass]
Output: extract/ue/<set>/sounds.json  {asset: [{time, files[], bone, label}]}
"""
import argparse
import csv
import json
import os
import subprocess
import nwplatform
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO = os.path.join(ROOT, "extract", "audio")
nwplatform.configure()
GAME = nwplatform.game_directory()
ANIMEVENTS = "animations/gameplay/character/player/male/temp_male.animevents"

# Wwise voice switch -> extracted voice folder.
VOCALS = {
    "set_switch_vocals_exerta": "combat/voice_male/vo_exerta",
    "set_switch_vocals_exertb": "combat/voice_male/vo_exertb",
    "set_switch_vocals_landsoft": "combat/voice_male/vo_landsoft",
}
# Reactions (montage -> sounds at the start). The target uses Heroic Heavy: sword impact on armor.
REACTIONS = {
    # R1/R2/R3 reactions (AM_Hit1_, AM_Hit_, AM_Hit3_, and the Bow ones AM_Bw_Hit*) and the R4 fall (AM_Down_).
    "AM_Hit": [(0.0, "combat/impacts/imp_swordmetal_armor", "impacto"), (0.05, "combat/voice_male/vo_hurt", "dor")],
    "AM_Bw_Hit": [(0.0, "combat/impacts/imp_swordmetal_armor", "impacto"), (0.05, "combat/voice_male/vo_hurt", "dor")],
    "AM_Down_": [(0.0, "combat/impacts/imp_swordmetal_armor", "impacto"), (0.05, "combat/voice_male/vo_hurt", "dor")],
    "AM_BlockReact": [(0.0, "combat/impacts/block_wpn_metal_med_vs_shield_metal_med", "bloqueio")],
    "AM_BlockBreak": [(0.0, "combat/impacts/gpui_blockbreaker", "guarda quebrada")],
    "AM_Death": [(0.0, "combat/voice_male/vo_death", "morte")],
}


# Creatures (set -> montage -> sounds). Time "golpe" = start of the montage's CAGE-Damage window (the whoosh and the
# bite play 0.15 s earlier). The animals' footsteps (walk_boar, run_wolf...) are in another bank, not extracted yet.
# Pain gets the sword-on-flesh impact: for now only the player hits creatures.
CREATURES = {
    "boar": {
        "AM_Attack1": [("golpe", "creatures/boar/sfx_boar_whoosh", "whoosh"), ("golpe", "creatures/boar/sfx_boar_furwhoosh", "pelo")],
        "AM_Heavy": [(0.0, "creatures/boar/sfx_boar_charge", "investida")],
        "AM_ChargeStart": [(0.0, "creatures/boar/vox_boar_charge_setup_snort", "bufo"), (0.1, "creatures/boar/sfx_boar_charge_setup", "patas")],
        "AM_Aggro": [(0.0, "creatures/boar/boar_roar1", "rugido")],
        "AM_Growl": [(0.0, "creatures/boar/vo_breathing", "voz")],
        "AM_Hit_": [(0.0, "combat/impacts/imp_swordmetal_flesh", "impacto"), (0.03, "creatures/boar/boar_hurts", "dor")],
        "AM_Death": [(0.0, "creatures/boar/boar_death", "morte")],
    },
    "wolf": {
        "AM_Attack1": [("golpe", "creatures/wolf/vox_wolf_bite", "mordida")],
        "AM_Attack2": [("golpe", "creatures/wolf/vox_wolf_bite", "mordida")],
        "AM_Heavy": [(0.0, "creatures/wolf/vox_wolf_growl", "rosnado"), ("golpe", "creatures/wolf/vox_wolf_bite", "mordida")],
        "AM_Leap": [(0.0, "creatures/wolf/sfx_wolf_jump", "pulo"), ("golpe", "creatures/wolf/vox_wolf_bite", "mordida")],
        "AM_Aggro": [(0.0, "creatures/wolf/vox_wolf_alert", "alerta")],
        "AM_Hit_": [(0.0, "combat/impacts/imp_swordmetal_flesh", "impacto"), (0.03, "creatures/wolf/vox_wolf_hurt", "dor")],
        "AM_Death": [(0.0, "creatures/wolf/vox_wolf_death", "morte")],
    },
}
CREATURE_ANIMEVENTS = "animations/gameplay/character/npc/natural/predator/{0}/{0}.animevents"


MAX_VARIATIONS = 12


def folder_files(rel):
    d = os.path.join(AUDIO, rel)
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".ogg"))[:MAX_VARIATIONS] if os.path.isdir(d) else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest")
    ap.add_argument("--surface", default="dirt")
    a = ap.parse_args()
    m = json.load(open(a.manifest))

    by_event = {}
    for row in csv.DictReader(open(os.path.join(AUDIO, "sfx_index.csv"))):
        by_event.setdefault(row["event"].lower(), []).append(row)

    def event_files(event, surface=None):
        rows = by_event.get(event.lower(), [])
        if surface:
            # The name has "decals=corruption" in almost everything (the extractor keeps the 1st state value); surfacetype decides.
            for want in (surface, "dirt", "default"):
                hit = [r for r in rows if f"[surfacetype={want}]" in r["txtp"]]
                if hit:
                    rows = sorted(hit, key=lambda r: "decals=" in r["txtp"])
                    break
        return [os.path.join(AUDIO, r["file"]) for r in rows][:MAX_VARIATIONS]

    # Footsteps per clip (full .caf path -> events).
    set_name = os.path.basename(os.path.dirname(os.path.abspath(a.manifest)))
    creature = CREATURES.get(set_name)
    events_file = CREATURE_ANIMEVENTS.format(set_name) if creature else ANIMEVENTS
    xml = subprocess.run([nwplatform.executable("nwbt"), "cat", "-g", GAME, events_file], capture_output=True, check=True).stdout
    steps = {}
    for anim in ET.fromstring(xml):
        name = os.path.splitext(os.path.basename(anim.get("name", "")))[0].lower()
        for ev in anim:
            if ev.get("name") == "footstep":
                steps.setdefault(name, []).append((float(ev.get("time")), ev.get("parameter", ""), ev.get("bone", "")))

    out, missing = {}, set()

    def add(asset, t, files, bone="", label=""):
        if files:
            out.setdefault(asset, []).append({"time": round(t, 4), "files": files, "bone": bone, "label": label})

    # Footsteps: from the base layer of each composite animation (it is the one that moves the legs).
    fps = m["fps"]
    for comp in m["composites"].values():
        base_clip = comp["layers"][0]
        # .animevents time is normalized (0–1) by the clip duration.
        duration = (comp["frames"] - 1) / fps
        for t, param, bone in steps.get(base_clip, []):
            files = event_files(param, a.surface)
            if not files:
                missing.add(param)
            add(comp["asset"], t * duration, files, bone, param)

    # .adb Audio windows in the montages.
    for montage, sections in m.get("windows", {}).items():
        for i, sec in enumerate(sections):
            keys = m["montages"][montage]
            asset = m["composites"][keys[min(i, len(keys) - 1)]]["asset"]
            for w in sec["windows"]:
                if w["type"] != "Audio":
                    continue
                name = w["name"].lower()
                if name.startswith("play_"):
                    files = event_files(name)
                elif name in VOCALS:
                    files = folder_files(VOCALS[name])
                else:
                    continue
                if not files:
                    missing.add(name)
                add(asset, w["start"], files, "", name)

    # Creatures: attacks, voice, pain and death.
    for montage, keys in m["montages"].items() if creature else ():
        for prefix, sounds in creature.items():
            if montage != prefix and not (prefix.endswith("_") and montage.startswith(prefix)):
                continue
            asset = m["composites"][keys[0]]["asset"]
            hit = next((w["start"] for sec in m.get("windows", {}).get(montage, []) for w in sec["windows"]
                        if w["type"] == "CAGE-Damage"), 0.0)
            for t, rel, label in sounds:
                files = folder_files(rel)
                if not files:
                    missing.add(rel)
                add(asset, max(0.0, hit - 0.15) if t == "golpe" else t, files, "", label)

    # Reactions.
    for montage, keys in m["montages"].items() if not creature else ():
        for prefix, sounds in REACTIONS.items():
            if montage.startswith(prefix):
                asset = m["composites"][keys[0]]["asset"]
                for t, rel, label in sounds:
                    add(asset, t, folder_files(rel), "", label)

    path = os.path.join(os.path.dirname(a.manifest), "sounds.json")
    json.dump(out, open(path, "w"), indent=1)
    n = sum(len(v) for v in out.values())
    files = {f for v in out.values() for e in v for f in e["files"]}
    print(f"{n} sounds in {len(out)} animations, {len(files)} .ogg files -> {path}")
    if missing:
        print(f"no audio: {sorted(missing)}")


if __name__ == "__main__":
    main()
