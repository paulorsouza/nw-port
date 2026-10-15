#!/usr/bin/env bash
# Builds bin/libooz.so (Oodle/Kraken decompression for nwbt on Linux) from powzix/ooz + patches/ooz.patch
# (tools/build_ooz.py), the same sources tools/windows/build_ooz.ps1 uses for ooz.dll. Goes through the
# heavy-job queue.
#
#     tools/linux/build_ooz.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$ROOT/bin"
cd "$(python3 "$ROOT/tools/build_ooz.py")"
"$ROOT/tools/linux/heavy.sh" g++ -O2 -fPIC -shared -w -o "$ROOT/bin/libooz.so.new" kraken.cpp bitknit.cpp lzna.cpp
mv -f "$ROOT/bin/libooz.so.new" "$ROOT/bin/libooz.so"
echo "bin/libooz.so ready"
