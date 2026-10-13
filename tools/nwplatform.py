"""Caminhos e ferramentas compartilhados pelos scripts Linux e Windows."""
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "bin"
WINDOWS = os.name == "nt"


def game_directory():
    configured = os.environ.get("NW_GAME") or os.environ.get("GAME")
    if configured:
        return configured
    if WINDOWS:
        return str(Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
                   / "Steam/steamapps/common/New World")
    return "/mnt/jogos/Steam/steamapps/common/New World"


def executable(name):
    override = os.environ.get("NW_" + name.upper().replace("-", "_"))
    if override:
        return override
    filename = name + (".exe" if WINDOWS else "")
    bundled = BIN / filename
    if bundled.is_file():
        return str(bundled)
    return shutil.which(filename) or filename


def environment():
    env = dict(os.environ)
    env["PATH"] = str(BIN) + os.pathsep + env.get("PATH", "")
    env.setdefault("NWBT_OODLE", str(BIN / ("ooz.dll" if WINDOWS else "libooz.so")))
    return env


def configure():
    os.environ.update(environment())
