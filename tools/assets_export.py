#!/usr/bin/env python3
"""Exporta o que a Unreal importou em zips numa pasta local, para subir à mão na pasta do OneDrive.

uso: assets_export.py [--dir <pasta>] [--allow-dirty] [--force] [--jobs 4]

A pasta (padrão ~/nw-port-assets, ou NW_ASSETS) espelha a do OneDrive: manifest.json + bundles/*.zip.
Vai: unreal/NWPort/Content (menos o que está no git), unreal/NWPort/Data e a lista dos ue_*.ok de extract/.etapas.
Um zip por pasta de até 1 GiB. Só é zipado o que mudou desde o manifest.json da pasta; os zips que não mudaram
continuam valendo no OneDrive e não precisam estar aqui. No fim, diz o que subir e o que apagar no OneDrive.
Antes de exportar, baixe do OneDrive o manifest.json mais novo para a pasta. Roteiro: docs/sincronizar-assets.md.
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
    ap.add_argument("--allow-dirty", action="store_true", help="exporta com código sem commit em Source/ ou tools/")
    ap.add_argument("--force", action="store_true", help="exporta por cima da publicação de outra máquina")
    ap.add_argument("--jobs", type=int, default=4)
    a = ap.parse_args()
    out_dir = a.dir.expanduser()
    bdir = out_dir / "bundles"

    preflight()
    head = git("rev-parse", "HEAD").stdout.strip()
    if not git("branch", "-r", "--contains", head).stdout.strip():
        die("O commit atual não está no GitHub: push do código antes (os assets dependem dele).")
    dirty = git("status", "--porcelain", "--", "unreal/NWPort/Source", "unreal/NWPort/Config",
                "unreal/NWPort/NWPort.uproject", "tools").stdout.strip()
    if dirty and not a.allow_dirty:
        print(dirty, file=sys.stderr)
        die("Mudança não commitada em código ou tools/: commit e push antes (ou --allow-dirty).")
    ue = ue_version()

    old = read_json(out_dir / "manifest.json", None)
    gen = old["generation"] if old else 0
    if old and not a.force:
        if old.get("id") != read_state().get("id"):
            die(f"O manifest.json da pasta é da geração {gen} ({old['machine']}, {old['created_at']}) e esta máquina "
                "não importou essa publicação: rode o assets_import.py antes (ou --force, que descarta a publicação "
                "dela).")
        if old["ue_version"] != ue:
            die(f"A publicação atual é da Unreal {old['ue_version']}, esta máquina usa {ue}.")

    print("Lendo os arquivos...")
    files = scan()
    if not files:
        die("Content e Data vazios: nada a exportar.")
    check_names(files)
    if old and len(files) < sum(b["files"] for b in old["bundles"].values()) / 2 and not a.force:
        die(f"{len(files)} arquivos aqui, a publicação atual tem bem mais. Rode o assets_import.py antes "
            "(ou --force para exportar assim).")
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
            entries[path] = p  # igual ao que já está no OneDrive
            continue
        entries[path] = {"deep": deep, "fingerprint": fp, "zip": name, "files": len(rels),
                         "bytes": sum(files[r][0] for r in rels)}
        todo.append((path, name, rels))
    hasher.save(files)
    etapas = etapas_markers()
    if etapas is None and old:
        etapas = old.get("etapas")
    if old and not todo and set(entries) == set(prev) and etapas == old.get("etapas"):
        print(f"Nada mudou desde a geração {gen}.")
        return

    print(f"{len(entries)} zips; {len(todo)} novos "
          f"({sum(entries[p]['bytes'] for p, _, _ in todo) / 2**30:.1f} GiB de origem).")
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
    print(f"\nExportada a geração {gen + 1} em {out_dir}.")
    print(f"Suba para a pasta do OneDrive ({new_bytes / 2**30:.2f} GiB): os zips primeiro, o manifest.json por último.")
    for _, name, _ in todo:
        print(f"  bundles/{name}")
    print("  manifest.json")
    if removed:
        print("Depois, pode apagar da pasta bundles/ do OneDrive:")
        for z in removed:
            print(f"  bundles/{z}")


if __name__ == "__main__":
    main()
