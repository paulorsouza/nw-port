#!/usr/bin/env python3
"""What this machine has and which setup path applies (.claude/skills/nw-setup). Reads only; changes nothing.

    python3 tools/setup_status.py          # summary
    python3 tools/setup_status.py --json   # same, for scripts
    python3 tools/setup_status.py --exe    # also hashes NewWorld.exe (slower, exact build check)

Paths:
    ready            the Unreal project is already imported: open the editor
    import-zips      asset zips found (tools/assets_import.py): import them
    build-from-game  New World installed at the team build: extract and import
    wrong-build      New World installed, but not the build in tools/game.version
    no-source        neither zips nor the game: nothing to build the project from
"""
import argparse
import importlib.util
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nwplatform  # noqa: E402  (loads local.env)

ROOT = nwplatform.ROOT
MARKERS = ROOT / "extract/.etapas"
MIN_PYTHON = (3, 11)
GiB = 1024 ** 3


def total_memory():
    if nwplatform.WINDOWS:
        import ctypes

        class Status(ctypes.Structure):
            _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong),
                        ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
                        ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
                        ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
                        ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]
        s = Status()
        s.dwLength = ctypes.sizeof(Status)
        ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(s))
        return s.ullTotalPhys
    try:
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith("MemTotal:"):
                return int(line.split()[1]) * 1024
    except OSError:
        pass
    return None


def steps(module, factory):
    """Step names of build_all.py / ue_import_all.py, without running anything."""
    try:
        mod = __import__(module)
        return [name for name, _, _ in factory(mod).pipeline()]
    except Exception as exc:  # a broken pipeline must not hide the rest of the report
        return {"error": str(exc)}


def missing_markers(names, prefix=""):
    if isinstance(names, dict):
        return names
    return [n for n in names if not (MARKERS / f"{prefix}{n}.ok").is_file()]


def game_status(check_exe):
    game = Path(nwplatform.game_directory())
    info = {"path": str(game), "found": (game / "assets").is_dir() and (game / "Bin64").is_dir(),
            "source": "NW_GAME" if os.environ.get("NW_GAME") else "steam" if nwplatform.find_game() else "default"}
    if info["found"]:
        import game_version
        errors = game_version.check_version(game.resolve(), ROOT / "tools/game.version", check_exe)
        info["team_build"] = not errors
        info["build_errors"] = errors
    return info


def unreal_status():
    engine = nwplatform.ue_directory()
    version = engine and nwplatform.ue_version(engine)
    editor = None
    if engine:
        editor = Path(engine) / ("Engine/Binaries/Win64/UnrealEditor.exe" if nwplatform.WINDOWS
                                 else "Engine/Binaries/Linux/UnrealEditor")
    return {"path": engine, "version": version, "required": nwplatform.UE_VERSION,
            "ok": version == nwplatform.UE_VERSION and bool(editor and editor.is_file()),
            "editor": str(editor) if editor else None}


def zips_status():
    folder = Path(os.environ.get("NW_ASSETS") or Path.home() / "nw-port-assets")
    manifest = folder / "manifest.json"
    info = {"folder": str(folder), "manifest": manifest.is_file()}
    if not info["manifest"]:
        return info
    try:
        m = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        info["error"] = str(exc)
        return info
    names = list(m.get("bundles", {}))
    present = [n for n in names if (folder / "bundles" / n).is_file() or (folder / n).is_file()]
    info.update({"bundles": len(names), "present": len(present), "ue_version": m.get("ue_version"),
                 "game_build": m.get("game_build"), "machine": m.get("machine"), "created_at": m.get("created_at")})
    return info


def which(name):
    return bool(shutil.which(nwplatform.executable(name)))


def tools_status():
    nwplatform.configure()
    t = {
        "git": bool(shutil.which("git")),
        "python": sys.version_info[:2] >= MIN_PYTHON and not nwplatform.store_python(),
        "numpy": importlib.util.find_spec("numpy") is not None,
        "Pillow": importlib.util.find_spec("PIL") is not None,
        "nwbt": which("nwbt"),
        "ooz": Path(os.environ["NWBT_OODLE"]).is_file(),
        "texconv": which("texconv"),
        "dbgwater": which("dbgwater"),
        "blender": which("blender"),
        "go": bool(shutil.which("go") or (Path.home() / ".local/go/bin/go").is_file()),
        "wwiser": (ROOT / "vendor/wwiser/wwiser.py").is_file(),
        "vgmstream-cli": which("vgmstream-cli"),
        "oggenc": which("oggenc"),
    }
    if not nwplatform.WINDOWS:
        t["wine"] = bool(shutil.which("wine"))
    if t["nwbt"]:
        import build_nwbt
        t["nwbt_patch"], _ = build_nwbt.check_binary()
    return t


