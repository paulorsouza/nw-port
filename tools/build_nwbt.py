"""Builds nwbt and dbgwater (Linux and Windows) from the fork + patches/nwbt.patch, and checks the binary.

    python3 tools/pesado.py python3 tools/build_nwbt.py [--source DIR] [--go GO] [--test]
    python3 tools/build_nwbt.py --conferir        # was bin/nwbt built with this checkout's patch?
    python3 tools/build_nwbt.py --teste-lista     # converts a game cube via "@lista" (loads the paks)
    python3 tools/build_nwbt.py --versao          # version the binary must have

On Windows use "python" instead of "python3". The fork checkout (vendor/nw-buddy or NW_BUDDY) is not changed:
commit BASE goes to .nwbt/build/src with LF line endings, the patch is applied there and the binary gets the version
("nwbt --version" shows "nwport-patch=<base>-<patch hash>"). nwbt changed: edit in the fork, redo the patch
(patches/README.md) and rebuild; check.sh and doctor.py flag a binary from another patch.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

import nwplatform

ROOT = nwplatform.ROOT
PATCH = ROOT / "patches/nwbt.patch"
# Commit of the paulorsouza/nw-buddy fork (branch nw-port) the patch applies on.
BASE = "5e2c7e195f05975b73e4dbc3399d60a815133ed8"
FORK = "https://github.com/paulorsouza/nw-buddy.git"
BUILD = ROOT / ".nwbt/build"
# Minimal game mesh for the "@lista" test.
TEST_MESH = "engineassets/objects/default/primitive_cube.cgf"


def patch_text():
    # Normalizes CRLF: a Windows checkout with autocrlf must give the same version.
    return PATCH.read_bytes().replace(b"\r\n", b"\n")


def expected_version():
    return f"{BASE[:12]}-{hashlib.sha256(patch_text()).hexdigest()[:12]}"


def binary_version(exe):
    try:
        out = subprocess.run([exe, "--version"], capture_output=True, text=True, timeout=60,
                             env=nwplatform.environment()).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None
    m = re.search(r"nwport-patch=(\S+)", out)
    return m.group(1) if m else ""


def check_binary(exe=None):
    """(ok, message) for the installed nwbt against the checkout's patch."""
    exe = exe or nwplatform.executable("nwbt")
    found = binary_version(exe)
    want = expected_version()
    if found is None:
        return False, f"{exe} does not run; build it with tools/build_nwbt.py"
    if found == want:
        return True, f"{exe} has the current patch ({want})"
    if found == "":
        return False, f"{exe} has no patch version (built before 2026-10-15 or outside build_nwbt.py); rebuild"
    return False, f"{exe} out of date: patch {found}, checkout wants {want}; rebuild with tools/build_nwbt.py"


def test_argfile(exe=None, game=None):
    """Converts TEST_MESH passing the list via "@file"; fails if nothing is written."""
    exe = exe or nwplatform.executable("nwbt")
    game = game or nwplatform.game_directory()
    if not (Path(game) / "assets").is_dir():
        return None, f"game not found at {game}: @lista test skipped"
    work = ROOT / ".nwbt/check"
    shutil.rmtree(work, ignore_errors=True)
    out = work / "glb"
    out.mkdir(parents=True)
    lista = work / "lista.txt"
    lista.write_text(TEST_MESH + "\n", encoding="utf-8", newline="\n")
    start = time.time()
    r = subprocess.run([exe, "models", "files", "-g", game, "-o", str(out), "--binary", "-w", "1", "@" + str(lista)],
                       capture_output=True, text=True, cwd=ROOT, env=nwplatform.environment())
    glbs = [p for p in out.rglob("*.glb") if p.stat().st_size > 0 and p.stat().st_mtime >= start - 1]
    if r.returncode != 0:
        return False, f"nwbt models files @lista exited with {r.returncode}: {(r.stderr or r.stdout).strip()[-300:]}"
    if not glbs:
        return False, "nwbt models files @lista exited 0 without writing anything (binary without @lista support?)"
    return True, f"nwbt converts via @lista ({glbs[0].name})"


def go_executable(explicit):
    if explicit:
        return explicit
    if os.environ.get("NW_GO"):
        return os.environ["NW_GO"]
    found = shutil.which("go")
    if found:
        return found
    local = Path.home() / ".local/go/bin" / ("go.exe" if nwplatform.WINDOWS else "go")
    if local.is_file():
        return str(local)
    raise SystemExit("Go not found: install it (docs/tech/tools.md) or use --go / NW_GO")


def git(*args, cwd=None, capture=False):
    r = subprocess.run(["git", *args], cwd=cwd, capture_output=capture)
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} failed")
    return r.stdout


def patch_header():
    text = patch_text().decode("utf-8")
    return text[:text.find("diff --git")] if "diff --git" in text else text


def src_diff(src):
    git("add", "-N", ".", cwd=src)
    return git("diff", cwd=src, capture=True).replace(b"\r\n", b"\n")


def write_patch():
    """Writes patches/nwbt.patch from .nwbt/build/src (the current patch header is kept)."""
    src = BUILD / "src"
    if not (src / ".git").is_dir():
        raise SystemExit("no .nwbt/build/src: run the build once (or --preparar)")
    PATCH.write_bytes(patch_header().encode("utf-8") + src_diff(src))
    print(f"{PATCH.relative_to(ROOT)}: {expected_version()}")


