"""Converts a town and its terrain on Windows/Linux without Bash; accepts --dry-run."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

import nwplatform
import nwprocess
from nwassets import complete_glb

LEVEL = "newworld_vitaeeterna"


def nonempty(path):
    return path.is_file() and path.stat().st_size > 0


def find_capital(regions, name):
    matches = []
    for path in sorted(regions.glob("r_*/capitals/*/*.capitals.json")):
        for capital in json.loads(path.read_text(encoding="utf-8-sig")).get("Capitals", []):
            if capital.get("sliceName", "").lower().rsplit("/", 1)[-1] == name.lower():
                matches.append((path, capital))
    if len(matches) != 1:
        return None, [], len(matches)
    path, capital = matches[0]
    position = capital["worldPosition"]
    radius = 768 / 2 * math.sqrt(2) + 2
    xs = range(math.floor((position["x"] - radius) / 2048), math.floor((position["x"] + radius) / 2048) + 1)
    ys = range(math.floor((position["y"] - radius) / 2048), math.floor((position["y"] + radius) / 2048) + 1)
    return path, [(x, y) for x in xs for y in ys], 1


def region_name(x, y):
    # The original format uses a literal '+' before the integer (negative ones included).
    return f"r_+{x:02d}_+{y:02d}"


def convert(name, game, dry_run=False):
    if not re.fullmatch(r"[A-Za-z0-9_]+", name):
        raise ValueError("use the slice name, without directory or extension")
    root = nwplatform.ROOT
    cities = root / "extract/cities"
    terrain_data = root / "extract/terrain"
    source = f"slices/structures/settlements/v2_settlements/{name}.dynamicslice"
    raw = cities / str(Path(source).with_suffix(".glb"))
    clean = cities / "limpa" / (name + ".glb")
    short = name.removesuffix("_complete").removeprefix("settlement_").removesuffix("_settlement")
    terrain = cities / (short + "_terrain.glb")
    log = cities / "logs" / (name + ".log")

    def run(command):
        nwprocess.run(command, log=log, min_memory=8 * nwprocess.GIB, dry_run=dry_run)

    def nwbt(*args):
        run([nwplatform.executable("nwbt"), *args])

    if not dry_run:
        deadline = time.monotonic() + 20 * 60
        wanted = nwprocess.memory_floor(12 * nwprocess.GIB)
        while nwprocess.available_memory() < wanted:
            if time.monotonic() >= deadline:
                raise nwprocess.ResourceError(f"less than {wanted / nwprocess.GIB:g} GiB available for 20 minutes")
            print("low memory; waiting to convert the town", flush=True)
            time.sleep(2)
        nwprocess.require_resources([root, game], disk=30 * nwprocess.GIB)
        clean.parent.mkdir(parents=True, exist_ok=True)
    if not complete_glb(raw):
        temporary = cities / (".tmp_" + name)
        nwbt("models", "slices", "-g", game, "-o", temporary, "--binary", "--embed", "--ior", "1.5",
             *nwplatform.nwbt_workers(), source)
        if not dry_run:
            matches = [p for p in temporary.rglob(name + ".glb") if complete_glb(p)]
            if len(matches) != 1:
                raise RuntimeError(f"expected one town GLB, found {len(matches)} in {temporary}")
            raw.parent.mkdir(parents=True, exist_ok=True)
            matches[0].replace(raw)
            for animation in (temporary / "animations").rglob("*"):
                if animation.is_file():
                    destination = cities / "animations" / animation.relative_to(temporary / "animations")
                    if not destination.exists():
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(animation, destination)
    if not complete_glb(clean):
        part = clean.with_suffix(".part.glb")
        run([sys.executable, "tools/glb_strip_nodraw.py", raw, part])
        if not dry_run:
            if not complete_glb(part):
                raise RuntimeError(f"strip did not produce {part}")
            part.replace(clean)
    if complete_glb(terrain) and nonempty(terrain.with_suffix(".json")):
        return
    regions = terrain_data / f"sharedassets/coatlicue/{LEVEL}/regions"
    if not any(regions.glob("r_*/capitals/*/*.capitals.json")) or not any(regions.glob("r_*/region.heightmap")):
        nwbt("unpack", "-g", game, "-o", terrain_data, "-e",
             rf"sharedassets/coatlicue/{LEVEL}/regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.heightmap)$")
    if dry_run:
        print("== at run time: locate the capital, extract missing heightmaps/roads_splat and build the terrain", flush=True)
        return
    capital, coordinates, count = find_capital(regions, name)
    if capital is None:
        raise RuntimeError(f"expected one capital with slice {name}; found {count}. Terrain pending.")
    missing_height = [re.escape(region_name(x, y)) for x, y in coordinates
                      if not nonempty(regions / region_name(x, y) / "region.heightmap")]
    missing_splat = [f"{x}_{y}" for x, y in coordinates if not nonempty(
        terrain_data / f"materials/terrain/{LEVEL}/jav_{LEVEL}_{x}_{y}/jav_{LEVEL}_{x}.{y}.roads_splat.png")]
    if missing_height:
        nwbt("unpack", "-g", game, "-o", terrain_data, "-e",
             rf"sharedassets/coatlicue/{LEVEL}/regions/({'|'.join(missing_height)})/region\.heightmap$")
    if missing_splat:
        # Missing roads were tolerated in Bash: keep the terrain without that weight.
        try:
            nwbt("unpack", "-g", game, "-o", terrain_data, "--x-dds", "png", "-e",
                 rf"materials/terrain/{LEVEL}/jav_{LEVEL}_({'|'.join(missing_splat)})/jav_{LEVEL}_[0-9]+\.[0-9]+\.roads_splat\.dds$")
        except subprocess.CalledProcessError:
            print("warning: roads_splat not converted; terrain without road weight", flush=True)
    for x, y in coordinates:
        if not nonempty(regions / region_name(x, y) / "region.heightmap"):
            raise RuntimeError(f"missing heightmap: {region_name(x, y)}")
    part = terrain.with_suffix(".part.glb")
    run([sys.executable, "tools/make_terrain.py", terrain_data, capital, name, part])
    if not complete_glb(part) or not nonempty(part.with_suffix(".json")):
        raise RuntimeError("make_terrain did not produce a complete GLB and JSON")
    part.with_suffix(".json").replace(terrain.with_suffix(".json"))
    part.replace(terrain)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("slice")
    parser.add_argument("--game", default=nwplatform.game_directory())
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        if args.dry_run:
            convert(args.slice, args.game, True)
        else:
            with nwprocess.heavy_lock():
                convert(args.slice, args.game)
    except nwprocess.ResourceError as exc:
        print(exc, file=sys.stderr)
        return 3
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
