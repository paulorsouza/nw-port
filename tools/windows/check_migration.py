"""Confere caches WSL no Windows; --apply corrige referencias com backup."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shutil
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nwplatform
import nwprocess
from nwassets import complete_glb
from convert_gatherables import mesh_paths

FILE_SUFFIXES = {".glb", ".fbx", ".png", ".blend", ".cdf", ".wem", ".bnk", ".txtp"}
ANCHORS = ("/extract/", "/unreal/NWPort/")


def normalized(value):
    return value.replace("\\", "/")


def absolute(value):
    return value.startswith("/") or PureWindowsPath(value).is_absolute()


def fingerprint(path):
    if path.is_symlink():
        return ("link", os.readlink(path))
    if path.is_file():
        return ("file", hashlib.sha256(path.read_bytes()).hexdigest())
    if not path.exists():
        return ("missing", "")
    raise ValueError(f"esperado arquivo, encontrado diretorio: {path}")


class Audit:
    def __init__(self, root, old_roots=()):
        self.root = Path(root).resolve()
        self.old_roots = [normalized(value).rstrip("/") for value in old_roots]
        self.issues = []
        self.actions = {}
        self.rerun = set()
        self.json_files = 0
        self.reusable_glbs = 0
        self.markers = self.root / "extract/.etapas"
        self.backup = None

    def relative(self, path):
        return str(path.relative_to(self.root)).replace("\\", "/")

    def issue(self, code, path, detail, *, level="warning", fixable=False):
        self.issues.append(dict(code=code, file=self.relative(path), detail=detail,
                                level=level, fixable=fixable))

    def add_action(self, path, kind, *, data=None, source=None, before=None):
        path = Path(path)
        # O arquivo/link final pode apontar para fora, mas seu diretorio nao pode.
        if not path.parent.resolve().is_relative_to(self.root):
            raise ValueError(f"diretorio fora do projeto: {path}")
        self.actions[path] = dict(kind=kind, before=before or fingerprint(path), data=data, source=source,
                                 source_before=fingerprint(source) if source else None)

    def invalidate(self, stage):
        self.rerun.add(stage)
        marker = self.markers / (stage + ".ok")
        if marker.is_file() or marker.is_symlink():
            if marker not in self.actions:
                self.issue("marker", marker, f"marcador sera invalidado: {stage}", fixable=True)
                self.add_action(marker, "invalidate")

    def candidate(self, value, document):
        value = normalized(value)
        if value.startswith(("/Game/", "/Engine/")):
            return None, "engine"
        candidates = []
        if not absolute(value):
            candidates = [document.parent / value, self.root / value]
        else:
            # Nunca aceite um arquivo existente em outro checkout como destino.
            root_text = self.root.as_posix().rstrip("/")
            left = value.casefold() if nwplatform.WINDOWS else value
            right = root_text.casefold() if nwplatform.WINDOWS else root_text
            if left.startswith(right + "/"):
                candidates.append(self.root / value[len(root_text) + 1:])
            explicit = False
            for old_root in self.old_roots:
                insensitive = bool(PureWindowsPath(old_root).drive)
                left = value.casefold() if insensitive else value
                right = old_root.casefold() if insensitive else old_root
                if left.startswith(right + "/"):
                    candidates.append(self.root / value[len(old_root) + 1:])
                    explicit = True
            if not explicit:
                for anchor in ANCHORS:
                    start = 0
                    while (index := value.find(anchor, start)) >= 0:
                        candidates.append(self.root / value[index + 1:])
                        start = index + len(anchor)
        found = {}
        for candidate in candidates:
            copying = self.actions.get(candidate, {}).get("kind") == "copy"
            resolved = candidate.parent.resolve() / candidate.name if copying else candidate.resolve()
            if resolved.is_relative_to(self.root) and (copying or candidate.is_file() and candidate.stat().st_size > 0):
                found[str(resolved)] = resolved
        if len(found) == 1:
            return next(iter(found.values())), "found"
        return None, "ambiguous" if found else "missing"

    def set_stage(self, document, name=None):
        if document.parent.name in ("sword_shield", "lost_sword"):
            self.invalidate("11")
            self.invalidate("ue_02")
            return
        # Conjuntos extras sao gerados manualmente, fora da etapa 11.
        found = False
        for spec in (self.root / "tools/sets").glob("*.json"):
            try:
                recipe = json.loads(spec.read_text(encoding="utf-8-sig"))
            except (OSError, ValueError):
                continue
            if recipe.get("name") == (name or document.parent.name):
                self.rerun.add("set:" + self.relative(spec))
                found = True
        if not found:
            self.rerun.add("manual:" + self.relative(document))

    def inspect_json(self, document):
        if document.is_symlink():
            self.issue("json_link", document, "JSON e link simbolico; copie o arquivo para este checkout", level="error")
            return
        if not document.resolve().is_relative_to(self.root):
            self.issue("external_json", document, "JSON fora do projeto; nao sera modificado", level="error")
            return
        self.json_files += 1
        try:
            original_bytes = document.read_bytes()
            contents = json.loads(original_bytes.decode("utf-8-sig"))
        except (OSError, ValueError) as exc:
            self.issue("json_invalid", document, str(exc), level="error")
            if document.name == "manifest.json":
                self.set_stage(document)
            else:
                self.invalidate("12" if document.is_relative_to(self.root / "unreal/NWPort/Data") else "11")
            return
        is_manifest = document.name == "manifest.json"
        cache_json = document.is_relative_to(self.root / "extract/ue")
        if is_manifest and (not isinstance(contents, dict) or not contents.get("fbx")):
            self.issue("export_missing", document, "manifesto ainda nao tem os FBX exportados", level="error")
            self.set_stage(document)
        changed = False

        def visit(value, trail=()):
            nonlocal changed
            if isinstance(value, dict):
                return {key: visit(item, (*trail, key)) for key, item in value.items()}
            if isinstance(value, list):
                return [visit(item, (*trail, str(index))) for index, item in enumerate(value)]
            if not isinstance(value, str) or value.startswith(("/Game/", "/Engine/")):
                return value
            text = normalized(value)
            known_absolute = absolute(text) and (cache_json or any(anchor in text for anchor in ANCHORS))
            relative_file = cache_json and Path(text).suffix.lower() in FILE_SUFFIXES and (
                not is_manifest or any(key in trail for key in ("glb", "fbx", "blend", "materials")))
            if not known_absolute and not relative_file:
                return value
            destination, status = self.candidate(value, document)
            field = ".".join(trail)
            if destination is None:
                # GLB/.blend servem para reexportar; FBX/texturas servem para importar.
                optional = "glb" in trail or "blend" in trail
                self.issue("path_" + status, document, f"{field}: {value}", level="warning" if optional else "error")
                if not optional:
                    if cache_json:
                        self.set_stage(document, contents.get("name") if isinstance(contents, dict) else None)
                    else:
                        self.invalidate("12")
                return value
            if Path(text).suffix.lower() == ".glb" and not complete_glb(destination):
                self.issue("glb_reference_invalid", document, f"{field}: GLB truncado em {destination}")
            replacement = destination.as_posix()
            if replacement != text:
                self.issue("path_rebase", document, f"{field}: {value} -> {replacement}", fixable=True)
                changed = True
                return replacement
            return value

        updated = visit(contents)
        if changed:
            self.add_action(document, "rewrite", data=(json.dumps(updated, ensure_ascii=False, indent=1) + "\n").encode("utf-8"),
                            before=("file", hashlib.sha256(original_bytes).hexdigest()))

    def inspect_txtp(self, document):
        if document.is_symlink() or not document.resolve().is_relative_to(self.root):
            self.issue("txtp_link", document, "TXT P e link ou esta fora do checkout; copie o arquivo")
            return
        original_bytes = document.read_bytes()
        text = original_bytes.decode("utf-8-sig")
        # Caminhos absolutos gerados pelo wwiser; preserva comentarios e diretivas.
        pattern = re.compile(r'(?P<path>(?:[A-Za-z]:[/\\]|/)[^\r\n#]*?\.(?:bnk|wem|txtp))(?=[\s"\']|$)', re.IGNORECASE)
        def replace(match):
            original = match.group("path")
            destination, status = self.candidate(original, document)
            if destination is None:
                self.issue("audio_path_" + status, document, original + "; OGGs prontos podem ser mantidos")
                return original
            if destination.as_posix() != normalized(original):
                self.issue("audio_rebase", document, original + " -> " + destination.as_posix(), fixable=True)
                return destination.as_posix()
            return original
        lines = []
        for line in text.splitlines(keepends=True):
            lines.append(line if line.lstrip().startswith("#") else pattern.sub(replace, line))
        updated = "".join(lines)
        if updated != text:
            self.add_action(document, "rewrite", data=updated.encode("utf-8"),
                            before=("file", hashlib.sha256(original_bytes).hexdigest()))

    def inspect_banks(self):
        sound = self.root / "extract/_raw/sounds/wwise"
        for source in sorted(sound.glob("*.bnk")):
            if not source.is_file() or source.stat().st_size == 0:
                continue
            destination = sound / "media" / source.name
            if destination.is_symlink() or not destination.exists():
                if not source.resolve().is_relative_to(self.root):
                    self.issue("bank_external", source, "banco original fora do projeto; nao sera copiado", level="error")
                    continue
                self.issue("bank_copy", destination, "substituir link/arquivo ausente por copia do banco existente", fixable=True)
                self.add_action(destination, "copy", source=source)

    def require(self, stage, path, *, glb=False, empty=False):
        if not (self.markers / (stage + ".ok")).exists():
            return
        valid = complete_glb(path) if glb else path.is_file() and (empty or path.stat().st_size > 0)
        if not valid:
            self.issue("output_missing", path, f"saida ausente/incompleta da etapa {stage}", level="error")
            self.invalidate(stage)
        elif glb:
            self.reusable_glbs += 1

    def require_glob(self, stage, folder, pattern):
        if (self.markers / (stage + ".ok")).exists() and not any(
                path.is_file() and path.stat().st_size > 0 for path in folder.rglob(pattern)):
            self.issue("output_group_missing", folder, f"sem {pattern} para a etapa {stage}", level="error")
            self.invalidate(stage)

    def mesh_outputs(self, stage, listing, output):
        if not (self.markers / (stage + ".ok")).exists():
            return
        try:
            meshes = mesh_paths(listing)
        except (OSError, ValueError) as exc:
            self.issue("mesh_list", listing, str(exc), level="error")
            self.invalidate(stage)
            return
        if not meshes:
            self.issue("mesh_list_empty", listing, "lista de malhas vazia", level="error")
            self.invalidate(stage)
        for mesh in meshes:
            self.require(stage, output / "limpa" / (Path(mesh).stem + ".glb"), glb=True)

    def inspect_markers(self):
        extract = self.root / "extract"
        for stage, relative, glb in (
            ("01", "extract/manifest/paths.txt", False),
            ("07", "extract/cities/limpa/settlement_southwindsward_complete.glb", True),
            ("07", "extract/cities/southwindsward_terrain.glb", True),
            ("07", "extract/cities/southwindsward_terrain.json", False),
            ("08", "extract/cities/guardaventos_terrain.glb", True),
            ("08", "extract/cities/guardaventos_water.glb", True),
            ("10", "extract/cities/chunks/windsward_capitals.glb", True),
            ("11", "extract/ue/sword_shield/manifest.json", False),
            ("11", "extract/ue/lost_sword/manifest.json", False),
            ("12", "unreal/NWPort/Data/World/worldmap.json", False),
            ("sistemas-01", "extract/cities/limpa/settlement_northwindsward_complete.glb", True),
            ("sistemas-01", "extract/cities/northwindsward_terrain.glb", True),
        ):
            self.require(stage, self.root / relative, glb=glb)
        self.require("01", extract / "ui/item_icons.txt", empty=True)
        for stage, folder, pattern in (
            ("02", "data", "*.datasheet.json"), ("02", "loc", "*.loc.xml"),
            ("03", "ui", "*.dynamicuicanvas.json"), ("04", "models", "*.adb"),
            ("05", "audio", "*.ogg"), ("06", "terrain", "region.heightmap"),
            ("sistemas-02", "slicejson", "settlement_northwindsward_complete.dynamicslice.json"),
            ("sistemas-03", "timelines", "*.timeline.json"),
        ):
            self.require_glob(stage, extract / folder, pattern)
        self.mesh_outputs("09", extract / "ue/gatherables/meshes.txt", extract / "ue/gatherables")
        self.mesh_outputs("13", self.root / "tools/arena/meshes.txt", extract / "ue/arena")
        if (self.markers / "00.ok").exists() and any(not (self.root / "bin" / name).is_file()
                for name in ("nwbt.exe", "dbgwater.exe", "ooz.dll", "texconv.exe")):
            self.invalidate("00")
        dll = self.root / "unreal/NWPort/Binaries/Win64/UnrealEditor-NWPort.dll"
        if (self.markers / "ue_01.ok").exists() and not (dll.is_file() and dll.stat().st_size > 0):
            self.issue("native_build", dll, "compilacao Windows do modulo necessaria; caches Linux nao substituem essa DLL")
            self.invalidate("ue_01")

    def scan(self):
        if not (self.root / "extract").is_dir():
            self.issue("no_extract", self.root / "extract", "nenhuma extracao para migrar; seguir setup novo")
        # As referencias podem apontar para bancos que serao copiados nesta aplicacao.
        self.inspect_banks()
        documents = set()
        for name in ("manifest.json", "materials.json", "lista.json"):
            documents.update((self.root / "extract/ue").rglob(name))
        documents.update((self.root / "unreal/NWPort/Data").rglob("*.json"))
        for document in sorted(documents):
            self.inspect_json(document)
        for document in sorted((self.root / "extract/_raw/txtp").rglob("*.txtp")):
            try:
                self.inspect_txtp(document)
            except (OSError, ValueError) as exc:
                self.issue("txtp_invalid", document, str(exc))
        self.inspect_markers()
        return self

    def report(self):
        return dict(root=str(self.root), old_roots=self.old_roots, json_checked=self.json_files,
                    reusable_glbs=self.reusable_glbs, issues=self.issues,
                    changes=[dict(file=self.relative(path), operation=action["kind"])
                             for path, action in sorted(self.actions.items())], rerun=sorted(self.rerun),
                    preserved_markers=[path.name for path in sorted(self.markers.glob("*.ok")) if path not in self.actions])

    def apply(self):
        if not self.actions:
            return None
        for path, action in self.actions.items():
            if fingerprint(path) != action["before"]:
                raise RuntimeError(f"arquivo mudou depois do check: {path}; execute novamente")
            if action["source"] and fingerprint(action["source"]) != action["source_before"]:
                raise RuntimeError(f"banco original mudou depois do check: {action['source']}; execute novamente")
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
        backup = self.root / "extract/.migration-backups" / stamp
        backup.mkdir(parents=True)
        self.backup = backup
        (backup / "plan.json").write_text(json.dumps(self.report(), ensure_ascii=False, indent=2), encoding="utf-8")
        for path, action in sorted(self.actions.items()):
            if fingerprint(path) != action["before"]:
                raise RuntimeError(f"arquivo mudou; backups em {backup}; execute novamente: {path}")
            if action["source"] and fingerprint(action["source"]) != action["source_before"]:
                raise RuntimeError(f"banco original mudou; backups em {backup}; execute novamente")
            original = backup / "files" / path.relative_to(self.root)
            if path.is_symlink():
                original.parent.mkdir(parents=True, exist_ok=True)
                # Preserva o proprio link, inclusive quando seu destino nao existe.
                path.replace(original)
            elif path.is_file():
                original.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, original)
            if action["kind"] == "invalidate":
                path.unlink(missing_ok=True)
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_name(path.name + ".migration-part")
            if action["kind"] == "copy":
                shutil.copy2(action["source"], temporary)
            else:
                temporary.write_bytes(action["data"])
            temporary.replace(path)
        return backup


def print_report(report):
    print(f"Projeto: {report['root']}")
    print(f"JSONs conferidos: {report['json_checked']}; GLBs reutilizaveis conferidos: {report['reusable_glbs']}")
    for issue in report["issues"]:
        label = "CORRIGIR" if issue["fixable"] else issue["level"].upper()
        print(f"{label}: {issue['file']}: {issue['detail']}")
    print(f"Alteracoes propostas: {len(report['changes'])}")
    for stage in report["rerun"]:
        if stage.startswith("ue_"):
            print(f"REFAZER: etapa Unreal {stage[3:]} (seguir o guia de teste)")
        elif stage.startswith("set:"):
            print("REFAZER: python tools/build_set.py " + stage[4:])
        elif stage.startswith("manual:"):
            print("REVISAR: conjunto/receita de " + stage[7:])
        else:
            print("REFAZER: python tools/build_all.py " + stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=nwplatform.ROOT, help="Checkout Windows com os arquivos copiados")
    parser.add_argument("--old-root", action="append", default=[], help="Caminho antigo WSL; pode repetir")
    parser.add_argument("--apply", action="store_true", help="Aplica correcoes locais e invalida marcadores indicados, com backup")
    parser.add_argument("--report", type=Path, help="Grava relatorio JSON; sem esta opcao o check nao escreve")
    args = parser.parse_args()
    try:
        if not args.root.is_dir():
            parser.error("--root precisa ser um diretorio existente")
        if any(not absolute(normalized(value)) or normalized(value).rstrip("/") == "" for value in args.old_root):
            parser.error("--old-root precisa ser o caminho absoluto do checkout antigo")
        audit = Audit(args.root, args.old_root).scan()
        destination = args.report.resolve() if args.report else None
        if destination:
            if destination.suffix.lower() != ".json" or destination in audit.actions or any(
                    destination.is_relative_to(audit.root / folder) for folder in
                    ("extract/ue", "extract/.etapas", "extract/.migration-backups", "unreal", "tools", "bin")):
                parser.error("use um relatorio .json separado de assets, manifestos, backups e ferramentas")
            if destination.exists():
                try:
                    previous_report = json.loads(destination.read_text(encoding="utf-8-sig"))
                except (OSError, ValueError):
                    parser.error("--report ja existe e nao e um relatorio desta ferramenta")
                if not isinstance(previous_report, dict) or not {"root", "json_checked", "changes"} <= previous_report.keys():
                    parser.error("--report ja existe e nao e um relatorio desta ferramenta")
        before = audit.report()
        print_report(before)
        backup = None
        if args.apply:
            with nwprocess.heavy_lock():
                backup = audit.apply()
            if backup:
                print(f"Backups: {backup}")
            audit = Audit(args.root, args.old_root).scan()
            print("\nDepois das correcoes:")
        report = audit.report()
        if args.apply:
            report["rerun"] = sorted(set(report["rerun"]) | set(before["rerun"]))
            report["before"] = before
            report["backup"] = str(backup) if backup else None
            print_report(report)
        if destination:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return 2 if audit.actions or report["rerun"] or any(issue["level"] == "error" for issue in audit.issues) else 0
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"check de migracao falhou: {exc}", file=sys.stderr)
        if "audit" in locals() and audit.backup:
            print(f"Backups da aplicacao: {audit.backup}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
