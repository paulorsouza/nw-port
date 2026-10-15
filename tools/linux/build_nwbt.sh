#!/usr/bin/env bash
# Compila bin/nwbt e bin/dbgwater no Linux pela fila de trabalho pesado: commit base do fork + patches/nwbt.patch
# (tools/build_nwbt.py, o mesmo do Windows; patches/README.md). Argumentos vão para o build_nwbt.py (ex.: --test).
#
#     tools/linux/build_nwbt.sh [--test]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
source tools/linux/env.sh
exec tools/linux/pesado.sh python3 tools/build_nwbt.py "$@"
