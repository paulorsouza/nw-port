"""Coletáveis de Guardaventos a partir das camadas do mundo e das tabelas do jogo (F4/G1).

    python3 tools/make_gatherables.py [--size 768]

Lê capitals/06swindsward_gatherables (posição, giro, escala, variante) e region.distribution (os ~99 mil
arbustos, árvores, pedras, minérios e plantas que o jogo espalha pela paisagem: só x/y; altura do heightmap,
giro aleatório fixo por ponto) da região r_+04_+01 e as tabelas
variations_gatherables_* (malha cheia/esgotada, raios) e gatherables (ofício, ferramenta, tempo, saque,
respawn). Converte para o referencial do mapa L_SouthWindsward: o mesmo de tools/make_terrain.py (local da
cidade, UE = (-x, y, z) × 100). --size limita ao quadrado do terreno importado (0 = território inteiro).

Gera unreal/NWPort/Data/World/guardaventos_gatherables.json e extract/ue/gatherables/meshes.txt (malhas a
converter com tools/convert_gatherables.sh).
"""
import argparse
import glob
import json
import math
import os
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_terrain  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "extract", "data")
REGION = glob.glob(os.path.join(ROOT, "extract", "terrain", "**", "r_+04_+01", "capitals"), recursive=True)[0]
OUT = os.path.join(ROOT, "unreal", "NWPort", "Data", "World", "guardaventos_gatherables.json")
MESHES = os.path.join(ROOT, "extract", "ue", "gatherables", "meshes.txt")
CITY = "Settlement_SouthWindsward_Complete"
# Variações que não são coleta de mundo aberto.
SKIP_KINDS = {"lootcontainers"}


def rows(name):
    return json.load(open(glob.glob(os.path.join(DATA, "**", f"javelindata_{name}.datasheet.json"), recursive=True)[0]))["rows"]


def capitals(layer):
    return json.load(open(glob.glob(os.path.join(REGION, layer, "*.capitals.json"))[0]))["Capitals"]


def mesh_path(p):
    """Caminho do .cgf como está no pak (minúsculo, barras normais)."""
    return str(p or "").replace("\\", "/").lower()


