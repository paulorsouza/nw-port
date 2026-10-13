"""Regenera Data/ com Python nativo; --dry-run mostra as etapas sem escrever."""
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
        print("== sem extract/serverpak: Data/AI fica como esta", flush=True)
    slice_path = (root / "extract/slicejson/slices/structures/settlements/v2_settlements/"
                  "settlement_northwindsward_complete.dynamicslice.json")
    if slice_path.is_file():
        yield ["tools/make_crafting.py"]
    else:
        print("== sem o slice de Queda Eterna: Data/Crafting fica como esta", flush=True)
    for name in (
        "make_items", "make_abilities", "make_progression", "make_worldmap", "build_strings",
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
        # Data/ e ignorado pelo Git; num checkout novo os scripts de UI gravam em Data/UI sem cria-la.
        (nwplatform.ROOT / "unreal/NWPort/Data/UI").mkdir(parents=True, exist_ok=True)
    try:
        run_steps(steps(nwplatform.ROOT), nwplatform.ROOT, args.dry_run)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"Data/ interrompido: {exc}", file=sys.stderr)
        return 1
    if not args.dry_run:
        count = sum(p.is_file() for p in (nwplatform.ROOT / "unreal/NWPort/Data").rglob("*"))
        print(f"Data/ pronto: {count} arquivos")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
