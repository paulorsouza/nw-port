#!/usr/bin/env bash
# Captura de tela do port sem abrir o jogo (esteira 4): a Unreal sem janela desenha a tela num PNG com o código do jogo
# (NWUIShotCommandlet), a partir de um cenário (bolsa, níveis, estação, receita, tamanho da janela).
#
#     tools/linux/ui_shot.sh tools/ui_shots/refining.json [captura]   ->  extract/ui/shots/refining_<captura>.png
#
# Precisa do módulo compilado (tools/linux/ue_import_all.sh 01). Roda na fila de trabalho pesado.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
[ -n "${NW_PESADO:-}" ] || exec "$ROOT/tools/linux/pesado.sh" "$ROOT/tools/linux/$(basename "$0")" "$@"
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
grep -aq "NWUIShot: [1-9][0-9]* capturas" "$LOG" || { echo "falhou, ver $LOG" >&2; exit 1; }
