"""Malha da água (rios, lagos, cachoeiras) de uma região, no referencial do mapa da cidade.

    python3 tools/make_water.py <region.f32> <capitals.json> <slice> <saida.glb> [--step 2]

<region.f32> = altura da água por metro, de `dbgwater` (nwbt, region.waterqt → float32 LE, linha 0 = y máximo,
como o heightmap); sem água = valor <= 0. Gera uma malha por ladrilho de 128 m com quadrados de --step metros
onde a água fica acima do terreno. Onde o nível muda de um quadrado para o vizinho a superfície fica inclinada:
são as cachoeiras. Mesmo referencial e eixos do tools/make_terrain.py (glTF = (−x, z, y) do CryEngine, local da
cidade). Cor de vértice R = 1 nos degraus (cachoeira), 0 na água parada.
"""
import argparse
import math
import os
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_terrain import REGION, GlbWriter, find_capital, sample  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("water")
    ap.add_argument("capitals")
    ap.add_argument("slice")
    ap.add_argument("out")
    ap.add_argument("--region", default="r_+04_+01")
    ap.add_argument("--step", type=int, default=2)
    a = ap.parse_args()

    cap = find_capital(a.capitals, a.slice)
    level = Path(a.capitals).parts[Path(a.capitals).parts.index("coatlicue") + 1]
    parts = Path(a.capitals).parts
    terrain_dir = Path(*parts[:parts.index("sharedassets")])
    T = cap["worldPosition"]
    ang = 2 * math.atan2(cap["rotation"]["z"], cap["rotation"]["w"])
    c, s = math.cos(ang), math.sin(ang)
    rx, ry = (int(t) for t in a.region.split("_")[1:3])

    water = np.fromfile(a.water, dtype="<f4").reshape(REGION, REGION)  # linha 0 = y máximo
    st = a.step
    # Amostra nos vértices da grade (st metros), coluna x / linha y do mundo.
    xs = np.arange(0, REGION + 1, st)
    ys = np.arange(0, REGION + 1, st)
    col = np.clip(xs, 0, REGION - 1)
    row = (REGION - 1) - np.clip(ys, 0, REGION - 1)
    W = water[row[:, None], col[None, :]]                                     # [y, x]
    wx, wy = np.meshgrid(rx * REGION + np.minimum(xs, REGION - 0.01), ry * REGION + np.minimum(ys, REGION - 0.01))
    H, _ = sample(terrain_dir, level, wx, wy, {})
    wet = W > 0
    # Vértice seco ao lado de molhado herda o nível do vizinho (a borda entra um pouco sob o terreno).
    Wf = np.where(wet, W, 0.0)
    for _ in range(2):
        pad = np.pad(Wf, 1, mode="edge")
        neigh = np.max(np.stack([pad[1:-1, :-2], pad[1:-1, 2:], pad[:-2, 1:-1], pad[2:, 1:-1]]), axis=0)
        Wf = np.where(Wf > 0, Wf, neigh)

    glb = GlbWriter()
    tile = 128 // st
    quads_total = 0
    for ty in range(0, len(ys) - 1, tile):
        for tx in range(0, len(xs) - 1, tile):
            sl = (slice(ty, min(ty + tile, len(ys) - 1) + 1), slice(tx, min(tx + tile, len(xs) - 1) + 1))
            w, h = Wf[sl], H[sl]
            n_y, n_x = w.shape
            # Quadrado entra se algum canto é água original acima do terreno.
            above = (W[sl] > 0) & (W[sl] > h + 0.05)
            q = above[:-1, :-1] | above[1:, :-1] | above[:-1, 1:] | above[1:, 1:]
            q &= (w[:-1, :-1] > 0) & (w[1:, :-1] > 0) & (w[:-1, 1:] > 0) & (w[1:, 1:] > 0)
            if not q.any():
                continue
            gx, gy = wx[sl], wy[sl]
            dx, dy = gx - T["x"], gy - T["y"]
            lx, ly = (c * dx + s * dy).astype(np.float32), (-s * dx + c * dy).astype(np.float32)
            lz = (w - T["z"]).astype(np.float32)
            pos = np.stack([-lx, lz, ly], -1).reshape(-1, 3)
            # Degrau = diferença de nível com o vizinho > 0,5 m (cachoeira).
            step_mask = np.zeros_like(w, dtype=np.float32)
            dzx = np.abs(np.diff(w, axis=1)) > 0.5
            dzy = np.abs(np.diff(w, axis=0)) > 0.5
            step_mask[:, :-1] += dzx
            step_mask[:, 1:] += dzx
            step_mask[:-1, :] += dzy
            step_mask[1:, :] += dzy
            colr = np.clip(step_mask, 0, 1).reshape(-1)
            colors = np.stack([colr, np.zeros_like(colr), np.zeros_like(colr), np.ones_like(colr)], -1).astype(np.float32)
            gz_x = np.gradient(lz, axis=1) / st
            gz_y = np.gradient(lz, axis=0) / st
            nrm = np.stack([gz_x, np.ones_like(lz), -gz_y], -1)
            nrm = (nrm / np.linalg.norm(nrm, axis=-1, keepdims=True)).reshape(-1, 3).astype(np.float32)
            uv = np.stack([lx / 8.0, ly / 8.0], -1).reshape(-1, 2).astype(np.float32)
            idx = np.arange(n_y * n_x).reshape(n_y, n_x)
            a0, a1, b0, b1 = idx[:-1, :-1][q], idx[:-1, 1:][q], idx[1:, :-1][q], idx[1:, 1:][q]
            tris = np.stack([a0, b0, a1, a1, b0, b1], -1).reshape(-1, 3)
            p0, p1, p2 = pos[tris[0]]
            if np.cross(p1 - p0, p2 - p0)[1] < 0:  # face para cima (+Y no glTF)
                tris = tris[:, ::-1]
            glb.mesh(f"agua_{tx:04d}_{ty:04d}", pos.astype(np.float32), nrm, uv, colors,
                     np.ascontiguousarray(tris.reshape(-1).astype(np.uint32)))
            quads_total += int(q.sum())
    glb.save(a.out)
    print(f"{a.out}: {len(glb.j['meshes'])} ladrilhos, {quads_total} quadrados de {st} m, água {W[W > 0].min():.1f}–{W.max():.1f} m")


if __name__ == "__main__":
    main()
