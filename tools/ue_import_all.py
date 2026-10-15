"""Monta o projeto Unreal a partir de extract/ (depois de tools/build_all.py), nativo no Windows. Com o editor FECHADO.

    python tools/ue_import_all.py          # todas as etapas que faltam
    python tools/ue_import_all.py 05       # so a etapa 05 (refeita mesmo se ja feita)
    python tools/ue_import_all.py --dry-run

Equivale a tools/linux/ue_import_all.sh. Cada etapa grava extract/.etapas/ue_<NN>.ok. Os commandlets devolvem codigo de
erro por avisos de material do glTF: por isso cada chamada confere a linha de resultado no log, nao o codigo de saida.
A engine vem de NW_UE (padrao C:/UnrealEngine no Windows).
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
        # O -script do commandlet separa arquivo e argumentos por espaco; o repositorio pode ter espacos no caminho.
        # Cada chamada usa um script de entrada numa pasta sem espacos, que monta sys.argv e roda o script real.
        self.launchers = Path(tempfile.gettempdir()) / "nw-ue-scripts"

    def launcher(self, name, script, args):
        if " " in str(self.launchers):
            raise RuntimeError(f"pasta temporaria com espaco: {self.launchers}; defina TEMP sem espacos")
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
            pass  # Avisos de material tambem dao codigo de erro; vale a linha de resultado abaixo.
        text = log.read_text(encoding="utf-8", errors="replace")
        problems = [line for line in text.splitlines() if "LogPython: Error" in line or "Traceback" in line]
        for line in problems[:5]:
            print(line, flush=True)
        if "Python script executed successfully" not in text:
            raise RuntimeError(f"falhou, ver {log}")

    def build(self):
        script = self.ue / "Engine/Build/BatchFiles" / ("Build.bat" if nwplatform.WINDOWS else "Linux/Build.sh")
        platform = "Win64" if nwplatform.WINDOWS else "Linux"
        # Engine compilada do fonte nao traz o ShaderCompileWorker; sem ele o editor fecha ao abrir (commandlets
        # com -nullrhi nao precisam). Num engine binario o build so confere que esta em dia.
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
        # Material de efeitos (rastro da flecha, explosao, nuvem) e transicoes curtas das montagens do arco.
        self.py("fx", self.tool("make_fx"))

    def arena(self):
        if (ROOT / "extract/ue/arena/limpa").is_dir():
            self.py("arena_meshes", self.tool("import_gatherables"), ROOT / "extract/ue/arena/limpa", "/Game/NW/Arena")
            self.py("arena_materials", self.tool("fix_nanite_materials"), "/Game/NW/Arena")
        else:
            print("sem extract/ue/arena/limpa: a arena sai sem as pecas do jogo (etapa 13 do build_all)", flush=True)
        self.py("test_map", self.tool("make_test_map"))

    def creatures(self):
        # G3: javali e lobo (tools/sets/*.json) e o ABP de modelo das criaturas (o grafo e montado a mao uma vez).
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
        # Camada ja importada (pasta em Content/NW/Region) nao e refeita; para refazer, apague a pasta.
        region = ROOT / "unreal/NWPort/Content/NW/Region"
        chunks = ROOT / "extract/cities/chunks"
        # Comparacao exata de maiusculas, como no Linux: Region/Water (import_water) nao e a camada "water".
        done = set(os.listdir(region)) if region.is_dir() else set()
        if "Capitals" not in done:
            self.py("region_capitals", self.tool("import_region_scene"), chunks / "windsward_capitals.glb", "Capitals")
        # Uma camada por vez (arquivos menores; o pacote unico esgotava a memoria do import).
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
        # Texturas de floresta de Guardaventos ate o material por bioma existir (docs/pendencias.md).
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
            ("01", "compilar o C++ do editor", self.build),
            ("02", "personagens, armas e animacoes", self.sets),
            ("03", "arena de testes (L_Arena)", self.arena),
            ("04", "cidade de Guardaventos", self.city),
            ("05", "terreno e agua da regiao", self.terrain),
            ("06", "malhas dos coletaveis", lambda: self.py("gatherables", self.tool("import_gatherables"),
                                                            ROOT / "extract/ue/gatherables/limpa")),
            ("07", "cenario da regiao", self.scenery),
            ("08", "materiais (Nanite e instancias)", lambda: self.py("materials", self.tool("fix_nanite_materials"), *materials)),
            ("11", "otimizacao do mapa (distancias, sombras, luzes)", lambda: self.py("optimize", self.tool("optimize_region"))),
            ("09", "texturas da interface", lambda: self.py("ui", self.tool("import_ui"))),
            ("10", "sons de fundo", lambda: self.py("audio", self.tool("import_audio"))),
            ("12", "criaturas (javali, lobo) e ABP de criatura", self.creatures),
            ("sistemas-01", "cidade de Queda Eterna (L_QuedaEterna)", self.systems_city),
            ("sistemas-02", "terreno de Queda Eterna e materiais", self.systems_terrain),
            ("sistemas-03", "NPCs de Queda Eterna (animacoes, aparencias, campo no mapa)", self.systems_npcs),
        ]


def check_editor_closed():
    if not nwplatform.WINDOWS:
        return
    output = subprocess.check_output(["tasklist.exe", "/FO", "CSV", "/NH"], text=True,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
    if '"unrealeditor.exe"' in output.casefold():
        raise nwprocess.ResourceError("O editor esta aberto: feche antes.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("step", nargs="?", help="Etapa a refazer, ex.: 05 ou sistemas-02")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    nwplatform.configure()
    importer = Importer(args.dry_run)
    pipeline = importer.pipeline()
    if args.step and args.step not in {name for name, _, _ in pipeline}:
        parser.error("etapa desconhecida")
    extra = {p.name for p in (ROOT / "tools/etapas").glob("*.ue.sh")} - {"sistemas.ue.sh"}
    if extra:
        parser.error("esteiras ainda nao portadas: " + ", ".join(sorted(extra)))
    if not args.dry_run:
        if not importer.editor.is_file():
            print(f"UnrealEditor-Cmd nao encontrado: {importer.editor} (defina NW_UE)", file=sys.stderr)
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
        print(f"Etapa ue_{args.step} concluida.")
        return 0
    print(f"Pronto. Abra o editor: {importer.ue / 'Engine/Binaries/Win64/UnrealEditor.exe'} {PROJECT} "
          "/Game/NW/Maps/L_SouthWindsward")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
