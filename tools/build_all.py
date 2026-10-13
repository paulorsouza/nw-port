"""Extracao nativa Windows/Linux em etapas retomaveis; --dry-run nao escreve."""
import argparse
import csv
import importlib.util
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

import nwplatform
import nwprocess
from nwassets import complete_glb

LEVEL = "newworld_vitaeeterna"
REGION = "r_+04_+01"
LAYERS = (
    "roaddressing", "water", "06sw01_furybay_props", "06sw02_cascadeharbor_props",
    "06sw03_noblereach_props", "06sw04_stoutmarch_props", "06sw05_greenhaven_props",
    "06sw06_eldergate_props", "06sw07_perilbrook_props", "06sw08_grenville_props",
    "06sw09_primrose_prop", "06sw_heartforge", "06sw_medea", "terrain_softup_hdeform_a",
    "terrain_softup_all_hdeform_c", "06swindsward_lore", "06swindsward_persistence",
)


def running_names():
    if nwplatform.WINDOWS:
        output = subprocess.check_output(["tasklist.exe", "/FO", "CSV", "/NH"],
                                         text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        return {row[0].casefold() for row in csv.reader(output.splitlines()) if row}
    result = set()
    for process in Path("/proc").glob("[0-9]*"):
        try:
            result.add((process / "comm").read_text().strip().casefold())
            for argument in (process / "cmdline").read_bytes().split(b"\0"):
                result.add(Path(argument.decode(errors="replace")).name.casefold())
        except (OSError, ProcessLookupError):
            pass
    return result


def check_closed():
    blocked = {"newworld.exe", "newworldlauncher.exe", "unrealeditor.exe", "unrealeditor-cmd.exe", "unrealeditor", "unrealeditor-cmd"}
    active = running_names() & blocked
    if active:
        raise nwprocess.ResourceError("feche antes da extracao: " + ", ".join(sorted(active)))


def write_manifest(source, paths, icons):
    paths.parent.mkdir(parents=True, exist_ok=True)
    icons.parent.mkdir(parents=True, exist_ok=True)
    temporary_paths, temporary_icons = paths.with_suffix(".part"), icons.with_suffix(".part")
    count = 0
    with source.open(encoding="utf-8-sig", newline="") as stream, temporary_paths.open("w", encoding="utf-8", newline="\n") as p, temporary_icons.open("w", encoding="utf-8", newline="\n") as i:
        for row in csv.reader(stream, delimiter=";"):
            if len(row) != 4 or not row[0]:
                raise ValueError(f"linha invalida no manifesto CSV: {row}")
            name = row[0]
            p.write(name + "\n")
            if re.fullmatch(r"lyshineui/images/icons/items/.*\.dds", name):
                i.write(name + "\n")
            count += 1
    if not count:
        raise RuntimeError("nwbt retornou um manifesto vazio")
    temporary_icons.replace(icons)
    temporary_paths.replace(paths)
    print(f"manifesto: {count} arquivos", flush=True)


class Extraction:
    def __init__(self, game, dry_run=False):
        self.root = nwplatform.ROOT
        self.game = game
        self.dry_run = dry_run
        buddy = Path(os.environ.get("NW_BUDDY", str(self.root / "vendor/nw-buddy"))).resolve()
        self.rtti = buddy / "tools/nwbt/rtti/nwt"
        self.logs = self.root / "extract/logs"
        self.log = self.logs / "build.log"

    def run(self, command, heavy=True):
        nwprocess.run(command, log=self.log, min_memory=10 * nwprocess.GIB if heavy else 0,
                      dry_run=self.dry_run)

    def py(self, name, *args, heavy=True):
        self.run([sys.executable, f"tools/{name}.py", *args], heavy=heavy)

    def nwbt(self, *args):
        self.run([nwplatform.executable("nwbt"), *args])

    def objects(self):
        target = self.root / ".nwbt/rtti"
        names = ("nwt-crc.json", "nwt-types.json")
        if not self.dry_run:
            target.mkdir(parents=True, exist_ok=True)
            for name in names:
                destination = target / name
                shutil.copy2(self.rtti / name, destination)
                if not nwplatform.WINDOWS:
                    # Compativel tambem com bin/nwbt Linux anterior ao patch.
                    destination.chmod(destination.stat().st_mode | stat.S_IXUSR)
        return ["--crc-file", target / names[0], "--uuid-file", target / names[1]]

    def unpack(self, output, expression, workers=4, *extra):
        self.nwbt("unpack", "-g", self.game, "-o", output, "-e", "-w", str(workers), *extra, expression)

    def tools(self):
        if self.dry_run:
            print("== conferir ferramentas, Python numpy/PIL, jogo e RTTI", flush=True)
            return
        missing = []
        for name in ("nwbt", "dbgwater", "texconv", "blender", "vgmstream-cli", "oggenc"):
            if not shutil.which(nwplatform.executable(name)):
                missing.append(name)
        if not Path(os.environ["NWBT_OODLE"]).is_file():
            missing.append("NWBT_OODLE")
        for module in ("numpy", "PIL"):
            if importlib.util.find_spec(module) is None:
                missing.append(module)
        if not (Path(self.game) / "assets").is_dir():
            missing.append(f"NW_GAME/assets: {self.game}")
        if not (self.root / "vendor/wwiser/wwiser.py").is_file():
            missing.append("vendor/wwiser")
        for name in ("nwt-crc.json", "nwt-types.json"):
            if not (self.rtti / name).is_file():
                missing.append(str(self.rtti / name))
        if not nwplatform.WINDOWS and not shutil.which("wine"):
            missing.append("wine")
        if missing:
            raise FileNotFoundError("falta: " + ", ".join(missing))

    def manifest(self):
        temporary = self.root / "extract/manifest/paths.part.csv"
        if not self.dry_run:
            temporary.parent.mkdir(parents=True, exist_ok=True)
        self.nwbt("list", "-g", self.game, "--csv", "-o", temporary)
        if not self.dry_run:
            write_manifest(temporary, self.root / "extract/manifest/paths.txt", self.root / "extract/ui/item_icons.txt")

    def tables(self):
        self.unpack("extract/data", r"sharedassets/springboardentitites/datatables/.*\.datasheet$", 4, "--x-datasheet", "json")
        self.unpack("extract/loc", r"localization/(pt-br|en-us)/.*\.loc\.xml$")

    def ui(self):
        self.unpack("extract/ui", r"lyshineui/.*\.dynamicuicanvas$", 4, "--x-objects", "json", *self.objects())
        self.unpack("extract/ui/tex", r"lyshineui/fonts/.*\.otf$", 2)
        self.unpack("extract/ui/luac", r"lyshineui/.*\.luac$", 2)

    def terrain_data(self):
        self.unpack("extract/terrain", rf"sharedassets/coatlicue/{LEVEL}/(regions/r_[^/]+/(capitals/[^/]+/[^/]+\.capitals\.json|mapsettings\.json|region\.(heightmap|distribution|waterqt|tractmap\.tif))|[a-z]+\.json)$")
        self.unpack("extract/terrain", r"materials/terrain/.*forest/.*(feildgrass|forestsoil)_a_diff\.dds$", 2, "--x-dds", "png")

    def terrain(self):
        base = f"extract/terrain/sharedassets/coatlicue/{LEVEL}/regions/{REGION}"
        if self.dry_run:
            capital = "<capitals.json localizado em execucao>"
        else:
            matches = sorted((self.root / base / "capitals/06swindsward_persistence").glob("*.json"))
            if len(matches) != 1:
                raise RuntimeError(f"esperado um capitals.json para Guardaventos; encontrados {len(matches)}")
            capital = matches[0]
        self.py("make_terrain", "extract/terrain", capital, "Settlement_SouthWindsward_Complete",
                "extract/cities/guardaventos_terrain.glb", "--region", REGION)
        water = f"extract/terrain/water_{REGION}.f32"
        self.run([nwplatform.executable("dbgwater"), f"{base}/region.waterqt", water])
        self.py("make_water", water, capital, "Settlement_SouthWindsward_Complete",
                "extract/cities/guardaventos_water.glb", "--region", REGION)

    def scenery(self):
        output = self.root / "extract/cities/chunks"
        if not self.dry_run:
            output.mkdir(parents=True, exist_ok=True)
        common = ["-g", self.game, "-o", output, "--binary", "--embed", "--ior", "1.5", "-w", "2",
                  "--texture-size", "2048", *self.objects(), "--level", LEVEL, "--region", REGION]
        for layer, filename in [("06swindsward_capitals", "windsward_capitals.glb"),
                                *[(name, f"windsward_{name}.glb") for name in LAYERS]]:
            if complete_glb(output / filename):
                continue
            # Erros de conversao deixam a etapa pendente; nao viram "sem conteudo".
            self.nwbt("models", "chunks", *common, "--name", layer, "-f", filename)
            if not self.dry_run and not complete_glb(output / filename):
                if layer == "06swindsward_capitals":
                    raise RuntimeError("nwbt nao gerou o GLB obrigatorio dos capitals")
                print(f"aviso: camada {layer} sem GLB; conferir o log", flush=True)

    def data(self):
        poi = self.root / "extract/pois/slices/pois/territories/r_+04_+01_pois.dynamicslice.json"
        if not poi.is_file():
            self.unpack("extract/pois", r"^slices/pois/territories/r_\+04_\+01_pois\.dynamicslice$", 1,
                        "--x-objects", "json", *self.objects())
        self.py("make_data", heavy=False)
        if self.dry_run:
            print("== em execucao: extrair as texturas listadas por Data/ que ainda nao existem", flush=True)
            return
        wanted = set()
        for listing in (self.root / "extract/ui").glob("*_textures.txt"):
            for line in listing.read_text(encoding="utf-8-sig").splitlines():
                texture = line.strip()
                if texture and not (self.root / "extract/ui/tex" / str(Path(texture).with_suffix(".png"))).is_file():
                    wanted.add(texture)
        if wanted:
            pattern = "^(" + "|".join(re.escape(name) for name in sorted(wanted)) + ")$"
            self.unpack("extract/ui/tex", pattern, 2, "--x-dds", "png")

    def systems_slices(self):
        for pattern in (
            r"slices/(structures/settlements/v2_settlements/settlement_northwindsward_complete|housing/plots/northwindsward/[^/]+)\.dynamicslice$",
            r"coatgen/(7b53567b|ba5fa8e9|8d7747d3)/[^/]*npc[^/]*\.dynamicslice$",
        ):
            self.unpack("extract/slicejson", pattern, 2, "--x-objects", "json", *self.objects())

    def pipeline(self):
        def audio():
            self.py("extract_audio")
            self.py("render_ambient", "windsward", heavy=False)
        def gatherables():
            self.py("make_gatherables", heavy=False)
            self.py("convert_gatherables")
        def characters():
            self.py("build_set", "tools/sets/sword_shield.json")
            self.py("build_set", "tools/sets/lost_sword.json")
        return [
            ("00", "ferramentas", self.tools),
            ("01", "lista dos paks", self.manifest),
            ("02", "tabelas e textos", self.tables),
            ("03", "telas da interface", self.ui),
            ("04", "receitas de animacao", lambda: self.unpack("extract/models", r"animations/mannequin/adb/.*\.(adb|xml)$")),
            ("05", "audio", audio),
            ("06", "dados da regiao", self.terrain_data),
            ("07", "cidade", lambda: self.py("convert_city", "settlement_southwindsward_complete", heavy=False)),
            ("08", "terreno e agua", self.terrain),
            ("09", "coletaveis", gatherables),
            ("10", "cenario da regiao", self.scenery),
            ("11", "personagens e animacoes", characters),
            ("12", "dados e texturas da interface", self.data),
            ("13", "pecas da arena", lambda: self.py("convert_gatherables", "tools/arena/meshes.txt", "extract/ue/arena")),
            ("sistemas-01", "cidade de Queda Eterna", lambda: self.py("convert_city", "settlement_northwindsward_complete", heavy=False)),
            ("sistemas-02", "entidades e lotes de Queda Eterna", self.systems_slices),
            ("sistemas-03", "timelines dos NPCs", lambda: self.unpack("extract/timelines", r"libs/timeline/npc/.*\.timeline$", 2,
                                                                       "--x-objects", "json", *self.objects())),
        ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("step", nargs="?", help="Etapa a refazer, ex.: 07 ou sistemas-02")
    parser.add_argument("--game", default=nwplatform.game_directory())
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    nwplatform.configure()
    os.environ["NW_GAME"] = str(Path(args.game).resolve())
    extraction = Extraction(os.environ["NW_GAME"], args.dry_run)
    pipeline = extraction.pipeline()
    if args.step and args.step not in {name for name, _, _ in pipeline}:
        parser.error("etapa desconhecida")
    extra = {p.name for p in (nwplatform.ROOT / "tools/etapas").glob("*.build.sh")} - {"sistemas.build.sh"}
    if extra:
        parser.error("esteiras ainda nao portadas: " + ", ".join(sorted(extra)))
    runner = nwprocess.Steps(nwplatform.ROOT / "extract/.etapas", only=args.step,
                             dry_run=args.dry_run, before=check_closed)
    def execute():
        extraction.py("jogo_versao", heavy=False)
        for name, description, action in pipeline:
            extraction.log = extraction.logs / f"{name}.log"
            runner.run(name, description, action)
    try:
        if args.dry_run:
            execute()
        else:
            with nwprocess.heavy_lock():
                execute()
    except nwprocess.ResourceError as exc:
        print(exc, file=sys.stderr)
        return 3
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(exc, file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130
    print("Extracao planejada." if args.dry_run else "Extracao concluida; proximo: importacao Unreal.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