def prepare_source(source):
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        git("clone", "--branch", "nw-port", FORK, str(source))
    if subprocess.run(["git", "-C", str(source), "cat-file", "-e", BASE + "^{commit}"],
                      capture_output=True).returncode != 0:
        git("-C", str(source), "fetch", "origin", "nw-port")
    src = BUILD / "src"
    if (src / ".git").is_dir() and src_diff(src) != patch_text()[len(patch_header().encode("utf-8")):]:
        raise SystemExit(f"{src} has changes outside patches/nwbt.patch: write the patch (--gerar-patch) "
                         "or delete the folder")
    shutil.rmtree(src, ignore_errors=True)
    src.mkdir(parents=True)
    archive = git("-C", str(source), "archive", "--format=tar", BASE, "tools/nwbt", capture=True)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(src, filter="data")
    for path in (src / "tools/nwbt").rglob("*"):
        if path.is_file() and path.suffix in (".go", ".mod", ".sum"):
            data = path.read_bytes()
            if b"\r\n" in data:
                path.write_bytes(data.replace(b"\r\n", b"\n"))
    # Own repository: git apply works relative to src and "git diff" shows the applied patch.
    git("init", "-q", cwd=src)
    git("add", "-A", cwd=src)
    git("-c", "user.name=nw-port", "-c", "user.email=nw-port@localhost", "commit", "-qm", "base " + BASE, cwd=src)
    with tempfile.NamedTemporaryFile("wb", suffix=".patch", delete=False) as f:
        f.write(patch_text())
    try:
        git("apply", "--whitespace=nowarn", f.name, cwd=src)
    finally:
        os.unlink(f.name)
    return src / "tools/nwbt"


def build(args):
    source = Path(args.source or os.environ.get("NW_BUDDY") or ROOT / "vendor/nw-buddy").resolve()
    go = go_executable(args.go)
    module = prepare_source(source)
    version = expected_version()
    ldflags = f"-X nw-buddy/tools/constants.NWPortPatch={version}"
    suffix = ".exe" if nwplatform.WINDOWS else ""
    env = dict(os.environ)
    for name, pkg in (("nwbt", "."), ("dbgwater", "./dbgwater")):
        target = nwplatform.BIN / (name + suffix)
        tmp = BUILD / (name + suffix)
        r = subprocess.run([go, "build", "-ldflags", ldflags, "-o", str(tmp), pkg], cwd=module, env=env)
        if r.returncode != 0:
            raise SystemExit(f"go build {name} failed")
        # Atomic swap: whoever is running the old binary keeps it (on Windows, fails if it is open).
        os.replace(tmp, target)
        print(f"{target.relative_to(ROOT)}: {version}")
    if args.test:
        tests = "TestResolveDataFile|TestAppendToPATH"
        if nwplatform.WINDOWS:
            dll = nwplatform.BIN / "ooz.dll"
            if not dll.is_file():
                raise SystemExit("Build ooz.dll before using --test (tools/windows/build_ooz.ps1)")
            env["NWBT_TEST_OOZ"] = str(dll)
            tests += "|TestOodleWindowsOverride|TestOozWindowsStoredBlock"
        r = subprocess.run([go, "test", "./utils", "./utils/env", "-run", tests, "-v"], cwd=module, env=env)
        if r.returncode != 0:
            raise SystemExit("nwbt tests failed")
    ok, msg = check_binary(str(nwplatform.BIN / ("nwbt" + suffix)))
    print(("ok    " if ok else "FAIL  ") + msg)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", help="fork checkout (default NW_BUDDY or vendor/nw-buddy)")
    ap.add_argument("--go", help="Go executable (default NW_GO, go in PATH or ~/.local/go)")
    ap.add_argument("--test", action="store_true", help="run the patch's Go tests after building")
    ap.add_argument("--conferir", action="store_true", help="only check the bin/nwbt version")
    ap.add_argument("--teste-lista", action="store_true", help="only test conversion via @lista (needs the game)")
    ap.add_argument("--versao", action="store_true", help="only show the expected version")
    ap.add_argument("--preparar", action="store_true", help="only set up .nwbt/build/src (base + patch) for editing")
    ap.add_argument("--gerar-patch", action="store_true", help="write patches/nwbt.patch from .nwbt/build/src")
    args = ap.parse_args()
    if args.gerar_patch:
        write_patch()
        return 0
    if args.preparar:
        print(prepare_source(Path(args.source or os.environ.get("NW_BUDDY") or ROOT / "vendor/nw-buddy").resolve()))
        return 0
    if args.versao:
        print(expected_version())
        return 0
    if args.conferir or args.teste_lista:
        code = 0
        if args.conferir:
            ok, msg = check_binary()
            print(("ok    " if ok else "FAIL  ") + msg)
            code |= not ok
        if args.teste_lista:
            ok, msg = test_argfile()
            print({True: "ok    ", False: "FAIL  ", None: "warn  "}[ok] + msg)
            code |= ok is False
        return int(code)
    return build(args)


if __name__ == "__main__":
    sys.exit(main())
