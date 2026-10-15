"""Caminhos e ferramentas compartilhados pelos scripts Linux e Windows.

Os caminhos da máquina (NW_GAME, NW_UE, NW_BLENDER, NW_ASSETS...) vêm do ambiente ou de local.env na raiz do
repositório (fora do git; modelo em local.env.example). O ambiente vence o arquivo.
"""
import json
import os
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "bin"
WINDOWS = os.name == "nt"
LOCAL_ENV = ROOT / "local.env"
GAME_FOLDER = "New World"
UE_VERSION = "5.8.3"


def read_local_env(path=LOCAL_ENV):
    values = {}
    if path.is_file():
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, value = line.split("=", 1)
                values[key.strip()] = value.strip().strip('"').strip("'")
    return values


for _key, _value in read_local_env().items():
    os.environ.setdefault(_key, os.path.expanduser(_value))


def steam_libraries():
    """Bibliotecas da Steam desta máquina (libraryfolders.vdf), a principal primeiro."""
    if WINDOWS:
        roots = [Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Steam"]
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
                roots.insert(0, Path(winreg.QueryValueEx(key, "SteamPath")[0]))
        except OSError:
            pass
    else:
        home = Path.home()
        roots = [home / ".steam/steam", home / ".local/share/Steam", home / ".steam/debian-installation",
                 home / ".var/app/com.valvesoftware.Steam/data/Steam"]
    found = []
    for root in roots:
        vdf = root / "steamapps/libraryfolders.vdf"
        paths = [root]
        if vdf.is_file():
            text = vdf.read_text(encoding="utf-8", errors="replace")
            paths += [Path(m.replace("\\\\", "\\")) for m in re.findall(r'"path"\s*"([^"]+)"', text)]
        for p in paths:
            try:
                key = p.resolve()
            except OSError:
                continue
            if (p / "steamapps").is_dir() and key not in found:
                found.append(key)
    return found


def find_game():
    for library in steam_libraries():
        game = library / "steamapps/common" / GAME_FOLDER
        if (game / "assets").is_dir():
            return str(game)
    return None


def game_directory():
    configured = os.environ.get("NW_GAME") or os.environ.get("GAME")
    if configured:
        return configured
    found = find_game()
    if found:
        return found
    if WINDOWS:
        return str(Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
                   / "Steam/steamapps/common/New World")
    return "/mnt/jogos/Steam/steamapps/common/New World"


def ue_version(engine):
    """Versão "M.m.p" de uma pasta da engine (a que tem Engine/Build/Build.version), ou None."""
    try:
        v = json.loads((Path(engine) / "Engine/Build/Build.version").read_text(encoding="utf-8"))
        return f"{v['MajorVersion']}.{v['MinorVersion']}.{v['PatchVersion']}"
    except (OSError, ValueError, KeyError):
        return None


def ue_directory():
    """NW_UE, ou a primeira engine 5.8 achada nos lugares de costume; None se não houver."""
    configured = os.environ.get("NW_UE")
    if configured:
        return configured
    if WINDOWS:
        candidates = [*sorted(Path(os.environ.get("ProgramFiles", "C:/Program Files")).glob("Epic Games/UE_5.8*"),
                              reverse=True), Path("C:/UnrealEngine")]
    else:
        home = Path.home()
        candidates = [*sorted(home.glob("UnrealEngine*/5.8*"), reverse=True),
                      *sorted(home.glob("Linux_Unreal_Engine_5.8*"), reverse=True),
                      home / "UnrealEngine", Path("/opt/UnrealEngine"), Path("/mnt/jogos/UnrealEngine/5.8")]
    for c in candidates:
        if ue_version(c):
            return str(c)
    return None


def executable(name):
    override = os.environ.get("NW_" + name.upper().replace("-", "_"))
    if override:
        return override
    filename = name + (".exe" if WINDOWS else "")
    bundled = BIN / filename
    if bundled.is_file():
        return str(bundled)
    found = shutil.which(filename)
    if not found and name == "blender" and not WINDOWS:
        # Onde o docs/FERRAMENTAS.md instala no Linux (o tools/linux/env.sh poe no PATH): dispensa o "source".
        found = next((str(p) for p in sorted(Path.home().glob(".local/opt/blender-*/blender"), reverse=True)), None)
    return found or filename


def store_python():
    # O Python da Microsoft Store roda empacotado: filhos fora do pacote (nwbt, Blender) saem do Job
    # do Windows e sobrevivem a um aborto. Use o instalador de python.org.
    return WINDOWS and any(part.casefold() == "windowsapps" for part in Path(sys.executable).parts)


def nwbt_workers():
    # NW_NWBT_WORKERS limita "nwbt models" sem -w (padrao 8) em maquinas com pouca RAM.
    configured = os.environ.get("NW_NWBT_WORKERS")
    return ["-w", configured] if configured else []


def environment():
    env = dict(os.environ)
    env["PATH"] = str(BIN) + os.pathsep + env.get("PATH", "")
    env.setdefault("NWBT_OODLE", str(BIN / ("ooz.dll" if WINDOWS else "libooz.so")))
    env.setdefault("PYTHONUTF8", "1")
    return env


def configure():
    os.environ.update(environment())


def shell_exports():
    """Linhas "export X=..." para os scripts de tools/linux/ (local.env e caminhos achados)."""
    import shlex
    values = dict(read_local_env())
    values.update({k: v for k, v in os.environ.items() if k in values})
    values.setdefault("NW_GAME", game_directory())
    engine = ue_directory()
    if engine:
        values.setdefault("NW_UE", engine)
    return "\n".join(f"export {k}={shlex.quote(os.path.expanduser(v))}" for k, v in values.items())


if __name__ == "__main__":
    if sys.argv[1:] == ["--shell"]:
        print(shell_exports())
    else:
        print(f"NW_GAME={game_directory()}\nNW_UE={ue_directory() or ''}")
