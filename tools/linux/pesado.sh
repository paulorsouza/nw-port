#!/usr/bin/env bash
# Roda um trabalho pesado na fila: um por vez na máquina, mesmo com várias sessões trabalhando juntas
# (docs/esteiras.md). Pesado = nwbt, Blender, build_set.py, UnrealEditor-Cmd, compilação, Ghidra em lote.
#
#     tools/linux/pesado.sh <comando> [args...]
#
# build_all.sh e ue_import_all.sh já entram na fila sozinhos. Dentro da fila (NW_PESADO=1) não espera de novo.
set -euo pipefail
LOCK="${XDG_RUNTIME_DIR:-/tmp}/nw-pesado.lock"
if [ -n "${NW_PESADO:-}" ]; then
    exec "$@"
fi
exec 9>"$LOCK"
if ! flock -n 9; then
    echo "[pesado] esperando outro trabalho pesado terminar ($LOCK)..." >&2
    flock 9
fi
export NW_PESADO=1
"$@"
