"""Regenerates Data/ with native Python; --dry-run shows the steps without writing."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys

import nwplatform


def steps(root):
    yield ["tools/make_gatherables.py"]
    yield ["tools/make_spawns.py"]
    yield ["tools/make_rules.py"]
    if (root / "extract/serverpak/json/sharedassets/ai").is_dir():
        yield ["tools/make_ai.py"]
    else:
        print("== no extract/serverpak: Data/AI stays as is", flush=True)
    slice_path = (root / "extract/slicejson/slices/structures/settlements/v2_settlements/"
                  "settlement_northwindsward_complete.dynamicslice.json")
    if slice_path.is_file():
        yield ["tools/make_crafting.py"]
    else:
        print("== no Everfall slice: Data/Crafting stays as is", flush=True)
    if (root / "extract/audio/sfx_index.csv").is_file():
        yield ["tools/make_ui_sounds.py"]
    else:
        print("== no extract/audio: Data/Audio stays as is", flush=True)
    for name in (
        "make_items", "make_port_items", "make_appearances", "make_abilities", "make_progression", "make_worldmap",
        "build_strings",
        "ui_build_hud", "ui_build_mastery", "ui_build_tradeskills", "ui_build_attributes",
        "ui_build_login", "ui_build_respawn", "ui_build_group", "ui_build_inventory",
    ):
        yield [f"tools/{name}.py"]
    for screen in sorted((root / "tools/ui_screens").glob("*.json")):
        yield ["tools/ui_build_screen.py", str(screen.relative_to(root))]
    yield ["tools/ui_textures_extra.py"]


def run_steps(commands, root, dry_run=False):
    for arguments in commands:
        command = [sys.executable, *arguments]
        display = subprocess.list2cmdline(command) if nwplatform.WINDOWS else shlex.join(command)
        print("==", display, flush=True)
        if not dry_run:
            subprocess.run(command, cwd=root, env=nwplatform.environment(), check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if not args.dry_run:
        # Data/ is ignored by Git; in a fresh checkout the UI scripts write to Data/UI without creating it.
        (nwplatform.ROOT / "unreal/NWPort/Data/UI").mkdir(parents=True, exist_ok=True)
    try:
        run_steps(steps(nwplatform.ROOT), nwplatform.ROOT, args.dry_run)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"Data/ interrupted: {exc}", file=sys.stderr)
        return 1
    if not args.dry_run:
        count = sum(p.is_file() for p in (nwplatform.ROOT / "unreal/NWPort/Data").rglob("*"))
        print(f"Data/ ready: {count} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
