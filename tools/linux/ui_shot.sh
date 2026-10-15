#!/usr/bin/env bash
# Screenshot of the port without opening the game (area 4): windowless Unreal draws the screen to a PNG with the game
# code (NWUIShotCommandlet), from a scenario (bag, levels, station, recipe, window size).
#
#     tools/linux/ui_shot.sh tools/ui_shots/refining.json [shot]   ->  extract/ui/shots/refining_<shot>.png
#
# Needs the compiled module (tools/linux/ue_import_all.sh 01). Runs in the heavy-job queue.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# NW_PESADO: old name, still accepted.
[ -n "${NW_HEAVY:-}${NW_PESADO:-}" ] || exec "$ROOT/tools/linux/heavy.sh" "$ROOT/tools/linux/$(basename "$0")" "$@"
UE_DIR="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}"
P="$ROOT/unreal/NWPort/NWPort.uproject"
SC="$(realpath "$1")"
OUT="$ROOT/extract/ui/shots"
LOG="$ROOT/unreal/NWPort/Saved/ImportLogs/ui_shot.log"
mkdir -p "$OUT" "$(dirname "$LOG")"
ARGS=(-run=NWUIShot "-scenario=$SC" "-out=$OUT" -nwlang="${NW_LANG:-pt-br}" -AllowCommandletRendering -RenderOffscreen
      -unattended -nosplash -nosound)
[ -n "${2:-}" ] && ARGS+=("-shot=$2")
# shellcheck disable=SC2206
ARGS+=(${NW_SHOT_ARGS:-})
nice "$UE_DIR/Engine/Binaries/Linux/UnrealEditor-Cmd" "$P" "${ARGS[@]}" > "$LOG" 2>&1
grep -a "NWUIShot\|LogNWUIShot: Error" "$LOG" | sed 's/.*NWUIShot: //'
grep -aq "NWUIShot: [1-9][0-9]* capturas" "$LOG" || { echo "failed, see $LOG" >&2; exit 1; }
