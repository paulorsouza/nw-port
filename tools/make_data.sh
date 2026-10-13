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
step tools/make_spawns.py
# Fatos da engenharia reversa (docs/analise-re/valores/) lidos pelo C++ (FNWRules).
step tools/make_rules.py
# IA das criaturas (árvores do Server.pak); pula se o Server.pak não foi extraído (docs/analise-re/ferramentas.md).
[ -d extract/serverpak/json/sharedassets/ai ] && step tools/make_ai.py || echo "== sem extract/serverpak: Data/AI fica como está"
# Esteira 4: estações e receitas de refino de Queda Eterna (o catálogo inclui os itens das receitas).
[ -f extract/slicejson/slices/structures/settlements/v2_settlements/settlement_northwindsward_complete.dynamicslice.json ] \
    && step tools/make_crafting.py || echo "== sem o slice de Queda Eterna: Data/Crafting fica como está"
step tools/make_items.py
step tools/make_abilities.py
step tools/make_progression.py
step tools/make_worldmap.py
step tools/build_strings.py
step tools/ui_build_hud.py
step tools/ui_build_mastery.py
step tools/ui_build_tradeskills.py
step tools/ui_build_attributes.py
step tools/ui_build_login.py
step tools/ui_build_respawn.py
step tools/ui_build_group.py
step tools/ui_build_inventory.py
# Telas montadas do canvas do jogo + estado (esteira 4: refino, barra de título, rodapé; tools/ui_screens/).
for f in tools/ui_screens/*.json; do step tools/ui_build_screen.py "$f"; done
step tools/ui_textures_extra.py
echo "Data/ pronto: $(find unreal/NWPort/Data -type f | wc -l) arquivos"
