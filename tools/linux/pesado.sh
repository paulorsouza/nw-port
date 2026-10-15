#!/usr/bin/env bash
# Runs a heavy job in the queue: one at a time on the machine, even with several sessions working together
# (docs/workflow.md). Heavy = nwbt, Blender, build_set.py, UnrealEditor-Cmd, compilation, batch Ghidra.
#
#     tools/linux/pesado.sh <comando> [args...]
#
# build_all.sh and ue_import_all.sh enter the queue on their own. Inside the queue (NW_PESADO=1) it does not wait again.
set -euo pipefail
LOCK="${XDG_RUNTIME_DIR:-/tmp}/nw-pesado.lock"
if [ -n "${NW_PESADO:-}" ]; then
    exec "$@"
fi
exec 9>"$LOCK"
if ! flock -n 9; then
    echo "[pesado] waiting for another heavy job to finish ($LOCK)..." >&2
    flock 9
fi
export NW_PESADO=1
"$@"
