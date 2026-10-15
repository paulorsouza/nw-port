"""Checks the team build on Windows/Linux, without Bash or sha256sum."""
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
                raise ValueError(f"repeated key in {path}: {key}")
            values[key] = value
    if not re.fullmatch(r"\d+", values.get("buildid", "")):
        raise ValueError(f"invalid buildid in {path}")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", values.get("exe_sha256", "")):
        raise ValueError(f"invalid exe_sha256 in {path}")
    return values["buildid"], values["exe_sha256"].lower()


def check_version(game, version_file, check_exe=False):
    """Returns mismatches; a missing manifest requires the executable hash."""
    want_build, want_sha = expected_version(version_file)
    errors = []
    manifest = game.parent.parent / "appmanifest_1063730.acf"
    if manifest.is_file():
        text = manifest.read_text(encoding="utf-8-sig")
        builds = re.findall(r'"buildid"\s*"([^"\r\n]*)"', text)
        if builds != [want_build]:
            errors.append(f"build {builds or 'missing'} installed, team uses {want_build}")
    else:
        print(f"game version: no {manifest}; checking by the executable only", file=sys.stderr)
        check_exe = True
    if check_exe:
        executable = game / "Bin64/NewWorld.exe"
        try:
            with executable.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
        except OSError as exc:
            errors.append(f"could not read {executable}: {exc}")
        else:
            if actual != want_sha:
                errors.append(f"NewWorld.exe with sha256 {actual}, team uses {want_sha}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", action="store_true", help="Also checks the executable SHA256")
    parser.add_argument("--game", type=Path, default=Path(nwplatform.game_directory()))
    args = parser.parse_args()
    try:
        errors = check_version(args.game.resolve(), nwplatform.ROOT / "tools/game.version", args.exe)
    except (OSError, ValueError) as exc:
        print(f"game version: {exc}", file=sys.stderr)
        return 4
    for error in errors:
        print(f"game version: {error}", file=sys.stderr)
    # NW_IGNORA_VERSAO: old name, still accepted.
    if errors and not (os.environ.get("NW_IGNORE_VERSION") or os.environ.get("NW_IGNORA_VERSAO")):
        print("Use the team build (tools/game.version) or agree on the update with everyone.", file=sys.stderr)
        return 4
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
