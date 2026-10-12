"""Malha do terreno do mundo sob uma cidade, no referencial do glb da cidade (`nwbt models slices`).

    python3 tools/make_terrain.py <terrain_dir> <capitals.json> <slice> <saida.glb> [--size 768] [--tile 128]

<terrain_dir> é a pasta do `nwbt unpack` com sharedassets/coatlicue/<level>/regions e materials/terrain.
Lê a posição da cidade no capitals.json (worldPosition + rotation do capital cujo sliceName termina em <slice>).

Fatos usados:
- region.heightmap: TIFF uint16 2048×2048, 1 amostra por metro, origem no canto superior esquerdo
  (linha 0 = y máximo da região); altura em metros = valor / 32.
- <regiao>.roads_splat: mesma grade e orientação do heightmap; vira cor de vértice R (peso de terra).
- glb do nwbt: glTF = (-x, z, y) do CryEngine.
"""
import argparse
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

REGION = 2048
HEIGHT_SCALE = 1 / 32
NWBT_DIR = Path(__file__).resolve().parent.parent / "vendor/nw-buddy/tools/nwbt"


def find_capital(path, slice_name):
    for c in json.load(open(path))["Capitals"]:
        if c.get("sliceName", "").lower().endswith(slice_name.lower()):
            return c
    raise SystemExit(f"{slice_name} não está em {path}")


