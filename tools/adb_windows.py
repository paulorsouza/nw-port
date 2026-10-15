#!/usr/bin/env python3
"""Time windows of the procedural clips (ProcLayer) of a Mannequin fragment.

In each ProcLayer the clips come in sequence: a clip's Blend.ExitTime is the delay relative to
the start of the previous clip in the same layer. A <Procedural type=""> only marks the end of the previous one.
The last clip of the layer runs until the end of the fragment (end = None).

usage: adb_windows.py <adb>... --tags 1H_Melee,Sword --fragment Attack_Primary
"""
import argparse
import json
import xml.etree.ElementTree as ET

# Parameter that identifies each procedural clip type.
KEY_PARAM = {
    "CAGE-Named": "Name",
    "CAGE-TransitionBuffered": "Condition",
    "CAGE-TransitionInstant": "Condition",
    "Audio": "StartTrigger",
    "WeaponEffects": "EffectGroupName",
    "ParticleEffect": "EffectName",
    "PayStaminaCost": "StaminaCost",
}


def best_fragment(adb_files, tags, fragment, need_anims=True):
    """Most specific variant of the fragment among those that fit the tags (same criterion as adb_resolve).
    need_anims=False accepts fragments with only ProcLayer (weapon scope, e.g. playermaleanims_1h_meleeattach.adb)."""
    best, best_n = None, -1
    for adb in adb_files:
        root = ET.parse(adb).getroot()
        for frag in root.find("FragmentList"):
            if frag.tag != fragment:
                continue
            for f in frag.findall("Fragment"):
                ftags = {t for t in f.get("Tags", "").split("+") if t}
                if ftags <= tags and len(ftags) > best_n and (f.findall("AnimLayer") or not need_anims) \
                        and (need_anims or f.findall("ProcLayer")):
                    best, best_n = f, len(ftags)
    return best


def windows(frag):
    out = []
    for layer in frag.findall("ProcLayer"):
        t, prev = 0.0, None
        items = list(layer)
        for i in range(0, len(items) - 1, 2):
            blend, proc = items[i], items[i + 1]
            # ExitTime is relative to the start of the previous clip in the layer, empty or not (empty only ends it).
            t += float(blend.get("ExitTime", 0))
            if prev is not None:
                prev["end"] = round(t, 4)
            ptype = proc.get("type", "")
            if not ptype:
                prev = None
                continue
            params = {p.tag: p.get("value") for p in proc.iter() if p.get("value") is not None}
            entry = {"type": ptype, "start": round(t, 4), "end": None,
                     "name": params.get(KEY_PARAM.get(ptype, ""), ""), "params": params}
            out.append(entry)
            prev = entry
    return sorted(out, key=lambda w: w["start"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("adb", nargs="+")
    ap.add_argument("--tags", required=True)
    ap.add_argument("--fragment", required=True)
    a = ap.parse_args()
    frag = best_fragment(a.adb, set(a.tags.split(",")), a.fragment)
    if frag is None:
        raise SystemExit("fragment not found")
    for w in windows(frag):
        end = f"{w['end']:.3f}" if w["end"] is not None else "end"
        print(f"{w['start']:6.3f} → {end:>6}  {w['type']:26} {w['name']}")


if __name__ == "__main__":
    main()
