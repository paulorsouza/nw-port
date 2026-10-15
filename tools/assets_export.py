#!/usr/bin/env python3
"""Exports what Unreal imported as zips in a local folder, to be uploaded by hand to the OneDrive folder.

usage: assets_export.py [--dir <folder>] [--allow-dirty] [--force] [--jobs 4]

The folder (default ~/nw-port-assets, or NW_ASSETS) mirrors the OneDrive one: manifest.json + bundles/*.zip.
Included: unreal/NWPort/Content (minus what is in git), unreal/NWPort/Data and the list of ue_*.ok in extract/.etapas.
One zip per folder of up to 1 GiB. Only what changed since the folder's manifest.json is zipped; unchanged zips
stay valid on OneDrive and do not need to be here. At the end, it says what to upload and what to delete on OneDrive.
Before exporting, download the newest manifest.json from OneDrive into the folder. Walkthrough: docs/tech/asset-sync.md.
"""
import argparse
import os
import sys
import uuid
import zipfile
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from assets_common import (DEFAULT_DIR, UPROJ, Hasher, check_names, die, etapas_markers, fingerprint, game_build,  # noqa: E402
                           git, group, machine, now, plan_bundles, preflight, read_json, read_state, scan,
                           sha256_file, ue_version, write_json, write_state, zip_name)


def make_zip(target, rels):
    tmp = Path(str(target) + ".tmp")
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as z:
        for rel in sorted(rels):
            z.write(UPROJ / rel, rel)
    os.replace(tmp, target)
    return target.name, sha256_file(target), target.stat().st_size


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, default=DEFAULT_DIR)
    ap.add_argument("--allow-dirty", action="store_true", help="export with uncommitted code in Source/ or tools/")
    ap.add_argument("--force", action="store_true", help="export over another machine's publication")
    ap.add_argument("--jobs", type=int, default=4)
    a = ap.parse_args()
    out_dir = a.dir.expanduser()
    bdir = out_dir / "bundles"

    preflight()
    head = git("rev-parse", "HEAD").stdout.strip()
    if not git("branch", "-r", "--contains", head).stdout.strip():
        die("The current commit is not on GitHub: push the code first (the assets depend on it).")
    dirty = git("status", "--porcelain", "--", "unreal/NWPort/Source", "unreal/NWPort/Config",
                "unreal/NWPort/NWPort.uproject", "tools").stdout.strip()
    if dirty and not a.allow_dirty:
        print(dirty, file=sys.stderr)
        die("Uncommitted change in code or tools/: commit and push first (or --allow-dirty).")
    ue = ue_version()

    old = read_json(out_dir / "manifest.json", None)
    gen = old["generation"] if old else 0
    if old and not a.force:
        if old.get("id") != read_state().get("id"):
            die(f"The folder's manifest.json is from generation {gen} ({old['machine']}, {old['created_at']}) and this "
                "machine has not imported that publication: run assets_import.py first (or --force, which discards that "
                "publication).")
        if old["ue_version"] != ue:
            die(f"The current publication is from Unreal {old['ue_version']}, this machine uses {ue}.")

    print("Reading the files...")
    files = scan()
    if not files:
        die("Content and Data are empty: nothing to export.")
    check_names(files)
    if old and len(files) < sum(b["files"] for b in old["bundles"].values()) / 2 and not a.force:
        die(f"{len(files)} files here, the current publication has many more. Run assets_import.py first "
            "(or --force to export anyway).")
    hasher = Hasher()
    bundles = plan_bundles(files)
    groups, _ = group(files, bundles)
    prev = old["bundles"] if old else {}
    entries, todo = {}, []
    for path, deep in bundles.items():
        rels = groups[path]
        fp = fingerprint(rels, files, hasher)
        name = zip_name(path, deep, fp)
        p = prev.get(path)
        if p and p["zip"] == name:
            entries[path] = p  # same as what is already on OneDrive
            continue
        entries[path] = {"deep": deep, "fingerprint": fp, "zip": name, "files": len(rels),
                         "bytes": sum(files[r][0] for r in rels)}
        todo.append((path, name, rels))
    hasher.save(files)
    etapas = etapas_markers()
    if etapas is None and old:
        etapas = old.get("etapas")
    if old and not todo and set(entries) == set(prev) and etapas == old.get("etapas"):
        print(f"Nothing changed since generation {gen}.")
        return

    print(f"{len(entries)} zips; {len(todo)} new "
          f"({sum(entries[p]['bytes'] for p, _, _ in todo) / 2**30:.1f} GiB of source).")
    bdir.mkdir(parents=True, exist_ok=True)
    with ProcessPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(make_zip, bdir / name, rels): path for path, name, rels in todo}
        for i, f in enumerate(futs, 1):
            name, sha, nbytes = f.result()
            entries[futs[f]].update(sha256=sha, zip_bytes=nbytes)
            print(f"  [{i}/{len(todo)}] {name} ({nbytes / 2**20:.0f} MiB)")

    manifest = {"generation": gen + 1, "id": uuid.uuid4().hex, "parent": old.get("id") if old else None,
                "machine": machine(), "created_at": now(), "git_commit": head, "ue_version": ue,
                "game_build": game_build(), "etapas": etapas, "bundles": entries}
    write_json(out_dir / "manifest.json", manifest)
    keep = {e["zip"] for e in entries.values()}
    for z in bdir.iterdir():
        if z.name not in keep:
            z.unlink()
    removed = sorted({e["zip"] for e in prev.values()} - keep)
    write_state(manifest)

    new_bytes = sum(entries[p]["zip_bytes"] for p, _, _ in todo)
    print(f"\nExported generation {gen + 1} to {out_dir}.")
    print(f"Upload to the OneDrive folder ({new_bytes / 2**30:.2f} GiB): the zips first, manifest.json last.")
    for _, name, _ in todo:
        print(f"  bundles/{name}")
    print("  manifest.json")
    if removed:
        print("Afterwards, you can delete from the OneDrive bundles/ folder:")
        for z in removed:
            print(f"  bundles/{z}")


if __name__ == "__main__":
    main()
