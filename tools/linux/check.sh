#!/usr/bin/env bash
# Check before merging a branch (docs/workflow.md, "Several people"). Light, no Unreal; with --cpp it also
# compiles the C++ (game target, works with the editor open; goes through the heavy-job queue).
#
#     tools/linux/check.sh [--cpp]
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
FAIL=0
ok() { echo "ok    $*"; }
bad() { echo "FAIL  $*"; FAIL=1; }

# Leftover conflict markers.
if git grep -nE '^(<<<<<<<|>>>>>>>) ' -- . ':!*.uasset' ':!*.umap' >/dev/null 2>&1; then
    bad "conflict markers:"; git grep -nE '^(<<<<<<<|>>>>>>>) ' -- . ':!*.uasset' ':!*.umap' | head
else
    ok "no conflict markers"
fi

# Hand-written JSON.
bad_json=$(git ls-files '*.json' | while read -r f; do python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$f" 2>/dev/null || echo "$f"; done)
[ -z "$bad_json" ] && ok "JSON valid" || bad "invalid JSON: $bad_json"

# Parts that get merged: sets, texts, RE values (a key repeated across areas is an error).
python3 - <<'PY' && ok "sets, texts and RE values without repeated keys" || bad "repeated key (above)"
import glob, os, sys
sys.path.insert(0, "tools")
import setspec, make_rules
from build_strings import load_strings
for f in sorted(glob.glob("tools/sets/*.json")):
    setspec.load(f)
load_strings()
make_rules.load_values()
PY

# Python and shell.
py_err=$(python3 -m py_compile tools/*.py tools/ue/*.py tools/re/*.py 2>&1) && ok "Python compiles" || bad "Python: $py_err"
sh_err=""; for f in tools/*.sh tools/linux/*.sh tools/etapas/*.sh; do [ -e "$f" ] && { bash -n "$f" 2>&1 || sh_err="$sh_err $f"; }; done
[ -z "$sh_err" ] && ok "shell without syntax errors" || bad "shell:$sh_err"

# nwbt built with this checkout's patch (patches/nwbt.patch) and converting via "@lista" (game cube, ~20 s in the
# heavy-job queue; without the game installed it becomes a warning). Rebuild: tools/linux/pesado.sh python3 tools/build_nwbt.py
python3 tools/build_nwbt.py --conferir || FAIL=1
tools/linux/pesado.sh python3 tools/build_nwbt.py --teste-lista || FAIL=1

# Game version (warning only: someone without the game installed can still review).
NW_IGNORA_VERSAO=1 tools/linux/jogo_versao.sh 2>&1 | sed 's/^/warn  /'

# What from the RE has not made it into the port yet (informational).
python3 tools/re_check.py 2>/dev/null | grep -E "^== (Valores|Perguntas)" | sed 's/^== /info  /'

if [ "${1:-}" = "--cpp" ]; then
    UE_DIR="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}"
    LOG="$ROOT/unreal/NWPort/Saved/check_build.log"
    mkdir -p "$(dirname "$LOG")"
    if tools/linux/pesado.sh bash -c "cd '$UE_DIR' && nice Engine/Build/BatchFiles/Linux/Build.sh NWPort Linux Development \
            -Project='$ROOT/unreal/NWPort/NWPort.uproject' -WaitMutex -MaxParallelActions=6" > "$LOG" 2>&1; then
        ok "C++ compiles (game target)"
    else
        bad "C++ (see $LOG):"; grep -E "error" "$LOG" | head
    fi
fi

[ "$FAIL" = 0 ] && echo "all good" || echo "fix the failures before merging"
exit "$FAIL"