def load_region(terrain_dir, level, rx, ry, cache):
    """Heightmap (m) e peso de estrada (0–1) da região, ou None se não existir."""
    key = (rx, ry)
    if key in cache:
        return cache[key]
    tiff = terrain_dir / f"sharedassets/coatlicue/{level}/regions/r_+{rx:02d}_+{ry:02d}/region.heightmap"
    if not tiff.exists():
        cache[key] = None
        return None
    r16 = tiff.with_suffix(".r16")
    if not r16.exists():
        subprocess.run(["go", "run", "./dbgheight", str(tiff.resolve()), str(r16.resolve())], cwd=NWBT_DIR, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    h = np.fromfile(r16, dtype="<u2").reshape(REGION, REGION).astype(np.float32) * HEIGHT_SCALE
    splat = terrain_dir / f"materials/terrain/{level}/jav_{level}_{rx}_{ry}/jav_{level}_{rx}.{ry}.roads_splat.png"
    if splat.exists():
        s = np.asarray(Image.open(splat).convert("L").resize((REGION, REGION)), dtype=np.float32) / 255
    else:
        s = np.zeros_like(h)
    cache[key] = (h, s)
    return cache[key]


def sample(terrain_dir, level, wx, wy, cache):
    """Altura e peso de estrada bilineares nas posições de mundo (arrays)."""
    hs, ss = np.zeros_like(wx), np.zeros_like(wx)
    rx, ry = (wx // REGION).astype(int), (wy // REGION).astype(int)
    for key in set(zip(rx.ravel().tolist(), ry.ravel().tolist())):
        reg = load_region(terrain_dir, level, *key, cache)
        if reg is None:
            continue
        m = (rx == key[0]) & (ry == key[1])
        x = np.clip(wx[m] - key[0] * REGION, 0, REGION - 1.001)
        y = np.clip(wy[m] - key[1] * REGION, 0, REGION - 1.001)
        row = (REGION - 1) - y           # linha 0 = topo (y máximo)
        r0, c0 = np.floor(row).astype(int), np.floor(x).astype(int)
        r1, c1 = np.minimum(r0 + 1, REGION - 1), np.minimum(c0 + 1, REGION - 1)
        fr, fc = row - r0, x - c0
        for out, a in ((hs, reg[0]), (ss, reg[1])):
            out[m] = (a[r0, c0] * (1 - fr) * (1 - fc) + a[r0, c1] * (1 - fr) * fc
                      + a[r1, c0] * fr * (1 - fc) + a[r1, c1] * fr * fc)
    return hs, ss


class GlbWriter:
    def __init__(self):
        self.bin = bytearray()
        self.j = {"asset": {"version": "2.0", "generator": "nw-port make_terrain"},
                  "buffers": [], "bufferViews": [], "accessors": [], "meshes": [], "nodes": [],
                  "materials": [{"name": "M_Terreno", "pbrMetallicRoughness": {"metallicFactor": 0, "roughnessFactor": 1}}],
                  "scenes": [{"nodes": []}], "scene": 0}

    def accessor(self, arr, target, kind):
        arr = np.ascontiguousarray(arr)
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += arr.tobytes()
        self.j["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes, "target": target})
        acc = {"bufferView": len(self.j["bufferViews"]) - 1, "count": len(arr), "type": kind,
               "componentType": 5125 if arr.dtype == np.uint32 else 5126}
        if kind == "VEC3" and arr.dtype == np.float32:
            acc["min"], acc["max"] = arr.min(0).tolist(), arr.max(0).tolist()
        self.j["accessors"].append(acc)
        return len(self.j["accessors"]) - 1

    def mesh(self, name, pos, nrm, uv, col, idx):
        prim = {"attributes": {"POSITION": self.accessor(pos, 34962, "VEC3"),
                               "NORMAL": self.accessor(nrm, 34962, "VEC3"),
                               "TEXCOORD_0": self.accessor(uv, 34962, "VEC2"),
                               "COLOR_0": self.accessor(col, 34962, "VEC4")},
                "indices": self.accessor(idx, 34963, "SCALAR"), "material": 0}
        self.j["meshes"].append({"name": name, "primitives": [prim]})
        self.j["nodes"].append({"name": name, "mesh": len(self.j["meshes"]) - 1})
        self.j["scenes"][0]["nodes"].append(len(self.j["nodes"]) - 1)

    def save(self, path):
        self.j["buffers"] = [{"byteLength": len(self.bin)}]
        js = json.dumps(self.j).encode()
        js += b" " * (-len(js) % 4)
        while len(self.bin) % 4:
            self.bin.append(0)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(self.bin)))
            f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
            f.write(struct.pack("<II", len(self.bin), 0x004E4942) + self.bin)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("terrain_dir", type=Path)
    ap.add_argument("capitals")
    ap.add_argument("slice")
    ap.add_argument("out")
    ap.add_argument("--size", type=int, default=768, help="lado do quadrado em metros")
    ap.add_argument("--tile", type=int, default=128, help="lado de cada malha em metros")
    ap.add_argument("--uv", type=float, default=4.0, help="metros por repetição da textura")
    ap.add_argument("--region", help="cobre a região inteira (ex.: r_+04_+01) em vez do quadrado em volta da cidade")
    a = ap.parse_args()

    cap = find_capital(a.capitals, a.slice)
    level = Path(a.capitals).parts[Path(a.capitals).parts.index("coatlicue") + 1]
    T = cap["worldPosition"]
    q = cap["rotation"]
    if abs(q["x"]) > 1e-4 or abs(q["y"]) > 1e-4:
        sys.exit("rotação do capital fora do eixo Z não suportada")
    ang = 2 * math.atan2(q["z"], q["w"])
    c, s = math.cos(ang), math.sin(ang)
    cache = {}
    glb = GlbWriter()
    half, n = a.size // 2, a.tile
    if a.region:
        # Ladrilhos alinhados ao mundo sobre a região; vértices levados ao referencial da cidade.
        rx, ry = (int(t) for t in a.region.split("_")[1:3])
        tiles = [(rx * REGION + i, ry * REGION + j) for j in range(0, REGION, n) for i in range(0, REGION, n)]
    else:
        tiles = [(tx, ty) for ty in range(-half, half, n) for tx in range(-half, half, n)]
    for tx, ty in tiles:
        if a.region:
            wx, wy = np.meshgrid(np.arange(tx, tx + n + 1, dtype=np.float64), np.arange(ty, ty + n + 1, dtype=np.float64))
            # A borda de cima/direita (x = fim da região) cai na região vizinha: segura dentro desta.
            wx = np.minimum(wx, (rx + 1) * REGION - 0.01)
            wy = np.minimum(wy, (ry + 1) * REGION - 0.01)
            dx, dy = wx - T["x"], wy - T["y"]
            lx, ly = (c * dx + s * dy).astype(np.float32), (-s * dx + c * dy).astype(np.float32)
        else:
            lx, ly = np.meshgrid(np.arange(tx, tx + n + 1, dtype=np.float32),
                                 np.arange(ty, ty + n + 1, dtype=np.float32))
            wx, wy = T["x"] + c * lx - s * ly, T["y"] + s * lx + c * ly
        if True:
            h, road = sample(a.terrain_dir, level, wx, wy, cache)
            lz = h - T["z"]
            # glTF = (-x, z, y) do CryEngine, no referencial local da cidade
            pos = np.stack([-lx, lz, ly], -1).reshape(-1, 3).astype(np.float32)
            dzx = np.gradient(lz, axis=1)
            dzy = np.gradient(lz, axis=0)
            nrm = np.stack([dzx, np.ones_like(lz), -dzy], -1)  # normal Cry (-dzx,-dzy,1) em glTF
            nrm = (nrm / np.linalg.norm(nrm, axis=-1, keepdims=True)).reshape(-1, 3).astype(np.float32)
            uv = np.stack([lx / a.uv, ly / a.uv], -1).reshape(-1, 2).astype(np.float32)
            col = np.stack([road, np.zeros_like(road), np.zeros_like(road), np.ones_like(road)], -1)
            col = col.reshape(-1, 4).astype(np.float32)
            w = n + 1
            i = np.arange(n)[None, :] + w * np.arange(n)[:, None]
            quads = np.stack([i, i + w, i + 1, i + 1, i + w, i + w + 1], -1).reshape(-1, 3)
            # vira o sentido se o primeiro triângulo não aponta para cima (+Y no glTF)
            p0, p1, p2 = pos[quads[0]]
            if np.cross(p1 - p0, p2 - p0)[1] < 0:
                quads = quads[:, ::-1]
            glb.mesh(f"terreno_{tx:05d}_{ty:05d}" if a.region else f"terreno_{tx + half:04d}_{ty + half:04d}", pos, nrm, uv, col,
                     np.ascontiguousarray(quads.reshape(-1).astype(np.uint32)))
    glb.save(a.out)

    # Ponto de partida: trecho de estrada mais perto do centro (estrada não tem construção em cima).
    lx, ly = np.meshgrid(np.arange(-80, 81, 2, dtype=np.float32), np.arange(-80, 81, 2, dtype=np.float32))
    h, road = sample(a.terrain_dir, level, T["x"] + c * lx - s * ly, T["y"] + s * lx + c * ly, cache)
    dist = np.hypot(lx, ly) + (road < 0.6) * 1e6
    k = np.unravel_index(np.argmin(dist), dist.shape)
    spawn_ue = [float(-lx[k] * 100), float(ly[k] * 100), float((h[k] - T["z"] + 1.5) * 100)]  # UE = (-x, y, z) × 100
    json.dump({"spawn": spawn_ue, "center_height_cm": float((sample(a.terrain_dir, level, np.array([T["x"]]),
               np.array([T["y"]]), cache)[0][0] - T["z"]) * 100)}, open(Path(a.out).with_suffix(".json"), "w"))
    print(f"partida (UE cm): {[round(v) for v in spawn_ue]}")
    print(f"{a.out}: {len(glb.j['meshes'])} malhas, {a.size} m, centro {T['x']:.1f},{T['y']:.1f}, giro {math.degrees(ang):.1f}°")


if __name__ == "__main__":
    main()
