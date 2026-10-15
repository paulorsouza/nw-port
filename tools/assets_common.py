"""Partes comuns de tools/assets_export.py e tools/assets_import.py (docs/sincronizar-assets.md).

Os caminhos dos arquivos são relativos a unreal/NWPort (Content/..., Data/...), com barra normal.
"""
import datetime
import hashlib
import json
import os
import platform
import re
import shutil
import socket
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UPROJ = ROOT / "unreal" / "NWPort"
SAVED = UPROJ / "Saved"
EXTRACT = Path(os.environ.get("NW_EXTRACT") or ROOT / "extract")
ETAPAS = EXTRACT / ".etapas"
STATE = SAVED / "assets_state.json"
HASHCACHE = SAVED / "assets_hashcache.json"
BACKUP = SAVED / "assets_backup"
# Pasta que espelha a do OneDrive (upload e download à mão).
DEFAULT_DIR = Path(os.environ.get("NW_ASSETS") or Path.home() / "nw-port-assets")
SCOPES = ("Content", "Data")
BUNDLE_LIMIT = int(os.environ.get("NW_BUNDLE_LIMIT", 1 << 30))  # pasta acima disso vira um zip por subpasta
KEEP_BACKUP = 3
WINDOWS = platform.system() == "Windows"
JUNK = {"desktop.ini", "Thumbs.db", ".DS_Store"}

# Nomes que o Windows ou o OneDrive não aceitam. O caminho é medido a partir de C:\nw-port\unreal\NWPort\.
WIN_PREFIX = len("C:\\nw-port\\unreal\\NWPort\\")
MAX_PATH = 240
BAD_CHARS = re.compile(r'[<>:"|?*\\\x00-\x1f]')
RESERVED = {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)), *(f"LPT{i}" for i in range(1, 10))}


def die(msg, code=1):
    print(msg, file=sys.stderr)
    sys.exit(code)


def now():
    return datetime.datetime.now().astimezone().isoformat(timespec="seconds")


def stamp():
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def machine():
    return f"{socket.gethostname()}-{platform.system().lower()}"


