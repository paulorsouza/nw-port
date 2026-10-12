#!/usr/bin/env python3
"""Liga o áudio extraído (extract/audio) às animações do conjunto.

Fontes, por animação composta:
  - janelas Audio do ProcLayer (.adb): Play_* e as vozes de esforço (Set_Switch_Vocals_*);
  - .animevents do jogo: passos (footstep) nos clipes de cada camada, com o osso do pé;
  - reações fixas: impacto e voz de dor nos Hit_*, som do escudo no bloqueio, quebra de guarda, morte.

uso: make_sound_map.py extract/ue/sword_shield/manifest.json [--surface grass]
Saída: extract/ue/<conjunto>/sounds.json  {asset: [{time, files[], bone, label}]}
"""
import argparse
import csv
import json
import os
import subprocess
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO = os.path.join(ROOT, "extract", "audio")
GAME = os.environ.get("GAME", "/mnt/jogos/Steam/steamapps/common/New World")
ANIMEVENTS = "animations/gameplay/character/player/male/temp_male.animevents"

# Troca de voz do Wwise -> pasta de voz extraída.
VOCALS = {
    "set_switch_vocals_exerta": "combat/voice_male/vo_exerta",
    "set_switch_vocals_exertb": "combat/voice_male/vo_exertb",
    "set_switch_vocals_landsoft": "combat/voice_male/vo_landsoft",
}
# Reações (montagem -> sons no início). O alvo usa Heroic Heavy: impacto de espada em armadura.
REACTIONS = {
    "AM_Hit_": [(0.0, "combat/impacts/imp_swordmetal_armor", "impacto"), (0.05, "combat/voice_male/vo_hurt", "dor")],
    "AM_BlockReact": [(0.0, "combat/impacts/block_wpn_metal_med_vs_shield_metal_med", "bloqueio")],
    "AM_BlockBreak": [(0.0, "combat/impacts/gpui_blockbreaker", "guarda quebrada")],
    "AM_Death": [(0.0, "combat/voice_male/vo_death", "morte")],
}


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
            # O nome traz "decals=corruption" em quase tudo (o extrator guarda o 1º valor do state); vale o surfacetype.
            for want in (surface, "dirt", "default"):
                hit = [r for r in rows if f"[surfacetype={want}]" in r["txtp"]]
                if hit:
                    rows = sorted(hit, key=lambda r: "decals=" in r["txtp"])
                    break
        return [os.path.join(AUDIO, r["file"]) for r in rows][:MAX_VARIATIONS]

    # Passos por clipe (caminho completo do .caf -> eventos).
    xml = subprocess.run(["nwbt", "cat", "-g", GAME, ANIMEVENTS], capture_output=True, check=True).stdout
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

    # Passos: da camada base de cada animação composta (é ela que move as pernas).
    fps = m["fps"]
    for comp in m["composites"].values():
        base_clip = comp["layers"][0]
        # Tempo do .animevents é normalizado (0–1) pela duração do clipe.
        duration = (comp["frames"] - 1) / fps
        for t, param, bone in steps.get(base_clip, []):
            files = event_files(param, a.surface)
            if not files:
                missing.add(param)
            add(comp["asset"], t * duration, files, bone, param)

    # Janelas Audio do .adb nas montagens.
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

    # Reações.
    for montage, keys in m["montages"].items():
        for prefix, sounds in REACTIONS.items():
            if montage.startswith(prefix):
                asset = m["composites"][keys[0]]["asset"]
                for t, rel, label in sounds:
                    add(asset, t, folder_files(rel), "", label)

    path = os.path.join(os.path.dirname(a.manifest), "sounds.json")
    json.dump(out, open(path, "w"), indent=1)
    n = sum(len(v) for v in out.values())
    files = {f for v in out.values() for e in v for f in e["files"]}
    print(f"{n} sons em {len(out)} animações, {len(files)} arquivos .ogg -> {path}")
    if missing:
        print(f"sem áudio: {sorted(missing)}")


if __name__ == "__main__":
    main()
