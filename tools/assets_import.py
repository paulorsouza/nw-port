#!/usr/bin/env python3
"""Atualiza o Content e o Data desta máquina com os zips exportados pelo tools/assets_export.py.

uso: assets_import.py [--dir <pasta>] [--check] [--yes] [--force]

A pasta (padrão ~/nw-port-assets, ou NW_ASSETS) recebe à mão o que está no OneDrive: primeiro só o manifest.json;
o import diz quais zips baixar (só os das pastas que estão diferentes daqui). Zip pode ficar em bundles/ ou solto.
Arquivo local substituído ou que não está na publicação vai para unreal/NWPort/Saved/assets_backup/<data>/ (ficam os
3 últimos). Arquivos versionados no git não são tocados. Roteiro: docs/tech/asset-sync.md.
"""
import argparse
import json
import os
import shutil
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from assets_common import (BACKUP, DEFAULT_DIR, ETAPAS, UPROJ, Hasher, ask, bundle_of, die, fingerprint,  # noqa: E402
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
        die(f"{zpath.name}: entrada fora da pasta do zip ({bad[0]}). Nada foi alterado.")


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
    ap.add_argument("--check", action="store_true", help="só mostra o que mudaria e quais zips baixar")
    ap.add_argument("--yes", action="store_true", help="sem a pergunta de confirmação")
    ap.add_argument("--force", action="store_true", help="ignora versão da Unreal, build do jogo e commit")
    a = ap.parse_args()
    folder = a.dir.expanduser()
    mf = folder / "manifest.json"
    if not mf.exists():
        die(f"Sem {mf}: baixe o manifest.json da pasta do OneDrive para lá.")
    m = json.loads(mf.read_text(encoding="utf-8"))
    if not a.check:
        preflight()
    if not a.force:
        ue = ue_version()
        if m["ue_version"] != ue:
            die(f"Os zips são da Unreal {m['ue_version']}, esta máquina usa {ue}.")
        if m["game_build"] != game_build():
            die(f"Os zips são da build {m['game_build']} do jogo; tools/jogo.versao aqui diz {game_build()}.")
        c = m["git_commit"]
        if git("cat-file", "-e", f"{c}^{{commit}}", check=False).returncode != 0:
            die(f"Commit {c[:9]} não existe aqui: git pull antes.")
        if git("merge-base", "--is-ancestor", c, "HEAD", check=False).returncode != 0:
            die(f"Os zips são do commit {c[:9]}, que não está no HEAD daqui: git pull antes.")

    st = read_state()
    print(f"Publicação: geração {m['generation']} de {m['machine']} em {m['created_at']}; aqui: {st['generation']}.")
    print("Comparando com os arquivos daqui...")
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
        print(f"  {p}{'' if e['deep'] else ' (soltos)'}: {len(local[p])} → {e['files']} arquivos")
    if orphans:
        print(f"  {len(orphans)} arquivo(s) daqui fora da publicação (vão para o backup), ex.: {orphans[0]}")
    want = set(m["etapas"]) if m.get("etapas") is not None else None
    have = {p.name for p in ETAPAS.glob("ue_*.ok")} if ETAPAS.exists() else set()
    etapas_diff = want is not None and want != have
    if etapas_diff:
        print(f"  marcadores de import: {len(want - have)} novos, {len(have - want)} a remover")
    if not changed and not orphans and not etapas_diff:
        print("Já está igual à publicação.")
        if not a.check:
            write_state(m)
        return

    missing = [m["bundles"][p]["zip"] for p in changed if not find_zip(folder, m["bundles"][p]["zip"])]
    total = sum(m["bundles"][p]["zip_bytes"] for p in changed)
    print(f"{len(changed)} zip(s), {total / 2**30:.2f} GiB.")
    if missing:
        size = sum(m["bundles"][p]["zip_bytes"] for p in changed if m["bundles"][p]["zip"] in missing)
        print(f"\nBaixe da pasta bundles/ do OneDrive para {folder / 'bundles'} ({size / 2**30:.2f} GiB):")
        print("\n".join(f"  {z}" for z in missing))
        print("Depois rode de novo. Nada foi alterado.")
        sys.exit(2)
    if a.check:
        return

    print("Conferindo os zips...")
    for p in changed:
        e = m["bundles"][p]
        z = find_zip(folder, e["zip"])
        if z.stat().st_size != e["zip_bytes"] or sha256_file(z) != e["sha256"]:
            die(f"{z.name} está diferente do manifest (download incompleto?). Baixe de novo. Nada foi alterado.")
        check_zip(z, p, e["deep"])
    ask(f"Aplicar ({len(changed)} pasta(s), {len(orphans)} arquivo(s) para o backup)?", a.yes)

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
    if etapas_diff:
        ETAPAS.mkdir(parents=True, exist_ok=True)
        for n in have - want:
            (ETAPAS / n).unlink()
        for n in want - have:
            (ETAPAS / n).touch()
    write_state(m)
    if bdir.exists():
        print(f"O que foi substituído ou removido aqui está em {bdir}.")
    prune_backups()
    print(f"Esta máquina está na geração {m['generation']}. Na primeira abertura a Unreal compila os shaders do que mudou.")


if __name__ == "__main__":
    main()