EXTRACT_TOOLS = ["nwbt", "nwbt_patch", "ooz", "texconv", "dbgwater", "blender", "go", "wwiser", "vgmstream-cli",
                 "oggenc", "numpy", "Pillow", "wine"]


def report(check_exe):
    disk = shutil.disk_usage(ROOT)
    r = {
        "os": f"{platform.system()} {platform.release()}",
        "repo": str(ROOT),
        "local_env": nwplatform.LOCAL_ENV.is_file(),
        "memory_gib": round((total_memory() or 0) / GiB),
        "disk_free_gib": round(disk.free / GiB),
        "unreal": unreal_status(),
        "game": game_status(check_exe),
        "zips": zips_status(),
        "tools": tools_status(),
        "extract_missing": missing_markers(steps("build_all", lambda m: m.Extraction(nwplatform.game_directory(), True))),
        "import_missing": missing_markers(steps("ue_import_all", lambda m: m.Importer(True)), "ue_"),
    }
    content = ROOT / "unreal/NWPort/Content/NW"
    r["content_present"] = content.is_dir() and any(content.iterdir())
    if r["content_present"] and r["import_missing"] == []:
        path = "ready"
    elif r["zips"].get("manifest"):
        path = "import-zips"
    elif r["game"]["found"] and r["game"].get("team_build"):
        path = "build-from-game"
    elif r["game"]["found"]:
        path = "wrong-build"
    else:
        path = "no-source"
    r["path"] = path
    blockers = []
    if not r["tools"]["git"] or not r["tools"]["python"]:
        blockers.append(f"git and Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]}+ (python.org build on Windows)")
    if not r["unreal"]["ok"]:
        blockers.append(f"Unreal Engine {nwplatform.UE_VERSION}")
    if path == "build-from-game":
        missing = [k for k in EXTRACT_TOOLS if k in r["tools"] and not r["tools"][k]]
        if missing:
            blockers.append("extraction tools: " + ", ".join(missing))
    if path == "import-zips":
        z = r["zips"]
        if z.get("present", 0) < z.get("bundles", 0):
            blockers.append(f"{z['bundles'] - z['present']} of {z['bundles']} zips not downloaded yet")
        if z.get("ue_version") and z["ue_version"] != r["unreal"]["version"]:
            blockers.append(f"zips were made with Unreal {z['ue_version']}")
    r["blockers"] = blockers
    return r


def mark(ok):
    return "ok     " if ok else "MISSING"


def print_report(r):
    print(f"System   {r['os']}, {r['memory_gib']} GiB RAM, {r['disk_free_gib']} GiB free at {r['repo']}")
    print(f"local.env {'present' if r['local_env'] else 'absent (defaults and auto-detection)'}")
    u = r["unreal"]
    print(f"{mark(u['ok'])} Unreal {u['required']}: {u['path'] or 'not found'}"
          + (f" (version {u['version']})" if u["version"] and not u["ok"] else ""))
    g = r["game"]
    build = "" if not g["found"] else " (team build)" if g.get("team_build") else " (" + "; ".join(g["build_errors"]) + ")"
    print(f"{mark(g['found'])} New World: {g['path']}{build}")
    z = r["zips"]
    if z.get("manifest"):
        print(f"ok      asset zips: {z['folder']} ({z.get('present', 0)}/{z.get('bundles', 0)} zips, "
              f"from {z.get('machine')} {z.get('created_at')})")
    else:
        print(f"-       asset zips: none in {z['folder']}")
    t = r["tools"]
    print("tools    " + ", ".join(f"{k}{'' if v else ' MISSING'}" for k, v in t.items()))
    em, im = r["extract_missing"], r["import_missing"]
    print(f"extract  {'done' if em == [] else em}")
    print(f"import   {'done' if im == [] else im}")
    print(f"\nPATH: {r['path']}")
    for b in r["blockers"]:
        print(f"  needs: {b}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--exe", action="store_true", help="hash NewWorld.exe against tools/game.version")
    a = ap.parse_args()
    r = report(a.exe)
    if a.json:
        print(json.dumps(r, indent=1))
    else:
        print_report(r)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
