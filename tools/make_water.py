"""Water mesh (rivers, lakes, waterfalls) of a region, in the town map frame.

    python3 tools/make_water.py <region.f32> <capitals.json> <slice> <output.glb> [--step 2]

<region.f32> = water height per meter, from `dbgwater` (nwbt, region.waterqt → float32 LE, row 0 = max y,
like the heightmap); no water = value <= 0. Writes one mesh per 128 m tile with --step meter squares
where the water is above the terrain. Where the level changes from one square to its neighbor the surface slopes:
those are the waterfalls. Same frame and axes as tools/make_terrain.py (glTF = CryEngine (−x, z, y), town-local).
Vertex color R = 1 on the steps (waterfall), 0 on still water.
"""
import argparse
import math
import os
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_terrain import REGION, GlbWriter, find_capital, sample  # noqa: E402


def floating_patches(W, H, wet, shore):
    """Mask of wet cells whose connected same-level patch (|Δ| <= 0.5 m) has no shore: no cell with
    the terrain within `shore` meters below the surface. Touching a cliff (terrain above the water, the top of the
    waterfall) does not count as shore."""
    from collections import deque
    seen = np.zeros(W.shape, bool)
    out = np.zeros(W.shape, bool)
    ny, nx = W.shape
    for y0, x0 in np.argwhere(wet & (W - H > shore)):
        if seen[y0, x0]:
            continue
        seen[y0, x0] = True
        q, cells, has_shore = deque([(y0, x0)]), [], False
        while q:
            y, x = q.popleft()
            cells.append((y, x))
            has_shore |= 0.0 <= W[y, x] - H[y, x] <= shore
            for v, u in ((y, x + 1), (y, x - 1), (y + 1, x), (y - 1, x)):
                if 0 <= v < ny and 0 <= u < nx and wet[v, u] and not seen[v, u] and abs(W[v, u] - W[y, x]) <= 0.5:
                    seen[v, u] = True
                    q.append((v, u))
        if not has_shore:
            out[tuple(np.array(cells).T)] = True
    return out


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

    water = np.fromfile(a.water, dtype="<f4").reshape(REGION, REGION)  # row 0 = max y
    st = a.step
    # Samples at the grid vertices (st meters), world column x / row y.
    xs = np.arange(0, REGION + 1, st)
    ys = np.arange(0, REGION + 1, st)
    col = np.clip(xs, 0, REGION - 1)
    row = (REGION - 1) - np.clip(ys, 0, REGION - 1)
    W = water[row[:, None], col[None, :]]                                     # [y, x]
    wx, wy = np.meshgrid(rx * REGION + np.minimum(xs, REGION - 0.01), ry * REGION + np.minimum(ys, REGION - 0.01))
    H, _ = sample(terrain_dir, level, wx, wy, {})
    wet = W > 0
    # The waterqt stores one height per tree leaf: at a waterfall step, the upper level's leaf also covers
    # the stretch where the terrain has already dropped to the lower level, and the water looked like a slab in the air.
    # 1) A same-level patch with no shore at all (terrain more than 3 m below the water in every cell) is a
    #    piece of the waterfall curtain floating in the air: removed (the fall itself is a game effect/mesh, not waterqt).
    # 2) An upper-level cell next to lower water, with the terrain up to 2 m above the lower level, belongs to the
    #    lower patch: it drops to it (propagates until the terrain rises, where the waterfall is).
    W = W.copy()
    loose = floating_patches(W, H, wet, shore=3.0)
    wet &= ~loose
    W[loose] = 0.0
    moved = 0
    for _ in range(64):
        pad = np.pad(np.where(wet, W, np.inf), 1, mode="constant", constant_values=np.inf)
        low = np.min(np.stack([pad[1:-1, :-2], pad[1:-1, 2:], pad[:-2, 1:-1], pad[2:, 1:-1]]), axis=0)
        drop = wet & np.isfinite(low) & (low < W - 0.5) & (H < low + 2.0)
        if not drop.any():
            break
        W[drop] = low[drop]
        moved += int(drop.sum())
    print(f"water: {moved} vertices dropped to the lower level, {int(loose.sum())} floating ones removed")
    # A dry vertex next to a wet one inherits the neighbor's level (the edge goes slightly under the terrain).
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
            # A square is included if any corner is original water above the terrain.
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
            # Step = level difference with the neighbor > 0.5 m (waterfall).
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
            if np.cross(p1 - p0, p2 - p0)[1] < 0:  # facing up (+Y in glTF)
                tris = tris[:, ::-1]
            glb.mesh(f"agua_{tx:04d}_{ty:04d}", pos.astype(np.float32), nrm, uv, colors,
                     np.ascontiguousarray(tris.reshape(-1).astype(np.uint32)))
            quads_total += int(q.sum())
    glb.save(a.out)
    print(f"{a.out}: {len(glb.j['meshes'])} tiles, {quads_total} squares of {st} m, water {W[W > 0].min():.1f}–{W.max():.1f} m")


if __name__ == "__main__":
    main()