def git(*args, check=True):
    r = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, encoding="utf-8")
    if check and r.returncode != 0:
        die(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r


def ue_version():
    if os.environ.get("NW_UE"):
        cands = [Path(os.environ["NW_UE"])]
    elif WINDOWS:
        cands = sorted(Path(r"C:\Program Files\Epic Games").glob("UE_5.*"), reverse=True)
    else:
        cands = [Path("/mnt/jogos/UnrealEngine/5.8")]
    for d in cands:
        f = d / "Engine" / "Build" / "Build.version"
        if f.exists():
            v = json.loads(f.read_text(encoding="utf-8"))
            return f"{v['MajorVersion']}.{v['MinorVersion']}.{v['PatchVersion']}"
    die("Unreal não encontrada: aponte NW_UE para a pasta da engine (a que tem Engine/Build/Build.version).")


def game_build():
    for line in (ROOT / "tools" / "jogo.versao").read_text(encoding="utf-8").splitlines():
        p = line.split()
        if len(p) == 2 and p[0] == "buildid":
            return p[1]
    return None


def editor_open():
    if WINDOWS:
        out = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
        return any(n in out for n in ('"UnrealEditor.exe"', '"UnrealEditor-Cmd.exe"'))
    return subprocess.run(["pgrep", "-f", r"UnrealEditor(-Cmd)? .*NWPort\.uproject"], capture_output=True).returncode == 0


_lock = None


def preflight():
    """Editor fechado; no Linux entra na fila do tools/pesado.sh e fica com ela até o fim."""
    global _lock
    if editor_open():
        die("O editor está aberto: feche antes.", 3)
    if WINDOWS or os.environ.get("NW_PESADO"):
        return
    import fcntl
    _lock = open(Path(os.environ.get("XDG_RUNTIME_DIR", "/tmp")) / "nw-pesado.lock", "w")
    try:
        fcntl.flock(_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        die("Tem trabalho pesado rodando (tools/pesado.sh): espere terminar.", 3)


def read_json(path, default):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except FileNotFoundError:
        return default


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(tmp, path)


def read_state():
    return read_json(STATE, {"generation": 0, "id": None})


def write_state(m):
    """Guarda qual publicação esta máquina tem (a que ela publicou ou baixou por último)."""
    write_json(STATE, {"generation": m["generation"], "id": m["id"], "machine": machine(), "at": now()})


def git_tracked():
    out = git("ls-files", "-z", "--", "unreal/NWPort/Content", "unreal/NWPort/Data").stdout
    return {p[len("unreal/NWPort/"):] for p in out.split("\0") if p}


def scan():
    """{caminho: (tamanho, mtime_ns)} de Content e Data, sem o que está no git."""
    tracked = git_tracked()
    files = {}
    for scope in SCOPES:
        base = UPROJ / scope
        if not base.exists():
            continue
        for dirpath, _, names in os.walk(base):
            for n in names:
                if n in JUNK:
                    continue
                p = Path(dirpath) / n
                rel = p.relative_to(UPROJ).as_posix()
                if rel in tracked:
                    continue
                st = p.stat()
                files[rel] = (st.st_size, st.st_mtime_ns)
    return files


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


class Hasher:
    """sha256 dos arquivos, guardado por (tamanho, mtime): só arquivo mexido é lido de novo."""

    def __init__(self):
        self.cache = read_json(HASHCACHE, {})
        self.dirty = False

    def sha(self, rel, size, mtime):
        c = self.cache.get(rel)
        if c and c[0] == size and c[1] == mtime:
            return c[2]
        h = sha256_file(UPROJ / rel)
        self.cache[rel] = [size, mtime, h]
        self.dirty = True
        return h

    def save(self, files):
        self.cache = {k: v for k, v in self.cache.items() if k in files}
        write_json(HASHCACHE, self.cache)


def plan_bundles(files):
    """Divide em zips: pasta até 1 GiB (ou sem subpasta) é um zip inteiro ("deep"); maior, um zip por subpasta e um
    só com os arquivos soltos dela ("deep": false). Devolve {pasta: deep}."""
    size, subdirs, direct = {}, {}, {}
    for rel, (sz, _) in files.items():
        parts = rel.split("/")
        direct.setdefault("/".join(parts[:-1]), []).append(rel)
        for i in range(1, len(parts)):
            d = "/".join(parts[:i])
            size[d] = size.get(d, 0) + sz
            if i > 1:
                subdirs.setdefault("/".join(parts[:i - 1]), set()).add(d)
    bundles = {}

    def visit(d):
        if size[d] <= BUNDLE_LIMIT or not subdirs.get(d):
            bundles[d] = True
            return
        if direct.get(d):
            bundles[d] = False
        for c in sorted(subdirs[d]):
            visit(c)

    for scope in SCOPES:
        if scope in size:
            visit(scope)
    return bundles


def bundle_of(rel, bundles):
    parent = rel.rsplit("/", 1)[0]
    if bundles.get(parent) is False:
        return parent
    d = parent
    while True:
        if bundles.get(d) is True:
            return d
        if "/" not in d:
            return None
        d = d.rsplit("/", 1)[0]


def group(files, bundles):
    """{pasta do zip: [caminhos]} e a lista dos arquivos que não cabem em zip nenhum."""
    out, orphans = {b: [] for b in bundles}, []
    for rel in files:
        b = bundle_of(rel, bundles)
        (out[b] if b is not None else orphans).append(rel)
    return out, orphans


def fingerprint(rels, files, hasher):
    h = hashlib.sha256()
    for rel in sorted(rels):
        sz, mt = files[rel]
        h.update(f"{rel}\t{sz}\t{hasher.sha(rel, sz, mt)}\n".encode())
    return h.hexdigest()


def zip_name(path, deep, fp):
    return path.replace("/", "__") + ("" if deep else "__soltos") + f"-{fp[:12]}.zip"


def check_names(rels):
    errors, seen = [], {}
    for rel in rels:
        if WIN_PREFIX + len(rel) > MAX_PATH:
            errors.append(f"caminho longo demais no Windows ({WIN_PREFIX + len(rel)}): {rel}")
        for part in rel.split("/"):
            if BAD_CHARS.search(part) or part.endswith((" ", ".")) or part.split(".")[0].upper() in RESERVED \
                    or part.startswith("~$") or "_vti_" in part or part == ".lock":
                errors.append(f"nome inválido no Windows/OneDrive: {rel}")
                break
        low = rel.lower()
        if low in seen and seen[low] != rel:
            errors.append(f"só diferem por maiúscula/minúscula: {seen[low]} × {rel}")
        seen[low] = rel
    if errors:
        print("\n".join(errors[:30]), file=sys.stderr)
        die(f"{len(errors)} nome(s) não vão funcionar no Windows/OneDrive: corrija antes.")


def etapas_markers():
    return sorted(p.name for p in ETAPAS.glob("ue_*.ok")) if ETAPAS.exists() else None


def ask(question, yes):
    if yes:
        return
    if not sys.stdin.isatty():
        die("Sem terminal para confirmar: use --yes.")
    if input(f"{question} [s/N] ").strip().lower() not in ("s", "sim", "y", "yes"):
        die("Cancelado.")


def remove_empty_dirs(rels):
    for d in sorted({str(Path(r).parent) for r in rels}, key=len, reverse=True):
        p = UPROJ / d
        while p != UPROJ and p.name not in SCOPES:
            try:
                p.rmdir()
            except OSError:
                break
            p = p.parent


def prune_backups():
    if BACKUP.exists():
        for d in sorted(BACKUP.iterdir())[:-KEEP_BACKUP]:
            shutil.rmtree(d, ignore_errors=True)
