#!/usr/bin/env python3
"""Updates this machine's Content and Data with the zips exported by tools/assets_export.py.

usage: assets_import.py [--dir <folder>] [--check] [--yes] [--force]

The folder (default ~/nw-port-assets, or NW_ASSETS) gets by hand what is on OneDrive: first only manifest.json;
the import says which zips to download (only those of folders that differ from here). A zip can be in bundles/ or loose.
A local file that is replaced or not in the publication goes to unreal/NWPort/Saved/assets_backup/<date>/ (the last
3 are kept). Files versioned in git are not touched. Walkthrough: docs/tech/asset-sync.md.
"""
import argparse
import json
import os
import shutil
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from assets_common import (BACKUP, DEFAULT_DIR, MARKERS, UPROJ, Hasher, ask, bundle_of, die, fingerprint,  # noqa: E402
                           game_build, git, preflight, prune_backups, read_state, remove_empty_dirs, scan,
                           sha256_file, stamp, ue_version, write_state)


def find_zip(folder, name):
    for p in (folder / "bundles" / name, folder / name):
        if p.exists():
            return p
    return None


def move_to_backup(rel, bdir):
    src = UPROJ / rel
    if not src.exists():
        return
    dst = bdir / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    os.replace(src, dst)


def safe_member(name, path, deep):
    parts = name.split("/")
    if name.startswith("/") or ".." in parts or "\\" in name or ":" in parts[0]:
        return False
    return name.startswith(path + "/") if deep else name.rsplit("/", 1)[0] == path


def check_zip(zpath, path, deep):
    with zipfile.ZipFile(zpath) as z:
        bad = [n for n in z.namelist() if not n.endswith("/") and not safe_member(n, path, deep)]
    if bad:
        die(f"{zpath.name}: entry outside the zip's folder ({bad[0]}). Nothing was changed.")


def extract(zpath):
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            out = UPROJ / info.filename
            out.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as fi, open(out, "wb") as fo:
                shutil.copyfileobj(fi, fo, 1 << 20)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, default=DEFAULT_DIR)
    ap.add_argument("--check", action="store_true", help="only show what would change and which zips to download")
    ap.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    ap.add_argument("--force", action="store_true", help="ignore the Unreal version, game build and commit")
    a = ap.parse_args()
    folder = a.dir.expanduser()
    mf = folder / "manifest.json"
    if not mf.exists():
        die(f"No {mf}: download manifest.json from the OneDrive folder to it.")
    m = json.loads(mf.read_text(encoding="utf-8"))
    if not a.check:
        preflight()
    if not a.force:
        ue = ue_version()
        if m["ue_version"] != ue:
            die(f"The zips are from Unreal {m['ue_version']}, this machine uses {ue}.")
        if m["game_build"] != game_build():
            die(f"The zips are from game build {m['game_build']}; tools/game.version here says {game_build()}.")
        c = m["git_commit"]
        if git("cat-file", "-e", f"{c}^{{commit}}", check=False).returncode != 0:
            die(f"Commit {c[:9]} does not exist here: git pull first.")
        if git("merge-base", "--is-ancestor", c, "HEAD", check=False).returncode != 0:
            die(f"The zips are from commit {c[:9]}, which is not in HEAD here: git pull first.")

    st = read_state()
    print(f"Publication: generation {m['generation']} from {m['machine']} at {m['created_at']}; here: {st['generation']}.")
    print("Comparing with the files here...")
    files = scan()
    hasher = Hasher()
    bundles = {p: e["deep"] for p, e in m["bundles"].items()}
    local = {p: [] for p in bundles}
    orphans = []
    for rel in files:
        b = bundle_of(rel, bundles)
        (local[b] if b is not None else orphans).append(rel)
    changed = [p for p, e in m["bundles"].items() if fingerprint(local[p], files, hasher) != e["fingerprint"]]
    hasher.save(files)

    for p in changed:
        e = m["bundles"][p]
        print(f"  {p}{'' if e['deep'] else ' (loose)'}: {len(local[p])} → {e['files']} files")
    if orphans:
        print(f"  {len(orphans)} file(s) here not in the publication (they go to the backup), e.g.: {orphans[0]}")
    want = set(m["etapas"]) if m.get("etapas") is not None else None  # manifest key kept: older publications use it
    have = {p.name for p in MARKERS.glob("ue_*.ok")} if MARKERS.exists() else set()
    markers_differ = want is not None and want != have
    if markers_differ:
        print(f"  import markers: {len(want - have)} new, {len(have - want)} to remove")
    if not changed and not orphans and not markers_differ:
        print("Already matches the publication.")
        if not a.check:
            write_state(m)
        return

    missing = [m["bundles"][p]["zip"] for p in changed if not find_zip(folder, m["bundles"][p]["zip"])]
    total = sum(m["bundles"][p]["zip_bytes"] for p in changed)
    print(f"{len(changed)} zip(s), {total / 2**30:.2f} GiB.")
    if missing:
        size = sum(m["bundles"][p]["zip_bytes"] for p in changed if m["bundles"][p]["zip"] in missing)
        print(f"\nDownload from the OneDrive bundles/ folder to {folder / 'bundles'} ({size / 2**30:.2f} GiB):")
        print("\n".join(f"  {z}" for z in missing))
        print("Then run again. Nothing was changed.")
        sys.exit(2)
    if a.check:
        return

    print("Checking the zips...")
    for p in changed:
        e = m["bundles"][p]
        z = find_zip(folder, e["zip"])
        if z.stat().st_size != e["zip_bytes"] or sha256_file(z) != e["sha256"]:
            die(f"{z.name} differs from the manifest (incomplete download?). Download it again. Nothing was changed.")
        check_zip(z, p, e["deep"])
    ask(f"Apply ({len(changed)} folder(s), {len(orphans)} file(s) to the backup)?", a.yes)

    bdir = BACKUP / stamp()
    for i, p in enumerate(changed, 1):
        e = m["bundles"][p]
        print(f"  [{i}/{len(changed)}] {e['zip']} ({e['zip_bytes'] / 2**20:.0f} MiB)")
        for rel in local[p]:
            move_to_backup(rel, bdir)
        extract(find_zip(folder, e["zip"]))
    for rel in orphans:
        move_to_backup(rel, bdir)
    remove_empty_dirs(orphans + [r for p in changed for r in local[p]])
    if markers_differ:
        MARKERS.mkdir(parents=True, exist_ok=True)
        for n in have - want:
            (MARKERS / n).unlink()
        for n in want - have:
            (MARKERS / n).touch()
    write_state(m)
    if bdir.exists():
        print(f"What was replaced or removed here is in {bdir}.")
    prune_backups()
    print(f"This machine is at generation {m['generation']}. On first open, Unreal compiles the shaders for what changed.")


if __name__ == "__main__":
    main()
