"""Diagnostico inicial, sem extrair assets nem iniciar a Unreal."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nwplatform


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", help="Diretorio da instalacao do New World")
    parser.add_argument("--full", action="store_true", help="Confere tambem audio, Blender e Unreal")
    args = parser.parse_args()
    if args.game:
        os.environ["NW_GAME"] = args.game
    nwplatform.configure()
    checks = {
        "New World assets": (Path(nwplatform.game_directory()) / "assets").is_dir(),
        "nwbt": bool(shutil.which(nwplatform.executable("nwbt"))),
        "texconv": bool(shutil.which(nwplatform.executable("texconv"))),
        "ooz.dll": Path(os.environ["NWBT_OODLE"]).is_file(),
    }
    if args.full:
        for name in ("dbgwater", "blender", "vgmstream-cli", "oggenc"):
            checks[name] = bool(shutil.which(nwplatform.executable(name)))
        for name in ("numpy", "PIL"):
            checks[name] = importlib.util.find_spec(name) is not None
        checks["wwiser"] = (nwplatform.ROOT / "vendor/wwiser/wwiser.py").is_file()
        ue = os.environ.get("NW_UE", "")
        checks["UnrealEditor-Cmd"] = bool(ue) and (
            Path(ue) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe").is_file()
    print("NW_GAME:", nwplatform.game_directory())
    for name, ok in checks.items():
        print(("OK   " if ok else "FALTA"), name)
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
