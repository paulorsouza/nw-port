#!/usr/bin/env bash
# Compila bin/libooz.so (descompressão Oodle/Kraken para o nwbt no Linux) de powzix/ooz + patches/ooz.patch
# (tools/build_ooz.py), as mesmas fontes que o tools/windows/build_ooz.ps1 usa para a ooz.dll. Entra na fila de
# trabalho pesado.
#
#     tools/linux/build_ooz.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$ROOT/bin"
cd "$(python3 "$ROOT/tools/build_ooz.py")"
"$ROOT/tools/linux/pesado.sh" g++ -O2 -fPIC -shared -w -o "$ROOT/bin/libooz.so.novo" kraken.cpp bitknit.cpp lzna.cpp
mv -f "$ROOT/bin/libooz.so.novo" "$ROOT/bin/libooz.so"
echo "bin/libooz.so pronto"
