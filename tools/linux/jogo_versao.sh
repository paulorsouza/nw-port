#!/usr/bin/env bash
# Checks that the New World install is the team's build (tools/jogo.versao).
#
#     tools/linux/jogo_versao.sh [--exe]     # --exe also checks the sha256 of NewWorld.exe (reverse engineering)
#
# Exits with an error if the build differs. NW_IGNORA_VERSAO=1 only warns (e.g. checking before the whole team updates).
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${NW_GAME:-/mnt/jogos/Steam/steamapps/common/New World}"
want_build=$(awk '$1 == "buildid" {print $2}' "$ROOT/tools/jogo.versao")
want_sha=$(awk '$1 == "exe_sha256" {print $2}' "$ROOT/tools/jogo.versao")
fail() {
    echo "game version: $*" >&2
    [ -n "${NW_IGNORA_VERSAO:-}" ] && return 0
    echo "Use the team's build (tools/jogo.versao) or coordinate the update with everyone." >&2
    exit 4
}
acf="$GAME/../../appmanifest_1063730.acf"
if [ -f "$acf" ]; then
    have=$(awk -F'"' '$2 == "buildid" {print $4}' "$acf")
    [ "$have" = "$want_build" ] || fail "build $have installed, team uses $want_build"
else
    echo "game version: no $acf; checking only by the executable" >&2
    set -- --exe
fi
if [ "${1:-}" = "--exe" ]; then
    have=$(sha256sum "$GAME/Bin64/NewWorld.exe" | cut -c1-64)
    [ "$have" = "$want_sha" ] || fail "NewWorld.exe has sha256 $have, team uses $want_sha"
fi
exit 0
