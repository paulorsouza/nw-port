#!/usr/bin/env python3
"""ooz sources (Oodle/Kraken decompression for nwbt): powzix/ooz at commit BASE + patches/ooz.patch.

    python3 tools/build_ooz.py      # prints the folder with the prepared sources (.nwbt/build/ooz)

The checkout in vendor/ooz (cloned if missing) is not changed. To build: tools/linux/build_ooz.sh or
tools/windows/build_ooz.ps1. ooz is GPL-3.0; the built binary stays out of git.
"""
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nwplatform  # noqa: E402

UPSTREAM = "https://github.com/powzix/ooz.git"
BASE = "05038060aa68f9187ae9923b2388ca8db40e58d1"
SOURCES = ["kraken.cpp", "bitknit.cpp", "lzna.cpp", "stdafx.h"]
ROOT = nwplatform.ROOT


def git(*args, cwd=None):
    subprocess.run(["git", *args], cwd=cwd, check=True)


def prepare():
    checkout = ROOT / "vendor/ooz"
    if not (checkout / ".git").is_dir():
        git("clone", "--quiet", UPSTREAM, str(checkout))
    if subprocess.run(["git", "cat-file", "-e", BASE + "^{commit}"], cwd=checkout).returncode != 0:
        git("fetch", "--quiet", "origin", cwd=checkout)
    out = ROOT / ".nwbt/build/ooz"
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    for name in SOURCES:
        data = subprocess.run(["git", "show", f"{BASE}:{name}"], cwd=checkout, check=True,
                              capture_output=True).stdout
        (out / name).write_bytes(data.replace(b"\r\n", b"\n"))
    # Run from the root: inside the nw-port checkout, git apply resolves the paths from it.
    git("apply", f"--directory={out.relative_to(ROOT).as_posix()}", "patches/ooz.patch", cwd=ROOT)
    return out


if __name__ == "__main__":
    print(prepare())
