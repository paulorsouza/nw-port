#!/usr/bin/env python3
"""Gera os assets de um conjunto (personagem + clipes + armas) prontos para o Unreal.

uso: build_set.py tools/sets/sword_shield.json [--paths extract/manifest/paths.txt]

Saída em extract/ue/<nome>/:
  glb/            saída bruta do nwbt
  fbx/            SK_*.fbx, SM_*.fbx, anim/A_*.fbx
  manifest.json   blend spaces, montagens, loops e sockets para o script do Unreal
Precisa de `source tools/env.sh` (nwbt e blender no PATH) e de GAME apontando para o jogo.
"""
import argparse
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adb_windows  # noqa: E402
import setspec  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANIM_ROOT = "animations/gameplay/character/player/male/"
GAME = os.environ.get("GAME", "/mnt/jogos/Steam/steamapps/common/New World")


def index(paths_file, anim_root=ANIM_ROOT):
    idx = {}
    with open(paths_file) as f:
        for line in f:
            p = line.strip()
            if p.startswith(anim_root) and p.endswith((".caf", ".bspace")):
                name, ext = os.path.splitext(os.path.basename(p))
                key = (name.lower(), ext)
                idx.setdefault(key, p)
    return idx


def nwbt_cat(path):
    return subprocess.run(["nwbt", "cat", "-g", GAME, path], capture_output=True, check=True).stdout


