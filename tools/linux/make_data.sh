#!/usr/bin/env bash
# Regenera unreal/NWPort/Data/ (catálogo, saque, coletáveis, textos, telas) a partir do que já foi extraído em
# extract/ (docs/SETUP.md, etapa 2). Leve: só Python, sem Unreal. Pode rodar de novo.
# A lista de passos fica só em tools/make_data.py (a mesma no Windows); passo novo entra lá.
#
#     tools/linux/make_data.sh [--dry-run]
set -euo pipefail
cd "$(dirname "$0")/../.."
exec python3 tools/make_data.py "$@"
