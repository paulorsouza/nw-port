"""Diagnostico inicial, sem extrair assets nem iniciar a Unreal."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nwplatform
import build_nwbt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", help="Diretorio da instalacao do New World")
    parser.add_argument("--full", action="store_true", help="Confere tambem audio, Blender e Unreal")
    args = parser.parse_args()
    if args.game:
        os.environ["NW_GAME"] = args.game
    nwplatform.configure()
    checks = {
        "Python fora da Microsoft Store": not nwplatform.store_python(),
        "New World assets": (Path(nwplatform.game_directory()) / "assets").is_dir(),
        "nwbt": bool(shutil.which(nwplatform.executable("nwbt"))),
        "texconv": bool(shutil.which(nwplatform.executable("texconv"))),
        "ooz.dll": Path(os.environ["NWBT_OODLE"]).is_file(),
    }
    if args.full:
        for name in ("dbgwater", "blender", "vgmstream-cli", "oggenc"):
            checks[name] = bool(shutil.which(nwplatform.executable(name)))
        # tools/make_terrain.py roda "go run ./dbgheight" na etapa 07.
        checks["go"] = bool(shutil.which("go"))
        for name in ("numpy", "PIL"):
            checks[name] = importlib.util.find_spec(name) is not None
        checks["wwiser"] = (nwplatform.ROOT / "vendor/wwiser/wwiser.py").is_file()
        ue = os.environ.get("NW_UE", "")
        checks["UnrealEditor-Cmd"] = bool(ue) and (
            Path(ue) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe").is_file()
    # nwbt compilado com o patch deste checkout (patches/nwbt.patch, tools/build_nwbt.py).
    notes = {}
    if checks["nwbt"]:
        checks["nwbt do patch atual"], notes["nwbt do patch atual"] = build_nwbt.check_binary()
        if args.full and checks["New World assets"]:
            # Converte um cubo do jogo por "@lista" (carrega os paks, ~20 s).
            checks["nwbt @lista"], notes["nwbt @lista"] = build_nwbt.test_argfile()
    print("NW_GAME:", nwplatform.game_directory())
    for name, ok in checks.items():
        print(("OK   " if ok else "FALTA"), name, ("- " + notes[name]) if name in notes and not ok else "")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