def read_bspace(path):
    """Dimensões e exemplos de um .bspace. Exemplos sem um parâmetro herdam 0 (como no jogo)."""
    root = ET.fromstring(nwbt_cat(path))
    dims = [{"name": p.get("Name"), "min": float(p.get("Min")), "max": float(p.get("Max"))}
            for p in root.find("Dimensions")]
    samples = []
    for ex in root.find("ExampleList"):
        vals = [float(ex.get(f"SetPara{i}", 0)) for i in range(len(dims))]
        # Parâmetro ausente: o motor do jogo calcula a partir do próprio clipe (ex.: MoveSpeed = velocidade do clipe).
        explicit = [ex.get(f"SetPara{i}") is not None for i in range(len(dims))]
        rate = float(ex.get("PlaybackScale", 1))
        samples.append({"clip": ex.get("AName").lower(), "params": vals, "explicit": explicit, "rate": rate})
    return {"source": path, "dimensions": dims, "samples": samples}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("set")
    ap.add_argument("--paths", default=os.path.join(ROOT, "extract", "manifest", "paths.txt"))  # nwbt list | awk '{print $NF}'
    a = ap.parse_args()

    spec = setspec.load(a.set)
    out = os.path.join(ROOT, "extract", "ue", spec["name"])
    os.makedirs(out, exist_ok=True)
    idx = index(a.paths, spec.get("anim_root", ANIM_ROOT))

    composites = {}

    def composite(layers, loop):
        layers = [c.lower() for c in dict.fromkeys(layers)]
        name = "+".join(layers)
        composites.setdefault(name, {"layers": layers, "loop": loop})
        return name

    manifest = {"name": spec["name"], "fps": 30, "straighten_loops": spec.get("straighten_loops", False),
                "mesh_name": spec.get("mesh_name", "SK_player_male"), "weapons": spec["weapons"], "blendspaces": {},
                "loops": {k: composite(v, True) for k, v in spec["loops"].items()},
                "montages": {k: [composite(sec, False) for sec in v] for k, v in spec["montages"].items()},
                "inplace_montages": spec.get("inplace_montages", []),
                "hold_montages": spec.get("hold_montages", []),
                "upper_montages": spec.get("upper_montages", []),
                "montage_starts": spec.get("montage_starts", {})}

    for bs_name, layers in spec["blendspaces"].items():
        spaces = []
        for bspace in layers:
            p = idx.get((bspace.lower(), ".bspace"))
            if not p:
                sys.exit(f"blend space não encontrado: {bspace}")
            spaces.append(read_bspace(p))
        base_bs = spaces[0]
        spans = [max(d["max"] - d["min"], 1e-6) for d in base_bs["dimensions"]]

        def nearest(bs, params):
            # Casa a amostra da camada pela distância normalizada nos parâmetros da base.
            n = min(len(params), len(bs["dimensions"]))
            return min(bs["samples"], key=lambda s: sum(((s["params"][i] - params[i]) / spans[i]) ** 2 for i in range(n)))["clip"]

        samples = []
        for smp in base_bs["samples"]:
            layers_clips = [smp["clip"]] + [nearest(bs, smp["params"]) for bs in spaces[1:]]
            samples.append({"params": smp["params"], "explicit": smp["explicit"], "rate": smp["rate"],
                            "anim": composite(layers_clips, True)})
        manifest["blendspaces"][bs_name] = {"dimensions": base_bs["dimensions"], "sources": [b["source"] for b in spaces],
                                            "samples": samples}

    # Nome de asset curto (ações do Blender têm limite de 63 caracteres): A_<camada base>[_n].
    used = {}
    for key in sorted(composites):
        b = composites[key]["layers"][0]
        used[b] = used.get(b, 0) + 1
        composites[key]["asset"] = f"A_{b}" if used[b] == 1 else f"A_{b}_{used[b]}"
    clips = sorted({c for comp in composites.values() for c in comp["layers"]})
    manifest["composites"] = composites
    files, missing = [], []
    for c in clips:
        p = idx.get((c, ".caf"))
        (files.append(p) if p else missing.append(c))
    if missing:
        sys.exit(f"clipes sem .caf: {missing}")
    manifest["clips"] = clips
    print(f"{len(files)} clipes, {len(composites)} animações compostas, {len(manifest['blendspaces'])} blend spaces, "
          f"{len(manifest['montages'])} montagens")

    # Janelas de combate (ProcLayer do Mannequin) por seção de montagem.
    adb_dir = os.path.join(ROOT, "extract", "models", "animations", "mannequin", "adb")
    adbs = [os.path.join(adb_dir, f) for f in spec.get("window_adbs", ["player/player_sword.adb", "player/player_1h_melee.adb", "player/player.adb"])]
    wtags = set(spec.get("window_tags", "").split(","))
    manifest["windows"] = {}
    # Velocidade do clipe no fragmento (Animation speed="1.1"): a montagem toca nessa taxa e as janelas, que estão em
    # tempo real, vão para o tempo da sequência (× velocidade) no import.
    manifest["montage_rates"] = {}
    overrides = spec.get("window_overrides", {})
    for montage, frags in spec.get("fragments", {}).items():
        secs = []
        m_adbs, m_tags = adbs, wtags
        m_attach = [os.path.join(adb_dir, f) for f in spec.get("window_attach_adbs", [])]
        for prefix, ov in overrides.items():
            if montage.startswith(prefix):
                m_adbs = [os.path.join(adb_dir, f) for f in ov["adbs"]]
                m_tags = set(ov["tags"].split(","))
                if "attach_adbs" in ov:
                    m_attach = [os.path.join(adb_dir, f) for f in ov["attach_adbs"]]
        for frag_name in frags:
            frag = adb_windows.best_fragment(m_adbs, m_tags, frag_name)
            wins = adb_windows.windows(frag) if frag is not None else []
            # Escopo da arma (attach_adbs): fragmentos só com janelas; na rapieira o dano dos ataques básicos mora lá.
            for att in m_attach:
                af = adb_windows.best_fragment([att], m_tags, frag_name, need_anims=False)
                if af is not None:
                    wins = sorted(wins + adb_windows.windows(af), key=lambda w: w["start"])
            rate = 1.0
            if frag is not None:
                first = frag.find("AnimLayer/Animation")
                if first is not None and first.get("speed"):
                    rate = float(first.get("speed"))
            secs.append({"fragment": frag_name, "windows": wins, "rate": rate})
        manifest["windows"][montage] = secs
        if any(sc["rate"] != 1.0 for sc in secs):
            manifest["montage_rates"][montage] = [sc["rate"] for sc in secs]
    print(f"janelas: {sum(len(w['windows']) for v in manifest['windows'].values() for w in v)} em {len(manifest['windows'])} montagens")

    glb_dir = os.path.join(out, "glb")
    weapon_files = [w["file"] for w in spec["weapons"].values()]
    subprocess.run(["nwbt", "models", "files", "-g", GAME, "-o", glb_dir, "--binary", "--embed", "--ior", "1.5",
                    spec["character"], *([spec["armor"]] if spec.get("armor") else []), *files, *weapon_files],
                   check=True, capture_output=True)

    manifest["glb"] = {
        "character": os.path.join(glb_dir, spec["character"] + ".glb"),
        "armor": os.path.join(glb_dir, spec["armor"] + ".glb") if spec.get("armor") else None,
        "weapons": {k: os.path.join(glb_dir, w["file"] + ".glb") for k, w in spec["weapons"].items()},
    }
    mpath = os.path.join(out, "manifest.json")
    json.dump(manifest, open(mpath, "w"), indent=1)

    subprocess.run(["blender", "-b", "-P", os.path.join(ROOT, "tools", "export_ue.py"), "--", mpath], check=True)
    print(f"ok: {out}")


if __name__ == "__main__":
    main()
