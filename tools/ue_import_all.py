"""Builds the Unreal project from extract/ (after tools/build_all.py), native on Windows. With the editor CLOSED.

    python tools/ue_import_all.py          # all missing steps
    python tools/ue_import_all.py 05       # only step 05 (redone even if already done)
    python tools/ue_import_all.py --dry-run

Equivalent to tools/linux/ue_import_all.sh. Each step writes extract/.etapas/ue_<NN>.ok. The commandlets return an error
code for glTF material warnings: so each call checks the result line in the log, not the exit code.
The engine comes from NW_UE (default C:/UnrealEngine on Windows).
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import nwplatform
import nwprocess

ROOT = nwplatform.ROOT
PROJECT = ROOT / "unreal/NWPort/NWPort.uproject"
LOGS = ROOT / "unreal/NWPort/Saved/ImportLogs"
DONE = ROOT / "extract/.etapas"


def engine_dir():
    return Path(nwplatform.ue_directory() or ("C:/UnrealEngine" if nwplatform.WINDOWS else "/mnt/jogos/UnrealEngine/5.8"))


class Importer:
    def __init__(self, dry_run=False):
        self.dry_run = dry_run
        self.ue = engine_dir()
        binaries = "Win64" if nwplatform.WINDOWS else "Linux"
        self.editor = self.ue / "Engine/Binaries" / binaries / ("UnrealEditor-Cmd.exe" if nwplatform.WINDOWS else "UnrealEditor-Cmd")
        # The commandlet -script splits file and arguments on spaces; the repository path may contain spaces.
        # Each call uses an entry script in a folder without spaces, which sets sys.argv and runs the real script.
        self.launchers = Path(tempfile.gettempdir()) / "nw-ue-scripts"

    def launcher(self, name, script, args):
        if " " in str(self.launchers):
            raise RuntimeError(f"temporary folder with a space: {self.launchers}; set TEMP without spaces")
        path = self.launchers / f"{name}.py"
        if self.dry_run:
            return path.as_posix()
        self.launchers.mkdir(parents=True, exist_ok=True)
        argv = [str(Path(script).resolve()), *map(str, args)]
        path.write_text(
            "import runpy, sys\n"
            f"sys.argv = {json.dumps(argv)}\n"
            "runpy.run_path(sys.argv[0], run_name='__main__')\n", encoding="utf-8")
        return path.as_posix()

    def py(self, name, script, *args):
        log = LOGS / f"{name}.log"
        command = [self.editor, PROJECT, "-run=pythonscript", f"-script={self.launcher(name, script, args)}",
                   "-unattended", "-nosplash", "-nullrhi"]
        if self.dry_run:
            print("==", subprocess.list2cmdline([str(c) for c in command]), "#", script, *args, flush=True)
            return
        log.unlink(missing_ok=True)
        try:
            nwprocess.run(command, log=log, min_memory=nwprocess.memory_floor(10 * nwprocess.GIB))
        except subprocess.CalledProcessError:
            pass  # Material warnings also give an error code; the result line below decides.
        text = log.read_text(encoding="utf-8", errors="replace")
        problems = [line for line in text.splitlines() if "LogPython: Error" in line or "Traceback" in line]
        for line in problems[:5]:
            print(line, flush=True)
        if "Python script executed successfully" not in text:
            raise RuntimeError(f"failed, see {log}")

    def build(self):
        script = self.ue / "Engine/Build/BatchFiles" / ("Build.bat" if nwplatform.WINDOWS else "Linux/Build.sh")
        platform = "Win64" if nwplatform.WINDOWS else "Linux"
        # An engine built from source does not ship ShaderCompileWorker; without it the editor closes on open
        # (commandlets with -nullrhi do not need it). On a binary engine the build only checks it is up to date.
        commands = [
            [script, "NWPortEditor", platform, "Development", f"-Project={PROJECT}", "-WaitMutex", "-MaxParallelActions=6"],
            [script, "ShaderCompileWorker", platform, "Development", "-WaitMutex", "-MaxParallelActions=6"],
        ]
        for command in commands:
            if self.dry_run:
                print("==", subprocess.list2cmdline([str(c) for c in command]), flush=True)
                continue
            try:
                nwprocess.run(command, cwd=self.ue, log=LOGS / "build.log")
            except subprocess.CalledProcessError:
                errors = [line for line in (LOGS / "build.log").read_text(encoding="utf-8", errors="replace").splitlines()
                          if "error" in line.lower()]
                print("\n".join(errors[:10]), file=sys.stderr)
                raise

    def tool(self, name):
        return ROOT / "tools/ue" / f"{name}.py"

    def sets(self):
        self.py("import_sword", self.tool("import_nw"), ROOT / "extract/ue/sword_shield/manifest.json")
        self.py("import_lost", self.tool("import_nw"), ROOT / "extract/ue/lost_sword/manifest.json")
        # Effects material (arrow trail, explosion, cloud) and short transitions of the Bow montages.
        self.py("fx", self.tool("make_fx"))

    def arena(self):
        if (ROOT / "extract/ue/arena/limpa").is_dir():
            self.py("arena_meshes", self.tool("import_gatherables"), ROOT / "extract/ue/arena/limpa", "/Game/NW/Arena")
            self.py("arena_materials", self.tool("fix_nanite_materials"), "/Game/NW/Arena")
        else:
            print("no extract/ue/arena/limpa: the arena comes out without the game pieces (step 13 of build_all)", flush=True)
        self.py("test_map", self.tool("make_test_map"))

    def creatures(self):
        # G3: boar and wolf (tools/sets/*.json) and the creature template ABP (the graph is built by hand once).
        self.py("import_boar", self.tool("import_nw"), ROOT / "extract/ue/boar/manifest.json")
        self.py("import_wolf", self.tool("import_nw"), ROOT / "extract/ue/wolf/manifest.json")
        self.py("creature_abp", self.tool("make_abp"), "creature")

    def city(self):
        self.py("import_city", self.tool("import_city"),
                ROOT / "extract/cities/limpa/settlement_southwindsward_complete.glb", "SouthWindsward")
        self.py("fix_city", self.tool("fix_city"), "SouthWindsward")
        self.py("nanite_city", self.tool("nanite_on"), "/Game/NW/Cities")

    def terrain(self):
        textures = ROOT / "extract/terrain/textures/terrain/forest"
        self.py("terrain", self.tool("import_terrain"), ROOT / "extract/cities/guardaventos_terrain.glb", "SouthWindsward",
                textures / "jav_fst_feildgrass_a_diff.png", textures / "jav_fst_forestsoil_a_diff.png")
        self.py("fix_spawn", self.tool("fix_spawn"), "SouthWindsward")
        self.py("water", self.tool("import_water"), ROOT / "extract/cities/guardaventos_water.glb")

    def scenery(self):
        # A layer already imported (folder in Content/NW/Region) is not redone; to redo it, delete the folder.
        region = ROOT / "unreal/NWPort/Content/NW/Region"
        chunks = ROOT / "extract/cities/chunks"
        # Exact case comparison, as on Linux: Region/Water (import_water) is not the "water" layer.
        done = set(os.listdir(region)) if region.is_dir() else set()
        if "Capitals" not in done:
            self.py("region_capitals", self.tool("import_region_scene"), chunks / "windsward_capitals.glb", "Capitals")
        # One layer at a time (smaller files; the single package exhausted the import memory).
        for path in sorted(chunks.glob("windsward_*.glb")):
            name = path.stem.removeprefix("windsward_")
            if name == "capitals" or name in done:
                continue
            self.py(f"region_{name}", self.tool("import_region_scene"), path, name)

    def systems_city(self):
        self.py("sistemas_city", self.tool("import_city"),
                ROOT / "extract/cities/limpa/settlement_northwindsward_complete.glb", "QuedaEterna")
        self.py("sistemas_fix_city", self.tool("fix_city"), "QuedaEterna")
        self.py("sistemas_nanite", self.tool("nanite_on"), "/Game/NW/Cities/QuedaEterna")

    def systems_terrain(self):
        # Windsward forest textures until the per-biome material exists (docs/backlog.md).
        textures = ROOT / "extract/terrain/textures/terrain/forest"
        self.py("sistemas_terrain", self.tool("import_terrain"), ROOT / "extract/cities/northwindsward_terrain.glb",
                "QuedaEterna", textures / "jav_fst_feildgrass_a_diff.png", textures / "jav_fst_forestsoil_a_diff.png")
        self.py("sistemas_materials", self.tool("fix_nanite_materials"), "/Game/NW/Cities/QuedaEterna")

    def systems_npcs(self):
        self.py("sistemas_npc_anims", self.tool("import_nw"), ROOT / "extract/ue/NpcQuedaEterna/manifest.json")
        self.py("sistemas_npcs", self.tool("import_npcs"), ROOT / "extract/ue/npcs", "NpcQuedaEterna", "L_QuedaEterna")

    def pipeline(self):
        materials = ["/Game/NW/Cities", "/Game/NW/Gatherables", "/Game/NW/Region", "/Game/NW/Materials", "/Game/NW/Terrain"]
        return [
            ("01", "compile the editor C++", self.build),
            ("02", "characters, weapons and animations", self.sets),
            ("03", "test arena (L_Arena)", self.arena),
            ("04", "Windsward city", self.city),
            ("05", "region terrain and water", self.terrain),
            ("06", "gatherable meshes", lambda: self.py("gatherables", self.tool("import_gatherables"),
                                                            ROOT / "extract/ue/gatherables/limpa")),
            ("07", "region scenery", self.scenery),
            ("08", "materials (Nanite and instances)", lambda: self.py("materials", self.tool("fix_nanite_materials"), *materials)),
            ("11", "map optimization (distances, shadows, lights)", lambda: self.py("optimize", self.tool("optimize_region"))),
            ("09", "UI textures", lambda: self.py("ui", self.tool("import_ui"))),
            ("10", "ambient sounds", lambda: self.py("audio", self.tool("import_audio"))),
            ("12", "creatures (boar, wolf) and creature ABP", self.creatures),
            ("sistemas-01", "Everfall city (L_QuedaEterna)", self.systems_city),
            ("sistemas-02", "Everfall terrain and materials", self.systems_terrain),
            ("sistemas-03", "Everfall NPCs (animations, appearances, training ground on the map)", self.systems_npcs),
        ]


def check_editor_closed():
    if not nwplatform.WINDOWS:
        return
    output = subprocess.check_output(["tasklist.exe", "/FO", "CSV", "/NH"], text=True,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
    if '"unrealeditor.exe"' in output.casefold():
        raise nwprocess.ResourceError("The editor is open: close it first.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("step", nargs="?", help="Step to redo, e.g. 05 or sistemas-02")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    nwplatform.configure()
    importer = Importer(args.dry_run)
    pipeline = importer.pipeline()
    if args.step and args.step not in {name for name, _, _ in pipeline}:
        parser.error("unknown step")
    extra = {p.name for p in (ROOT / "tools/etapas").glob("*.ue.sh")} - {"sistemas.ue.sh"}
    if extra:
        parser.error("areas not ported yet: " + ", ".join(sorted(extra)))
    if not args.dry_run:
        if not importer.editor.is_file():
            print(f"UnrealEditor-Cmd not found: {importer.editor} (set NW_UE)", file=sys.stderr)
            return 2
        LOGS.mkdir(parents=True, exist_ok=True)
    runner = nwprocess.Steps(DONE, only=args.step and f"ue_{args.step}", dry_run=args.dry_run,
                             before=check_editor_closed)
    try:
        for name, description, action in pipeline:
            runner.run(f"ue_{name}", description, action)
    except nwprocess.ResourceError as exc:
        print(exc, file=sys.stderr)
        return 3
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130
    if args.step:
        print(f"Step ue_{args.step} done.")
        return 0
    print(f"Done. Open the editor: {importer.ue / 'Engine/Binaries/Win64/UnrealEditor.exe'} {PROJECT} "
          "/Game/NW/Maps/L_SouthWindsward")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
