"""Converts meshes in batches on Windows/Linux, with a queue and memory protection."""
import argparse
from pathlib import Path, PurePosixPath
import subprocess
import sys

import nwplatform
import nwprocess
from nwassets import complete_glb


def mesh_paths(path):
    meshes = []
    names = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        mesh = line.strip()
        if not mesh:
            continue
        value = PurePosixPath(mesh)
        if value.is_absolute() or ".." in value.parts or ":" in mesh or "\\" in mesh or value.suffix.lower() != ".cgf":
            raise ValueError(f"invalid mesh path: {mesh}")
        short = value.stem.casefold()
        if short in names and names[short] != mesh:
            raise ValueError(f"meshes with the same output name: {names[short]} and {mesh}")
        names[short] = mesh
        if mesh not in meshes:
            meshes.append(mesh)
    return meshes


def convert(list_path, output, game, dry_run=False):
    meshes = mesh_paths(list_path)
    raw, clean = output / "glb", output / "limpa"
    todo = [mesh for mesh in meshes if not complete_glb(raw / (mesh + ".glb"))]
    print(f"meshes: {len(meshes)}, to convert: {len(todo)}", flush=True)
    if not dry_run:
        raw.mkdir(parents=True, exist_ok=True)
        clean.mkdir(parents=True, exist_ok=True)
        # nwbt --skip checks existence; move truncated files out of the final path.
        for mesh in todo:
            incomplete = raw / (mesh + ".glb")
            if incomplete.is_file():
                incomplete.replace(incomplete.with_name(incomplete.name + ".incomplete"))
    for offset in range(0, len(todo), 8):
        if not dry_run:
            nwprocess.require_resources([output, game], memory=16000 * 1024 ** 2, disk=30 * nwprocess.GIB)
        command = [nwplatform.executable("nwbt"), "models", "files", "-g", game,
                   "-o", raw, "--binary", "--embed", "--ior", "1.5", "-w", "2",
                   "--texture-size", "2048", "--skip", *todo[offset:offset + 8]]
        nwprocess.run(command, log=output / "nwbt.log", min_memory=10000 * 1024 ** 2, dry_run=dry_run)
    missing = []
    for mesh in meshes:
        src = raw / (mesh + ".glb")
        dst = clean / (PurePosixPath(mesh).stem + ".glb")
        if dry_run:
            nwprocess.run([sys.executable, "tools/glb_strip_nodraw.py", src, dst, "--sem-atributos"], dry_run=True)
            continue
        if not complete_glb(src):
            missing.append(mesh)
            continue
        if complete_glb(dst) and dst.stat().st_mtime_ns >= src.stat().st_mtime_ns:
            continue
        temporary = dst.with_suffix(".part.glb")
        nwprocess.run([sys.executable, "tools/glb_strip_nodraw.py", src, temporary, "--sem-atributos"],
                      log=output / "strip.log")
        if not complete_glb(temporary):
            raise RuntimeError(f"strip did not produce {temporary}")
        temporary.replace(dst)
    if missing:
        raise RuntimeError(f"nwbt did not produce {len(missing)} meshes: {missing}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("list", nargs="?", type=Path)
    parser.add_argument("output", nargs="?", type=Path, default=Path("extract/ue/gatherables"))
    parser.add_argument("--game", default=nwplatform.game_directory())
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    output = (nwplatform.ROOT / args.output).resolve()
    list_path = (nwplatform.ROOT / args.list).resolve() if args.list else output / "meshes.txt"
    try:
        if args.dry_run:
            convert(list_path, output, args.game, True)
        else:
            with nwprocess.heavy_lock():
                convert(list_path, output, args.game)
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
