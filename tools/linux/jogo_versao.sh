#!/usr/bin/env bash
# Confere se a instalação do New World é a build do time (tools/jogo.versao).
#
#     tools/linux/jogo_versao.sh [--exe]     # --exe também confere o sha256 do NewWorld.exe (engenharia reversa)
#
# Sai com erro se a build for outra. NW_IGNORA_VERSAO=1 só avisa (ex.: conferir antes de atualizar o time todo).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
want_build=$(awk '$1 == "buildid" {print $2}' "$ROOT/tools/jogo.versao")
want_sha=$(awk '$1 == "exe_sha256" {print $2}' "$ROOT/tools/jogo.versao")
fail() {
    echo "versão do jogo: $*" >&2
    [ -n "${NW_IGNORA_VERSAO:-}" ] && return 0
    echo "Use a build do time (tools/jogo.versao) ou combine a atualização com todos." >&2
    exit 4
}
acf="$GAME/../../appmanifest_1063730.acf"
if [ -f "$acf" ]; then
    have=$(awk -F'"' '$2 == "buildid" {print $4}' "$acf")
    [ "$have" = "$want_build" ] || fail "build $have instalada, time usa $want_build"
else
    echo "versão do jogo: sem $acf; conferindo só pelo executável" >&2
    set -- --exe
fi
if [ "${1:-}" = "--exe" ]; then
    have=$(sha256sum "$GAME/Bin64/NewWorld.exe" | cut -c1-64)
    [ "$have" = "$want_sha" ] || fail "NewWorld.exe com sha256 $have, time usa $want_sha"
fi
exit 0
