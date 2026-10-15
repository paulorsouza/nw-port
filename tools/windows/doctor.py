"""Initial diagnostics, without extracting assets or starting Unreal."""
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
    parser.add_argument("--game", help="New World install directory")
    parser.add_argument("--full", action="store_true", help="Also checks audio, Blender and Unreal")
    args = parser.parse_args()
    if args.game:
        os.environ["NW_GAME"] = args.game
    nwplatform.configure()
    checks = {
        "Python not from the Microsoft Store": not nwplatform.store_python(),
        "New World assets": (Path(nwplatform.game_directory()) / "assets").is_dir(),
        "nwbt": bool(shutil.which(nwplatform.executable("nwbt"))),
        "texconv": bool(shutil.which(nwplatform.executable("texconv"))),
        "ooz.dll": Path(os.environ["NWBT_OODLE"]).is_file(),
    }
    if args.full:
        for name in ("dbgwater", "blender", "vgmstream-cli", "oggenc"):
            checks[name] = bool(shutil.which(nwplatform.executable(name)))
        # tools/make_terrain.py runs "go run ./dbgheight" in step 07.
        checks["go"] = bool(shutil.which("go"))
        for name in ("numpy", "PIL"):
            checks[name] = importlib.util.find_spec(name) is not None
        checks["wwiser"] = (nwplatform.ROOT / "vendor/wwiser/wwiser.py").is_file()
        ue = os.environ.get("NW_UE", "")
        checks["UnrealEditor-Cmd"] = bool(ue) and (
            Path(ue) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe").is_file()
    # nwbt built with this checkout's patch (patches/nwbt.patch, tools/build_nwbt.py).
    notes = {}
    if checks["nwbt"]:
        checks["nwbt of the current patch"], notes["nwbt of the current patch"] = build_nwbt.check_binary()
        if args.full and checks["New World assets"]:
            # Converts a cube from the game via "@list" (loads the paks, ~20 s).
            checks["nwbt @list"], notes["nwbt @list"] = build_nwbt.test_argfile()
    print("NW_GAME:", nwplatform.game_directory())
    for name, ok in checks.items():
        print(("OK     " if ok else "MISSING"), name, ("- " + notes[name]) if name in notes and not ok else "")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
