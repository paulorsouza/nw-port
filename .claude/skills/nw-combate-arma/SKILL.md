---
name: nw-combate-arma
description: Esteira 1 — porta ou refina o combate de uma arma do New World (lança, machado grande, mosquete, cajados...; ou ajustes no arco, rapieira, espada, machadinha) com os dados do jogo. Usar quando pedirem "traz a lança", "porta o mosquete", "o golpe X está diferente do jogo", "refina o combate da arma Y".
---

# Portar ou refinar o combate de uma arma

Esteira 1 (`docs/esteiras.md`: ordem das armas, tabela de polimento, regras com as outras sessões; skill
`nw-esteira`). Roteiro completo: `docs/combate-armas.md`. Regras gerais do jogo: `docs/combate-regras-jogo.md`.
Leia os três antes, e `docs/analise-re/resumo-plano-a.md` (o que a RE já leu do cliente: combos pelos procclips
`CAGE-Transition*`, janelas `CAGE-Named`, saída das ações, reação, stamina, giro, homing, e as divergências do port).
Sub-esteiras 1a (manopla de gelo) e 1b (espada grande): arquivos próprios por arma, seção "Sub-esteiras de arma" de
`docs/esteiras.md`.

Princípio: número, janela e animação saem dos dados do jogo, com a fonte (arquivo + ID) num comentário curto ou no
`docs/armas/<arma>.md`. Nada de chute. Sem fonte nos dados: pergunte pela skill `nw-re`; sem resposta, seção "Sem fonte" do doc da arma.

## Passos

1. Relatório: `python3 tools/weapon_report.py <arma>` (arma fora da lista: `--tag --adb --base --damage --cost --ability`).
   Lê só o que já foi extraído; não precisa da Unreal.
2. Levantamento em `docs/armas/<arma>.md` (modelo: `docs/armas/arco.md`, `docs/armas/rapieira.md`): mecânica básica,
   cada ação (fragmento, clipes, janelas, DamageID, stamina, recarga), habilidades, status, proposta para o port.
   O que não estiver em `extract/`: lógica de estados nas actionlists CAGE (`extract/serverpak/client/.../cage/`);
   arquivo do pak com `tools/pesado.sh nwbt cat -g "$NW_GAME" <caminho>` (fila de trabalho pesado, ~6 GB cada).
3. Receita: montagens com prefixo próprio em `tools/sets/player/armas.json` (`montages`, `fragments`,
   `window_overrides` com tags e `.adb` da arma, `upper_montages`, `montage_starts`, `weapons`, `blendspaces`).
4. Converter: `source tools/env.sh && tools/pesado.sh python3 tools/build_set.py tools/sets/sword_shield.json`
   (New World original fechado). O `sword_shield.json` junta as partes de `tools/sets/player/`; só a `armas.json` é sua.
5. Ações: `<ARMA>_ACTIONS`, `_SHAPES`, `_ABILITY_IDS`, `_EFFECTS` e `SETS` em `tools/make_datatables.py`; depois
   `python3 tools/make_datatables.py extract/ue/sword_shield/manifest.json` e
   `python3 tools/make_sound_map.py extract/ue/sword_shield/manifest.json`.
6. Habilidades na HUD/maestria: `WEAPONS` em `tools/make_abilities.py`; `tools/build_all.sh 12` (dados e ícones).
7. C++: `FNWWeaponSet` em `ANWCharacter::AddPlayerWeaponSets` (`NWCharacterArmas.cpp`; números da arma do
   `javelindata_itemdefinitions_weapons`) e
   mecânica nova no `UNWCombatComponent` só se a tabela de mecânicas do roteiro não cobrir. Compilar com o editor
   fechado (`tools/ue_import_all.sh 01`); para só conferir sintaxe com o editor aberto, compile o alvo do jogo
   (`Build.sh NWPort Linux Development`).
8. Import com o editor fechado: `tools/ue_import_all.sh 02` (conjunto) e `09` (ícones). Só tabelas mudaram:
   `tools/pesado.sh UnrealEditor-Cmd ... -run=pythonscript -script="tools/ue/import_tables.py <manifest>"`.
9. Teste: abrir o editor e pedir ao dono para testar na arena (`L_Arena`, F10 > Arena / Trocar par de armas) com a
   lista do `docs/testes/armas.md`. Nunca rodar o jogo/teste sozinho; pedir o print.
10. Polimento até a tabela da Esteira 1 em `docs/esteiras.md` fechar (animação, mecânica, habilidades e maestria,
    som, efeitos, HUD, rede).

## Conferir no fim

- Todo número novo tem fonte (doc da arma ou comentário).
- `docs/combate-armas.md` (estado das armas, tabela de mecânicas) e `docs/testes/armas.md` (itens de teste) atualizados.
- `python3 tools/re_check.py` sem pendência da arma.
- Diferenças que ficaram: seção "Diferenças conhecidas" do doc da arma.
