# Guardaventos completo (F4)

Pedido de 2026-10-11: só Guardaventos, com NPCs, minérios e o resto, antes de qualquer outra região. A fila de
conversão das outras cidades foi cancelada (edengrove já convertido fica guardado, sem import).

## O que é Guardaventos no jogo

Território inteiro da região `r_+04_+01` (x 8200–10200, y 2050–4090: 2 km × 2 km). O assentamento é o
`Settlement_SouthWindsward_Complete`, que já está no UE (`L_SouthWindsward`, com recorte do terreno).
Camadas em `extract/terrain/.../regions/r_+04_+01/capitals/`:

| Camada | Conteúdo |
|---|---|
| `06swindsward_gatherables` | 1.412 coletáveis + 1.169 lotes de vegetação (`<PLOT>`) |
| `06sw01..09_*_ai` | 1.263 spawners de criaturas nos 9 POIs, 180 baús de saque |
| `06sw01..09_*_props`, `06swindsward_capitals`, `roaddressing`, `water` | props dos POIs, estradas, água |
| `06swindsward_persistence` | cidade, 5 teleportes, forte, posto avançado, NPCs, lotes de acampamento |

Tabelas extraídas em `extract/data` (`--x-datasheet json`): `gatherables`, `variations_gatherables_*`
(malha cheia e esgotada, raio, ícone), `loottables`, `variations_npcs`, `*_npcs`, `basevitals_*`, `crafting_*`.

### Coletáveis

| Tipo | Ofício | Qtde |
|---|---|---|
| Alquimia (fungos, plantas elementais) | Colheita / Mineração | 454 |
| Plantas (cânhamo, legumes, frutas) | Colheita | 343 |
| Minérios (ferro, prata, ouro, pedra, guano) | Mineração | 312 |
| Pedras soltas | sem ferramenta | 124 |
| Arbustos | Colheita | 138 |
| Árvores | Corte de madeira | 40 (o resto da floresta vem dos lotes `<PLOT>`) |

161 malhas distintas (cheia + esgotada). Exemplo: `Vein_Iron_c` → `OreVeinSmall`, picareta, 15 s, saque
`OreVeinFinishSmall`, respawn 504–720 s.

### Criaturas dos spawners

Javali (96), lobo cinza (72), lince (21), esqueleto rastejante (64), esqueleto com clava (115), esqueleto com
espada (55), Risen/Withered (40), navegador morto-vivo (29). Os 541 genéricos (`0wp_Wander`, `1wp_Stand`, ...)
dependem do conteúdo do slice para saber a criatura.

## Etapas

Cada etapa termina testada pela rede, como a F3. Conversões pesadas (nwbt de slices, import na Unreal) rodam
uma por vez, sem build em paralelo.

- **G1 — Coleta.** Gerador `tools/make_gatherables.py` (capitals + variações + tabelas → `Data/World/
  guardaventos_gatherables.json`), conversão das 161 malhas, `ANWGatherable` autoritativo no servidor
  (interação com E, tempo do jogo, malha esgotada, respawn, saque no inventário), ferramentas (picareta, foice,
  machado de lenhador, faca de esfolar) e animações de coleta do jogo. Ofícios com nível no backend.
- **G2 — Terreno da região.** Heightmap inteiro (2048², altura = valor/32) como Landscape, materiais pelo splat,
  água, estradas e props dos 9 POIs.
- **G3 — Criaturas.** Conteúdo dos spawners → tabela de spawn; um conjunto (`tools/sets/*.json`) por família,
  começando por javali e lobo; IA de patrulha/vagar do jogo; saque e XP.
  **Em andamento (2026-10-12):** `tools/make_spawns.py` (1216 spawns, 114 criaturas; spawner → AIVariantProvider →
  `javelindata_vitals`; waypoints com tarefas; vida/dano na escala do port pelo `vitalsleveldata`), conjuntos
  `boar` e `wolf` importados, `ANWCreature` (patrulha, perseguição, volta para casa), `ANWSpawnField` (nasce a 60 m,
  some a 90 m, renasce em 2–5 min), carcaça para esfolar (coletáveis `Boar`/`Wolf` do jogo). Falta: o grafo do
  `ABP_NW_Creature` (manual), testar, XP de personagem por abate (escala do `KillEvent` desconhecida), saque de
  `CreatureLootMaster` (condições/tags), próximas famílias (peru, coelho, esqueletos, Lost, Corrompidos...).
- **G4 — Cidade viva.** NPCs da cidade com idle, interação com E (cartão do jogo), baú da cidade, mercador,
  teleportes.
- **G5 — Baús de saque e vegetação.** 180 baús dos POIs; floresta dos lotes `<PLOT>`.
