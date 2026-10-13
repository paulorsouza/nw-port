"""Confere a build do time no Windows/Linux, sem Bash ou sha256sum."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import sys

import nwplatform


def expected_version(path):
    values = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            key, value = line.split()
            if key in values:
                raise ValueError(f"chave repetida em {path}: {key}")
            values[key] = value
    if not re.fullmatch(r"\d+", values.get("buildid", "")):
        raise ValueError(f"buildid invalido em {path}")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", values.get("exe_sha256", "")):
        raise ValueError(f"exe_sha256 invalido em {path}")
    return values["buildid"], values["exe_sha256"].lower()


def check_version(game, version_file, check_exe=False):
    """Retorna divergencias; ausencia do manifesto exige hash do executavel."""
    want_build, want_sha = expected_version(version_file)
    errors = []
    manifest = game.parent.parent / "appmanifest_1063730.acf"
    if manifest.is_file():
        text = manifest.read_text(encoding="utf-8-sig")
        builds = re.findall(r'"buildid"\s*"([^"\r\n]*)"', text)
        if builds != [want_build]:
            errors.append(f"build {builds or 'ausente'} instalada, time usa {want_build}")
    else:
        print(f"versao do jogo: sem {manifest}; conferindo so pelo executavel", file=sys.stderr)
        check_exe = True
    if check_exe:
        executable = game / "Bin64/NewWorld.exe"
        try:
            with executable.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
        except OSError as exc:
            errors.append(f"nao foi possivel ler {executable}: {exc}")
        else:
            if actual != want_sha:
                errors.append(f"NewWorld.exe com sha256 {actual}, time usa {want_sha}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", action="store_true", help="Confere tambem o SHA256 do executavel")
    parser.add_argument("--game", type=Path, default=Path(nwplatform.game_directory()))
    args = parser.parse_args()
    try:
        errors = check_version(args.game.resolve(), nwplatform.ROOT / "tools/jogo.versao", args.exe)
    except (OSError, ValueError) as exc:
        print(f"versao do jogo: {exc}", file=sys.stderr)
        return 4
    for error in errors:
        print(f"versao do jogo: {error}", file=sys.stderr)
    if errors and not os.environ.get("NW_IGNORA_VERSAO"):
        print("Use a build do time (tools/jogo.versao) ou combine a atualizacao com todos.", file=sys.stderr)
        return 4
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
