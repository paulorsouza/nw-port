"""World map (M) for Windsward: reference frame and list of game tiles.

    python3 tools/make_worldmap.py

The tiles are in lyshineui/worldtiles/newworld_vitaeeterna/map_l<z>_y<yyy>_x<xxx>.dds, 1024 px each. At level 1 each
tile covers 256 m (4 px/m); at level z it covers 256·2^(z-1) m and the indices step by 2^(z-1). The y index grows
to the north (world y). The port uses levels 2 (512 m) and 3 (1024 m) around the region, with a margin.

Writes:
  unreal/NWPort/Data/World/worldmap.json  city frame (world = origin + rotation · local; UE = (-lx, ly) · 100),
                                          region bounds and the tiles per level
  extract/ui/worldmap_textures.txt        textures for tools/ue/import_ui.py (tiles and pieces of the magicmap screen)
POIs: slices/pois/territories/r_+04_+01_pois.dynamicslice (extract/pois, nwbt --x-objects json) gives each
territory's position (TerritoryDataProviderComponent + transform); pointofinterestdefinitions gives map name and icon.
Then: nwbt unpack of the tiles to PNG (docs/tech/asset-pipeline.md) and tools/linux/ue_import_all.sh 09.
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
MARGIN = 512   # meters beyond the region edge
LEVEL = "newworld_vitaeeterna"
SCREEN = ["map/icon/player.dds", "map/map_gradient.dds", "map/map_gradientside.dds", "map/scale_key.dds",
          "slices/button/buttonbg.dds", "slices/button/heroframedisabled2023.dds", "slices/button/focusnew.dds",
          "icons/misc/icon_close.dds", "map/icon/groupmember.dds", "map/icon/lastdeathposition.dds",
          "map/icon/waypoint.dds", "map/icon/waypoint_compass.dds"]


def pois():
    src = os.path.join(ROOT, "extract", "pois", "slices", "pois", "territories", f"r_+{REGION[0]:02d}_+{REGION[1]:02d}_pois.dynamicslice.json")
    if not os.path.exists(src):
        print("no POIs (extract/pois): run tools/linux/build_all.sh 12")
        return []
    defs = {}
    for f in glob.glob(os.path.join(ROOT, "extract", "data", "**", "pointofinterestdefinitions", "*.json"), recursive=True):
        d = json.load(open(f))
        for r in d["rows"] if isinstance(d, dict) else d:
            defs[str(r["TerritoryID"])] = r
    ents = json.load(open(src))["components"]["element"]["entities"]["element"]
    out = []
    for e in ents:
        comps = e["components"]["element"]
        comps = comps if isinstance(comps, list) else [comps]
        tid = pos = None
        for c in comps:
            if c["__type"] == "TerritoryDataProviderComponent":
                tid = str(next(v for k, v in c.items() if k.lower().replace(" ", "_") == "territory_id"))
            elif c["__type"] == "TransformComponent":
                pos = c["transform"]["data"][9:11]
        d = defs.get(tid)
        if not d or not pos or not d.get("MapIcon") or str(d.get("IsPOI")).lower() != "true":
            continue
        icon = "lyshineui/images/" + d["MapIcon"].lower().split("lyshineui/images/")[1].rsplit(".", 1)[0] + ".dds"
        out.append({"x": round(pos[0], 1), "y": round(pos[1], 1), "name": d["NameLocalizationKey"].lstrip("@").lower(), "icon": icon})
    return out


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
    poi = pois()
    textures += sorted({p["icon"] for p in poi})
    out = os.path.join(ROOT, "unreal", "NWPort", "Data", "World")
    os.makedirs(out, exist_ok=True)
    json.dump({"origin": [T["x"], T["y"]], "angle": ang, "region": [x0, y0, x0 + REGION_SIZE, y0 + REGION_SIZE],
               "levels": levels, "pois": poi}, open(os.path.join(out, "worldmap.json"), "w"), indent=1)
    textures += ["lyshineui/images/" + t for t in SCREEN]
    open(os.path.join(ROOT, "extract", "ui", "worldmap_textures.txt"), "w").write("\n".join(textures) + "\n")
    print(f"origin {T['x']:.1f},{T['y']:.1f} rotation {math.degrees(ang):.1f}°; "
          + ", ".join(f"level {l['level']}: {len(l['tiles'])} tiles" for l in levels) + f"; {len(poi)} POIs")


if __name__ == "__main__":
    main()
