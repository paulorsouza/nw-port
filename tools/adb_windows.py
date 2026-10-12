#!/usr/bin/env python3
"""Janelas de tempo dos clipes procedurais (ProcLayer) de um fragmento do Mannequin.

Em cada ProcLayer os clipes vêm em sequência: o Blend.ExitTime de um clipe é o atraso em relação
ao início do clipe anterior da mesma camada. Um <Procedural type=""> só marca o fim do anterior.
O último clipe da camada vai até o fim do fragmento (end = None).

uso: adb_windows.py <adb>... --tags 1H_Melee,Sword --fragment Attack_Primary
"""
import argparse
import json
import xml.etree.ElementTree as ET

# Parâmetro que identifica cada tipo de clipe procedural.
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
    """Variante mais específica do fragmento entre as que cabem nas tags (mesmo critério do adb_resolve).
    need_anims=False aceita fragmentos só com ProcLayer (escopo da arma, ex. playermaleanims_1h_meleeattach.adb)."""
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
            # ExitTime é relativo ao início do clipe anterior da camada, vazio ou não (o vazio só encerra o anterior).
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
        raise SystemExit("fragmento não encontrado")
    for w in windows(frag):
        end = f"{w['end']:.3f}" if w["end"] is not None else "fim"
        print(f"{w['start']:6.3f} → {end:>6}  {w['type']:26} {w['name']}")


if __name__ == "__main__":
    main()