def distribution(region_dir):
    """region.distribution (formato do nw-buddy, formats/distribution): [(variante, x, y)] em metros de mundo."""
    d = open(os.path.join(region_dir, "region.distribution"), "rb").read()
    rx, ry = (int(t) for t in os.path.basename(region_dir).split("_")[1:3])
    o = 4 if d[:4] == bytes([0xD6, 0xB9, 0x01, 0x00]) else 0
    n = struct.unpack_from("<H", d, o)[0]
    o += 2

    def strings(k):
        nonlocal o
        out = []
        for _ in range(k):
            c = d[o]
            out.append(d[o + 1:o + 1 + c].decode())
            o += 1 + c
        return out

    _slices, variants = strings(n), strings(n)
    count = struct.unpack_from("<I", d, o)[0]
    o += 4
    idx = struct.unpack_from(f"<{count}H", d, o)
    o += 2 * count
    pos = struct.unpack_from(f"<{2 * count}H", d, o)  # (y, x) por ponto
    return [(variants[idx[i]], rx * 2048 + pos[2 * i + 1] / 65535 * 2048, ry * 2048 + pos[2 * i] / 65535 * 2048)
            for i in range(count)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=float, default=0, help="lado do quadrado em metros (0 = território inteiro)")
    a = ap.parse_args()

    city = next(c for c in capitals("06swindsward_persistence") if c["sliceName"].endswith(CITY))
    T = city["worldPosition"]
    ang = 2 * math.atan2(city["rotation"]["z"], city["rotation"]["w"])
    ca, sa = math.cos(-ang), math.sin(-ang)
    qcity = (0.0, 0.0, math.sin(-ang / 2), math.cos(-ang / 2))  # giro que leva o mundo ao local da cidade

    variants = {}
    for f in glob.glob(os.path.join(DATA, "**", "javelindata_variations_gatherables_*.datasheet.json"), recursive=True):
        kind = os.path.basename(f).split("_gatherables_")[1].split(".")[0]
        for r in json.load(open(f))["rows"]:
            variants.setdefault(r["VariantID"], (kind, r))
    gatherables = {r["GatherableID"]: r for r in rows("gatherables")}

    by_lower = {k.lower(): k for k in variants}
    points = []  # (variante, x, y, z, quat do mundo (x, y, z, w), escala)
    for c in capitals("06swindsward_gatherables"):
        p, q = c["worldPosition"], c["rotation"]
        points.append((c.get("variantName"), p["x"], p["y"], p["z"], (q["x"], q["y"], q["z"], q["w"]),
                       float(c.get("scale") or 1.0)))
    dist = distribution(os.path.dirname(REGION))
    xs = np.array([p[1] for p in dist])
    ys = np.array([p[2] for p in dist])
    hs, _ = make_terrain.sample(Path(os.path.join(ROOT, "extract", "terrain")), "newworld_vitaeeterna", xs, ys, {})
    rng = np.random.default_rng(406)  # giro aleatório, igual em toda geração
    yaws = rng.uniform(0, 2 * math.pi, len(dist))
    for (v, x, y), h, yaw in zip(dist, hs, yaws):
        points.append((v, x, y, float(h), (0.0, 0.0, math.sin(yaw / 2), math.cos(yaw / 2)), 1.0))

    out_variants, order, instances, meshes = {}, {}, [], set()
    for v, wx, wy, wz, q, scale in points:
        v = by_lower.get(str(v or "").lower())
        if not v:
            continue
        kind, r = variants[v]
        g = gatherables.get(r.get("GatherableEntryID"))
        if kind in SKIP_KINDS or not g or not r.get("StartMesh"):
            continue
        dx, dy = wx - T["x"], wy - T["y"]
        lx, ly, lz = ca * dx - sa * dy, sa * dx + ca * dy, wz - T["z"]
        start, depleted = mesh_path(r.get("StartMesh")), mesh_path(r.get("DepletedMesh"))
        meshes.update(m for m in (start, depleted) if m)  # malhas do território inteiro, mesmo recortado
        if a.size and (abs(lx) > a.size / 2 or abs(ly) > a.size / 2):
            continue
        # q_local = q_cidade⁻¹ · q_mundo; espelho em X (Cry → UE) = (x, -y, -z, w)
        x1, y1, z1, w1 = qcity
        x2, y2, z2, w2 = q
        ql = (w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
              w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2)
        if v not in out_variants:
            order[v] = len(order)
            out_variants[v] = {
                "index": order[v], "kind": kind, "gatherable": g["GatherableID"],
                "mesh": start, "depleted_mesh": depleted,
                "interaction_radius": float(r.get("InteractionRadius") or 3) * 100,
                "name_key": str(g.get("DisplayName") or ""),
                "tradeskill": str(g.get("Tradeskill") or "None"),
                "action": str(g.get("GatheringAction") or ""),
                "restriction": str(g.get("Restriction") or "NoToolNecessary"),
                "time": float(g.get("BaseGatherTime") or 0),
                "loot": str(g.get("FinalLootTable") or ""),
                "respawn": [float(g.get("MinRespawnRate") or 0), float(g.get("MaxRespawnRate") or 0)],
                "compass_icon": str(r.get("CompassIcon") or "").replace("\\", "/").lower(),
            }
        # Compacto (100 mil pontos): [variante, x, y, z, qx, qy, qz, qw, escala] em cm no referencial do mapa.
        instances.append([order[v], round(-lx * 100, 1), round(ly * 100, 1), round(lz * 100, 1),
                          round(ql[0], 4), round(-ql[1], 4), round(-ql[2], 4), round(ql[3], 4), round(scale, 3)])

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({"variants": out_variants, "instances": instances}, open(OUT, "w"), separators=(",", ":"))
    os.makedirs(os.path.dirname(MESHES), exist_ok=True)
    open(MESHES, "w").write("\n".join(sorted(meshes)) + "\n")
    kinds = {}
    for i in instances:
        k = next(o["kind"] for o in out_variants.values() if o["index"] == i[0])
        kinds[k] = kinds.get(k, 0) + 1
    print(f"{len(instances)} coletáveis {kinds}, {len(out_variants)} variantes, {len(meshes)} malhas -> {OUT}")


if __name__ == "__main__":
    main()
