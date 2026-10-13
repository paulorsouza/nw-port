---
name: nw-coleta
description: Esteira 2 — porta e pole os ofícios de coleta do New World (Mineração, Corte de madeira, Colheita, Esfolamento, Pesca): animação por ferramenta, regras de ofício e saque, som, efeitos, HUD e bugs visuais dos coletáveis. Usar quando pedirem algo de coleta, coletáveis, ferramentas de coleta, ofícios de coleta ou pesca.
---

# Coleta (esteira 2)

Regras de trabalho: skill `nw-esteira` e `docs/esteiras.md`. Estado e levantamento: `docs/coleta.md` (crie no
primeiro item). Testes: `docs/testes/coleta.md`, mapa `L_SouthWindsward`.
Ações de coleta seguem as regras das actionlists lidas pela RE (`docs/analise-re/cage-runtime.md`, resumo em
`docs/analise-re/resumo-plano-a.md`).

## Onde está hoje

- C++: `NWGatherComponent` (interação, montagens `AM_GatherMine`/`AM_GatherCut`/`AM_GatherChop` do conjunto do
  jogador, saque, XP de ofício) e `NWGatherField` (instâncias, respawn).
- Dados: `tools/make_gatherables.py` → `Data/World/guardaventos_gatherables.json`; malhas por
  `tools/convert_gatherables.sh` (skill `nw-convert-gatherables`) e `tools/ue/import_gatherables.py`.
- Pendências: `docs/pendencias.md`, seção "Coleta: polimento".

## Fontes do jogo

| O quê | Onde |
|---|---|
| Coletáveis (ferramenta, tempo, saque, variações, VFX/SFX de esgotar) | `datatables/javelindata_gatherables*.datasheet.json` |
| Ofícios (XP por nível, velocidade, bônus de saque) | `datatables/javelindata_tradeskill{mining,logging,harvesting,skinning,fishing}` |
| Saque | `javelindata_loottables*` (regras de dado em `docs/ESTADO.md`, "Dados do jogo") |
| Animações | fragmento `Gather` em `player_pick.adb`, `player_axe.adb`, `player_1h_melee.adb`, `player_2h_melee.adb`, `player_offhand.adb`, `player.adb`; pesca em `player_fishingpole.adb` (`Fishing_*`) |
| Lógica de estados | actionlists CAGE de coleta em `extract/serverpak/client/.../cage/` |
| Telas | `hud/unifiedinteractcard/*`, aviso de saque, bússola (`CompassIcon`); `docs/hud-original.md` |

`datatables/` = `extract/data/sharedassets/springboardentitites/datatables/`. Arquivo que não estiver extraído:
`tools/pesado.sh nwbt cat -g "$NW_GAME" <caminho>`.

## Por ofício

1. Levantamento em `docs/coleta.md`: o que o jogo define (tabela acima) contra o que o port faz.
2. Animações: montagens novas com prefixo `AM_Gather*`, ferramentas, fragmentos e janelas do `.adb` em
   `tools/sets/player/coleta.json` (parte do conjunto do jogador); conversão
   `tools/pesado.sh python3 tools/build_set.py tools/sets/sword_shield.json`.
3. Regras e saque no `NWGatherComponent` e no `make_gatherables.py`, com a fonte de cada número.
4. Som e efeitos: sons do Wwise já extraídos (`extract/audio`, `tools/extract_audio.py`), mapa de sons
   (`tools/make_sound_map.py`), VFX das variações.
5. HUD: cartão de interação, barra de progresso, aviso de saque, bússola.
6. Compilar e importar com o editor fechado (`tools/ue_import_all.sh 01`, `02`; coletáveis `03`/`13` conforme o caso),
   abrir em `L_SouthWindsward`, pedir print.

Esfolar precisa de criatura morta: criaturas e IA são da esteira de armas; peça pelo relatório o que faltar.
