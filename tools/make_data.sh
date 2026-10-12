#!/usr/bin/env bash
# Regenera unreal/NWPort/Data/ (catálogo, saque, coletáveis, textos, telas) a partir do que já foi extraído em
# extract/ (docs/SETUP.md, etapa 2). Leve: só Python, sem Unreal. Pode rodar de novo.
#
#     tools/make_data.sh
set -euo pipefail
cd "$(dirname "$0")/.."

step() { echo "== $*"; python3 "$@"; }
# Ordem importa: o catálogo de itens lê os coletáveis (saque); os textos leem os coletáveis; o inventário lê os itens.
step tools/make_gatherables.py
step tools/make_items.py
step tools/make_abilities.py
step tools/build_strings.py
step tools/ui_build_hud.py
step tools/ui_build_mastery.py
step tools/ui_build_login.py
step tools/ui_build_respawn.py
step tools/ui_build_group.py
step tools/ui_build_inventory.py
step tools/ui_textures_extra.py
echo "Data/ pronto: $(find unreal/NWPort/Data -type f | wc -l) arquivos"
