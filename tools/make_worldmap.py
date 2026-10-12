"""Mapa do mundo (M) para Guardaventos: referencial e lista de tiles do jogo.

    python3 tools/make_worldmap.py

Os tiles ficam em lyshineui/worldtiles/newworld_vitaeeterna/map_l<z>_y<yyy>_x<xxx>.dds, 1024 px cada. No nível 1 cada
tile cobre 256 m (4 px/m); no nível z cobre 256·2^(z-1) m e os índices andam de 2^(z-1) em 2^(z-1). O índice y cresce
para o norte (y do mundo). O port usa os níveis 2 (512 m) e 3 (1024 m) em volta da região, com uma margem.

Grava:
  unreal/NWPort/Data/World/worldmap.json  referencial da cidade (mundo = origem + giro · local; UE = (-lx, ly) · 100),
                                          limites da região e os tiles por nível
  extract/ui/worldmap_textures.txt        texturas para tools/ue/import_ui.py (tiles e peças da tela magicmap)
Depois: nwbt unpack dos tiles em PNG (docs/ASSETS.md) e tools/ue_import_all.sh 09.
"""
import glob
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGION_DIR = glob.glob(os.path.join(ROOT, "extract", "terrain", "**", "r_+04_+01", "capitals"), recursive=True)[0]
CITY = "Settlement_SouthWindsward_Complete"
REGION = (4, 1)
REGION_SIZE = 2048
TILE_L1 = 256
MARGIN = 512   # metros além da borda da região
LEVEL = "newworld_vitaeeterna"
SCREEN = ["map/icon/player.dds", "map/map_gradient.dds", "map/map_gradientside.dds", "map/scale_key.dds",
          "slices/button/buttonbg.dds", "slices/button/heroframedisabled2023.dds", "slices/button/focusnew.dds",
          "icons/misc/icon_close.dds"]


def main():
    caps = json.load(open(glob.glob(os.path.join(REGION_DIR, "06swindsward_persistence", "*.capitals.json"))[0]))["Capitals"]
    city = next(c for c in caps if c["sliceName"].endswith(CITY))
    T = city["worldPosition"]
    ang = 2 * math.atan2(city["rotation"]["z"], city["rotation"]["w"])
    x0, y0 = REGION[0] * REGION_SIZE, REGION[1] * REGION_SIZE
    levels, textures = [], []
    for z in (2, 3):
        step = 2 ** (z - 1)
        size = TILE_L1 * step
        tiles = []
        for ty in range((y0 - MARGIN) // size * step, (y0 + REGION_SIZE + MARGIN) // TILE_L1, step):
            for tx in range((x0 - MARGIN) // size * step, (x0 + REGION_SIZE + MARGIN) // TILE_L1, step):
                path = f"lyshineui/worldtiles/{LEVEL}/map_l{z}_y{ty:03d}_x{tx:03d}.dds"
                tiles.append({"x": tx * TILE_L1, "y": ty * TILE_L1, "tex": path})
                textures.append(path)
        levels.append({"level": z, "size": size, "tiles": tiles})
    out = os.path.join(ROOT, "unreal", "NWPort", "Data", "World")
    os.makedirs(out, exist_ok=True)
    json.dump({"origin": [T["x"], T["y"]], "angle": ang, "region": [x0, y0, x0 + REGION_SIZE, y0 + REGION_SIZE],
               "levels": levels}, open(os.path.join(out, "worldmap.json"), "w"), indent=1)
    textures += ["lyshineui/images/" + t for t in SCREEN]
    open(os.path.join(ROOT, "extract", "ui", "worldmap_textures.txt"), "w").write("\n".join(textures) + "\n")
    print(f"origem {T['x']:.1f},{T['y']:.1f} giro {math.degrees(ang):.1f}°; "
          + ", ".join(f"nível {l['level']}: {len(l['tiles'])} tiles" for l in levels))


if __name__ == "__main__":
    main()
