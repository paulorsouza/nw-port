#!/usr/bin/env python3
"""NPCs da capital de Queda Eterna: posição no mapa L_QuedaEterna, aparência, animação parada e nome.

    python3 tools/make_npcs.py

Fontes:
- capitals das regiões r_+04_+01 e r_+04_+02 a até RADIUS m da capital `ef_persistence`
  (settlement_northwindsward_complete);
- `AmbientNPC_MasterVariant`: a variante vem no próprio capital (`variantName`);
- `CoatGen/<capital>/MasterNpc_*Spawner_*.dynamicslice` (extract/slicejson, etapa sistemas-02 do build_all): a
  variante é o `m_slicevariant`;
- `CoatGen/<capital>/NPC_*.dynamicslice`: o slice já traz o `.cdf`, o nome (`m_name`) e as timelines;
- `javelindata_variations_npcs` (+ `_walkaway`): `.cdf`, IdleTimeline e Name de cada variante;
- textos em extract/loc/localization/<idioma>.

Sem as missões do jogo (decisão do dono, docs/sistemas.md): ficam os NPCs do estado inicial, sem conquista nenhuma.
`MasterNpc_SwapHidden_*` aparece (some depois de uma conquista) e `MasterNpc_SwapShown_*` não (só aparece depois).

Saída: unreal/NWPort/Data/World/queda_eterna_npcs.json
    {"npcs": [{"variant", "npc_id", "name": {"pt-br", "en-us"}, "cdf", "idle", "pos": [x, y, z], "yaw"}]}
"""
import collections
import glob
import json
import math
import os
import re
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")
REGIONS = os.path.join(ROOT, "extract", "terrain", "sharedassets", "coatlicue", "newworld_vitaeeterna", "regions")
SLICEJSON = os.path.join(ROOT, "extract", "slicejson")
LOC = os.path.join(ROOT, "extract", "loc", "localization")
TIMELINES = os.path.join(ROOT, "extract", "timelines")   # etapa sistemas-03 do build_all
CDFS = os.path.join(ROOT, "extract", "npcs")             # .cdf dos NPCs (tools/pesado.sh nwbt unpack, ver docs/sistemas.md)
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "queda_eterna_npcs.json")
SET = os.path.join(ROOT, "tools", "sets", "npc_queda_eterna.json")
REWARDS = os.path.join(ROOT, "tools", "npc_rewards.json")              # brindes por ofício (docs/sistemas.md)
REWARDS_OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "npc_rewards.json")
CITY_ID = "queda_eterna"
# Esqueletos que o port anima: o comum dos NPCs e o do jogador masculino (mesmos ossos). Outros (Grace O'Malley tem
# o próprio) ficam de fora até terem conjunto.
SKELETONS = ("objects/characters/npc/npc_skel.chr", "objects/characters/player/male/player_male.chr")
CITY = "settlement_northwindsward_complete"
RADIUS = 300.0  # m em volta da capital


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


def main():
    caps = []
    for f in sorted(glob.glob(os.path.join(REGIONS, "r_+04_+0[12]", "capitals", "*", "*.capitals.json"))):
        caps += json.load(open(f)).get("Capitals", [])
    city = next(c for c in caps if norm(c.get("sliceName")).endswith("/" + CITY))
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
        """Alias da animação parada (camada PlayAnim; os *_look_poses_* são aditivos de olhar)."""
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
            f = os.path.join(SLICEJSON, name + ".json")
            if not os.path.exists(f):
                skipped["sem slice: " + base] += 1
                continue
            s = open(f).read()
            m = re.search(r'"m_slicevariant"\s*:\s*"([^"]+)"', s)
            if m:
                variant = m.group(1)
            else:
                cdfs = re.findall(r'"([^"]+\.cdf)"', s)
                if not cdfs:
                    skipped["sem npc: " + base] += 1
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
                skipped["variante sem linha: " + variant] += 1
                continue
            cdf, idle, key = norm(v.get("CharacterDefinition")), norm(v.get("IdleTimeline")), v.get("Name") or ""
        name_pt = text(pt, key)
        if name_pt.upper() == "DEPRECATED":
            skipped["DEPRECATED: " + (variant or base)] += 1
            continue
        skel = skeleton(cdf)
        if skel not in SKELETONS:
            skipped[f"esqueleto {skel or '?'}: {variant or base}"] += 1
            continue
        q = c["rotation"]
        yaw = 2 * math.atan2(q["z"], q["w"])
        npcs.append({
            "variant": variant or base.rsplit("_", 1)[0],
            "npc_id": (variants.get(variant) or {}).get("NPCId", "") if variant else "",
            "name": {"pt-br": name_pt, "en-us": text(en, key)},
            "cdf": cdf,
            "mesh": "SK_" + os.path.splitext(os.path.basename(cdf))[0],
            "idle": idle,
            "anim": idle_anim(idle),
            "pos": to_map(p["x"], p["y"], p["z"]),
            # Giro no mapa: o do mundo menos o da cidade, espelhado em X (Cry → UE), como em make_spawns.py.
            "yaw": round(180 - math.degrees(yaw - ang), 1),
        })
    # Conjunto das animações (build_set.py): personagem base com npc_skel, um laço por animação parada.
    anims = sorted({n["anim"] for n in npcs if n["anim"]})
    base = next(n["cdf"] for n in npcs if skeleton(n["cdf"]) == SKELETONS[0])
    spec = {
        "name": "NpcQuedaEterna",
        "mesh_name": "SK_NpcQuedaEterna",
        "character": base,
        "_comment": "Gerado por tools/make_npcs.py: animações paradas dos NPCs de Queda Eterna (timelines IdleTimeline).",
        "weapons": {}, "blendspaces": {}, "montages": {},
        # export_ue.py exige um laço "Idle" (pose de referência das montagens): a parada genérica dos NPCs.
        "loops": {"Idle": ["settle_stand_idle"], **{a: [a] for a in anims}},
    }
    # Arquivo versionado: LF tambem no Windows, para regenerar sem sujar o diff.
    json.dump(spec, open(SET, "w", newline="\n"), indent=1)
    # Ofício de cada NPC (grupo do sorteio semanal) e cópia dos brindes para o jogo.
    rewards = json.load(open(REWARDS))
    for n in npcs:
        n["tipo"] = rewards["npcs"].get(n["variant"], "")
    sem = [n["variant"] for n in npcs if n["name"]["pt-br"] and not n["tipo"]]
    if sem:
        raise SystemExit(f"NPC com nome sem ofício em {REWARDS}: {sem}")
    json.dump({k: v for k, v in rewards.items() if k != "_comment"}, open(REWARDS_OUT, "w"), ensure_ascii=False, indent=1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    # Campo de treino dos tutoriais de arma (tools/tutorials.json, "bonecos").
    dummies = json.load(open(os.path.join(ROOT, "tools", "tutorials.json"))).get("bonecos", {}).get(CITY_ID, [])
    json.dump({"city": CITY_ID, "npcs": npcs, "dummies": dummies}, open(OUT, "w"), ensure_ascii=False, indent=1)
    cdfs = collections.Counter(n["cdf"] for n in npcs)
    print(f"{SET}: base {base}, {len(anims)} animações")
    print(f"{OUT}: {len(npcs)} NPCs, {len(cdfs)} aparências, {len(set(n['idle'] for n in npcs))} timelines de parado")
    for k, v in sorted(skipped.items()):
        print(f"  pulados {v}: {k}")


if __name__ == "__main__":
    main()
