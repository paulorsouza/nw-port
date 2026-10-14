# Sistemas e Queda Eterna (esteira 4)

Regras: `docs/esteiras.md`. Testes: `docs/testes/sistemas.md`. Mapa: `L_QuedaEterna`.
Tabelas do jogo em `extract/data/sharedassets/springboardentitites/datatables/` (abaixo `DT/`).

## Cidade

| Item | Valor |
|---|---|
| Slice | `slices/structures/settlements/v2_settlements/settlement_northwindsward_complete.dynamicslice` (não existe `settlement_everfall`) |
| Capital | `ef_persistence`, região `r_+04_+02`, posição (8931, 4224, 135) |
| Terreno | regiões 4_1 e 4_2, quadrado de 768 m (`make_terrain.py`) |
| Conversão | `tools/build_all.sh sistemas-01` (`convert_city.sh settlement_northwindsward_complete`) → `extract/cities/limpa/settlement_northwindsward_complete.glb`, `extract/cities/northwindsward_terrain.glb` |
| Slices em JSON | `tools/build_all.sh sistemas-02` → `extract/slicejson/slices/...` (cidade e lotes `housing/plots/northwindsward/`) |
| Import | `tools/ue_import_all.sh sistemas-01` (cidade, `L_QuedaEterna`) e `sistemas-02` (terreno, partida, materiais); `tools/etapas/sistemas.ue.sh` |
| Exposição | volume `ExposicaoDoJogo` no mapa (`tools/ue/city_exposure.py`, etapa `sistemas-04`): histograma de 0,1 a 1, da faixa 1x–10x do ciclo de dia do jogo (`libs/timeofday/zones/everfall_a.xml`); o projeto desliga a exposição automática |
| Ponto de partida | com o editor aberto no mapa (fora do Play): `python3 tools/ue/remote.py tools/ue/spawn_trace.py`. O `fix_spawn.py` deixava o personagem dentro do piso da praça e ele atravessava a cidade e o terreno |

Território 4 = `@Everfall` (`DT/javelindata_territorydefinitions`): nível 1–25, TerritoryStandingXpModifier 1.25,
PoiDiscoveryXpModifier 75.

## Base comum do port

- Servidor dedicado → nw-core por HTTP/JSON: `UNWBackend` (`NWBackend.cpp`), header `X-NW-Server-Secret`. Rotas
  internas: `POST /internal/tickets/redeem`, `PUT /internal/characters/{id}/state` (a cada 30 s e no logout; grava o
  personagem inteiro numa transação, apaga e reinsere os itens).
- Migrations: 0001 contas e personagens, 0002 `characters.coins` e `items` (container `bag|equip`), 0003
  `characters.progression` (`{level, xp, spent[5], trade[4]}`), 0004 armazém, 0005 mercado, 0006 casas.
- Armazém, mercado e casas ficam fora do PUT state. Cada ação leva a bolsa e as moedas de depois dela e grava
  tudo numa transação; o armazém tem versão (409 `storage_changed`). Rotas e regras: `backend/README.md`. Testes:
  `backend/tests/api.rs` (`armazem_*`, `mercado_*`, `casa_*`).
- Interação: tecla E (`IA_Interact`) só chega ao `UNWGatherComponent`. Não há interagível genérico. Reaproveitável:
  foco → texto → RPC no servidor com checagem de distância (`InReach`), cartão `ANWHUD::DrawGather`, anel de progresso.
  Falta o cartão de várias opções `hud/unifiedinteractcard/interactscreen` (`docs/hud-levantamento.md`).
- Telas: layout do jogo por `tools/ui_build_*.py` → `Data/UI/*.json`, desenho em C++. Canvases ainda sem build:
  `crafting/craftingscreenv4`, `stationproperties/stationproperties`, `container/catcontainer`, `shop/shop`,
  `contracts/contractbrowser`, `contracts/compensations`, `housing/*`, `conversation/conversationscreenv2`.

## 1. NPCs

Jogo:
- `DT/javelindata_variations_npcs` (1360 linhas): VariantID, NPCId, Name, CharacterDefinition (`.cdf`), Greet/Bye/
  Idle/InteractTimeline, LookPoseTimeline. Ex.: `0407a_Commander` → `0407_commander.cdf`, idle
  `malek_standing_ambient.timeline`. Variantes `_walkaway` (257) e `_clientpathing_walkaway` (6).
- Nome, título, diálogo: `DT/questdata/npcdata_npc_NNNNN` (NPCId, GenericName, Title, ConversationStateId1..10).
- `DT/javelindata_npcs` está vazia.
- Queda Eterna: variantes `04xx` (Magistrate, HuntsmanLee, Constable, ArtificerPetrowski, Commander, Alchemist,
  Adjudicator, BarkeepPolly, Innkeep, Fisher, TownGuard e outras, 41 no total) e 11 `EF_Ambient_*`.
- Posição: entidades do slice da cidade. Slices de NPC de missão em `slices/characters/mission/npc_04*`.

Marcador sobre o NPC (scripts de interface `lyshineui/markers/markerdata.luac` tipo `Conversation` e
`staticworldmarker.luac`, lidos com `tools/luac_dump.py`):
- Não mostra o nome: só um ícone pelo estado de conversa (`eConversationState_*`). `NoObjective` →
  `npc_noObjective.dds`, `OnCooldown` → `icon_questCooldown_npc.dds`, `HasObjective` → `icon_questAvailable_npc.dds`
  (história principal: `icon_questMainStoryAvailable_npc.dds`), `HasTalkToTask` → `icon_questDialogue_npc.dds`,
  `InProgress` → `icon_questInProgress_npc.dds`, `TurnIn` → `icon_questReadyForTurnIn_npc.dds`, `HasShop` →
  `icon_npcshop.png`.
- Altura 1,9 m (`wZ`). Escala de 1 a 0,35 até 40 m (`scaleDistance`); some de 125 a 250 m (`fadeStart` 125,
  `fadeDistance` 125). Aparece através de paredes (`allowWorldOcclusion` false). Cartão de interação a 1,1 m
  (`interactWorldZOffset`).
- O nome aparece só no cartão de interação, com o NPC em foco.

Port (esteira 4):
- Lista: `tools/make_npcs.py` → `Data/World/queda_eterna_npcs.json` (30 NPCs a até 300 m da capital) e o conjunto
  das animações `tools/sets/npc_queda_eterna.json` (16 laços, esqueleto `npc_skel`).
- Fontes da lista: capitals `ef_lore`, `ef_persistence`, `04nwindsward_lore`, `ambient_4,1`, `ambient_4,2`. A
  variante vem de `variantName` (ambientes) ou de `m_slicevariant` nos spawners `coatgen/<capital>/MasterNpc_*`
  (etapa `sistemas-02` do build_all). As timelines vêm da etapa `sistemas-03`.
- Sem missões, vale o estado inicial: `MasterNpc_SwapShown_*` fica de fora.
- Também ficam de fora a variante `DEPRECATED` e a Grace O'Malley (esqueleto próprio, NPC de missão).
- A IdleTimeline toca um alias (`animationalias`) que é o nome do `.caf` em `player/male/`. Os
  `*_look_poses_15x` (olhar, aditivos) ainda não entram.
- Aparências: `tools/convert_npcs.py` (nwbt + `tools/export_npcs.py` no Blender) → `extract/ue/npcs/SK_*`.
- Import: etapa `sistemas-03` do `ue_import_all` (`import_nw.py` do conjunto + `tools/ue/import_npcs.py`), que
  salva um `ANWNpcField` no mapa.
- C++: `ANWNpc` (cápsula, malha, laço parado, nome) e `ANWNpcField` (lê o JSON e cria os NPCs em cada máquina,
  sem replicar, encaixando cada um no chão por raio).

## 2. Estações de refino e ofício

Regras do port para fabricar (tempo, espectros, evolução das mesas): `docs/oficio.md`. Itens fabricados: `docs/itens.md`.

Jogo:
- Receitas: `DT/javelindata_crafting_*` (14 arquivos, 8294 linhas; `javelindata_crafting` vazia). Colunas: RecipeID,
  CraftingCategory, Tradeskill, RecipeLevel, StationType1..4, ItemID, OutputQty, Ingredient1..7, Type1..7
  (`Item`/`Category_Only`), Qty1..7, CraftingFee, UseCraftingTax, BaseGearScore, BaseTier, GameEventID,
  CooldownSeconds, BonusItemChance. Ex.: `IngotT4`, Smelting 100, `smelting2`, OreT4×6 + IngotT3×2 + FluxT5 +
  CharcoalT1, taxa 7.
- StationType = tipo + tier mínimo (`smelting2`, `blacksmith5`, `camp1..5`, `Specialty_*Shop1`).
- Categorias: `DT/javelindata_craftingcategories` (1466).
- Níveis: `DT/javelindata_tradeskill<ofício>` (21 tabelas, níveis 0–500): MaximumInfluence, RecipeLevel, RollBonus,
  GearScoreBonus, XpReward (XP de personagem ao subir).
- XP por fabricação: GameEventID → `DT/javelindata_gameevents` (CategoricalProgressionId/Reward,
  UniversalExpAmount, TerritoryStanding).
- Tier da estação na cidade: `DT/javelindata_territoryprogression` (projetos de upgrade por estação).
- Slices: `slices/structures/settlements/stations/settlement_station_<tipo>_t2..t5|_alt|master`.

Port (refino, esteira 4):
- Dados: `tools/make_crafting.py` → `Data/Crafting/refining.json`.
  - 5 estações de Queda Eterna, T5, com a posição do slice: tecelagem, fundição, carpintaria, curtume e cantaria.
    O nome vem de `<Tipo>T5_MasterName`.
  - 125 receitas conhecidas por padrão dos 5 ofícios de refino, sem as `TerritoryStandingSupplies*` (pedem azoth).
  - Categorias de ingrediente vindas de `IngredientCategories`.
  - XP de ofício vindo de `gameevents`.
  - Roda no `make_data.sh` antes do `make_items.py`, que põe no catálogo os itens das receitas.
- Progressão: 9 ofícios (4 de coleta + Smelting, Woodworking, Leatherworking, Weaving, Stonecutting), tabelas do jogo
  até o nível 250 (`make_progression.py`, `NWTradeskill::Num`).
- Mundo: `ANWStation`, criada pelo `ANWNpcField`, tem o cartão "[E] Refinar" (`ui_prefab_refinery_interact`).
- Servidor: `UNWInteractorComponent::ServerRefine` → `FNWCrafting::Refine`.
  - Confere estação a até 5 m, tier, nível do ofício (livre no PIE sem backend), materiais e taxa.
  - Categoria: gasta primeiro o item de que se tem mais.
  - Dá o produto e o XP do ofício.
  - Instantâneo.
  - Item extra: chance = `BonusItemChance` + ajuste de tier do reagente + `RollBonus` do nível × 0,0001
    (`sistemas-001`, ids `crafting.bonus_*`; Fundição 250 = 30%, igual ao jogo). Quantidade: floor(chance) + 1 com a
    parte fracionária (H, servidor do jogo).
- Tela: `FNWRefiningMenu`, com as peças de `craftingscreenv4`. Lista à esquerda; à direita produto, nível do ofício,
  materiais tenho/preciso, taxa, quantidade (−, +, Máx.) e Refinar.
- O `NWHUD` expõe as telas da esteira 4 ao controlador (`IsStationMenuOpen`, `CloseStationMenus`,
  `StationMenuClick`, `StationMenuWheel`).
- Dev: F10 > "Bolsa de dev (peso)" alterna o peso máximo entre 1.000 e 10.000.000 (não salva).

Port (fabricar, esteira 4, 2026-10-13; regras em `docs/oficio.md` e `docs/itens.md`; testado pelo dono):
- Mesas: forja (`blacksmith`, Weaponsmithing), oficina (`engineering`, Engineering) e repositório arcano (`alchemy`,
  Arcana) do slice da cidade. 33 receitas de arma, escudo e ferramenta T2–T5 (`gear` em `refining.json`, com material
  refinado fixo do tier) e 9 de alquimia (poção de vida T5, reagentes T2), na hora como o refino.
- Ofícios 9..11 na progressão (tabelas do jogo até 250, com o bônus de GS do artesão por tier).
- Servidor: `FNWCrafting::Craft` / `UNWInteractorComponent::ServerCraft`. Abaixo do nível 50 é treino (só XP). Do 50 em
  diante o item é sorteado (`FNWItemRules::Roll`, faixa `FNWCrafting::GsRange`) e vai para a bolsa; item assinado
  (qualidade ≥ 80%) pega o número de série no nw-core antes (migração 0009, `backend/src/routes/serials.rs`).
- Melhorar (aba "Melhorar itens" da mesa): `ServerImprove` / `FNWCrafting::Improve` com `ENWImprove` Attribute (outro
  atributo), GearScore (refinar: 51% sobe, senão desce, 1 mais provável que 2, dentro da faixa da fabricação, `FNWCrafting::GsOdds`) e Unbind,
  pagas no material escolhido na tela (`FNWCrafting::ImproveMaterials`, `materiais_melhoria`). Custo provisório em
  `FNWCrafting::ImproveCost` (`tools/itens.json` `_custos`, `custo_melhoria`). Captura: `tools/ui_shots/melhorar.json`.
- Resultado: `ClientCraftResult` (item antes e depois) → `UNWInteractorComponent::CraftResult`; a tela espera o `Seq`
  e mostra o roll de equipamento do jogo (`FNWRefiningMenu::DrawGearRoll`, `docs/analise-re/craftroll-equipamento.md`).
- Item: `FNWItem` com atributo, qualidade, perks, gema, vínculo e assinatura (`props` no nw-core, migração 0008);
  regras em `Data/Items/regras_itens.json` (`tools/make_itens.py`, `tools/itens.json`, `tools/perks_decisoes.json`).
  Tooltip com perks com ícone e o slot de gema (moldura do soquete).
- Captura: `tools/ui_shots/forja.json` (receitas e o roll no meio e no fim). Dev (F10): "+50 níveis nos ofícios de
  fabricar" e "Materiais de fabricar".

Port (gemas e vidro rúnico, esteira 4, 2026-10-13; regras em `docs/itens.md` "Slot"):
- Itens: as 4 lapidações do jogo (`<Gema>CutT1..T4`, Tier de item 2–5, 2–5%) de Ônix, Opala, Diamante, Safira, Jaspe,
  Esmeralda e Malaquita, e o vidro rúnico `Runeglass_Gem_<Gema>_Melee|Arcane` (gema Pura + amplificação física ou
  mágica). Efeitos por item em `Data/Items/regras_itens.json` `gema_itens` (`tools/make_itens.py` a partir de
  `tools/itens.json` `gemas`, `gema_pct_por_tier`, `vidro_capsulas`, `vidro_amplificacao`).
- Receitas do jogo (`javelindata_crafting_jeweler`, `_weapon`, `_refining`): lapidar na estação de equipamentos
  (`outfitting`, ofício Joalheria, índice 12 do `NWTradeskill`); cápsula do vidro na forja e vidro na cantaria, tier 5,
  nível 180 (o jogo usa a estação de runas, que Queda Eterna não tem). Mesmo caminho do refino (`FNWCrafting::Refine`).
- Servidor: `UNWInventoryComponent::ServerSocket` (gema da bolsa no item da bolsa ou equipado; só em slot vazio: com
  gema, recusa e avisa para retirar antes) e `ServerUnsocket` (gema volta para a bolsa; custo provisório: 1
  gema bruta do mesmo tipo e tier, `FNWItemRules::UnsocketCost`). Regra de encaixe: `FNWItemRules::CanSocket` (gema
  de armadura em armadura e escudo, de arma em arma; só item fabricado). A gema fica em `props.gem` (sem migração).
- Telas: soltar a gema no item vazio abre "Acoplar Gema" (texto do jogo) com o efeito; Shift + clique direito no item
  com gema abre "Retirar gema" com o custo. Miniatura do item com o ícone da gema no canto de baixo à direita. Tooltip do item com ícone, nome e efeitos da gema; da
  gema solta, os efeitos e onde acopla (sem a descrição do jogo, que fala dos efeitos do jogo). Tela de resultado:
  `GemPerkSlot` com o ícone e os efeitos.
- Captura: `tools/ui_shots/gemas.json` (bolsa, tooltips, acoplar, retirar). Dev (F10): "Gemas e Joalheria
  200" (Joalheria e Cantaria no 200, 7 gemas Puras, 2 vidros, gema bruta para o custo, material para lapidar e montar
  um vidro).
- Efeito no personagem (resistência, crítico, amplificação): fica para o item 3 da esteira (com a esteira 1).

Port (polimento da tela de resultado, esteira 4, 2026-10-13):
- Fim do roll de equipamento: botões Continuar a Confeccionar, Equipar (só com item equipável na bolsa; ferramenta
  "Equipar Ferramenta") e Sair da Estação no `ActiveButtonsContainer` do jogo (x 232, y 520, coluna de 370, espaçamento
  8; craftroll-equipamento.md §8). Tomam o lugar do "Clique em qualquer lugar"; clicar fora continua voltando à mesa.
- Sons (craftroll-equipamento.md §9): `tools/extract_audio.py` grupo `ui` → `extract/audio/ui/crafting` (102 arquivos,
  21 eventos); `tools/make_ui_sounds.py` lê os `.txtp` e grava as camadas e variações em `Data/Audio/ui_sounds.json`;
  `tools/ue/import_ui_sounds.py` (etapa `sistemas-05`) importa em `/Game/NW/Audio/UI`. `FNWRefiningMenu::Sound` sorteia
  uma variação por camada (a raridade toca duas camadas juntas). Tocam: abrir/fechar a mesa, escolher receita, botão
  Fabricar/Refinar, fim do refino (1,4 s), raridade (0,5 s), cada perk e a gema. Pular o roll não toca os pendentes.
- Lista de receitas com 16 linhas visíveis (a 17ª saía cortada).

Tela (2026-10-13): `craftingscreenv4` no modo refino com estilos e posições do Lua (`docs/hud/refino.md`,
`tools/ui_screens/refining.json`), conferida pela captura sem o jogo (`tools/ui_shots/refining.json`) e testada pelo
dono. O que falta na tela: `docs/hud/README.md`.

## Bolsa e chão (decisão do dono 2026-10-13)

- Bolsa com 500 espaços (pilha única por espaço; `UNWInventoryComponent::BagSize`, `MAX_BAG_SLOTS` no nw-core). O peso
  dá a sobrecarga, como antes.
- Bolsa cheia: o que chega (fabricar, refinar, coletar, F10) vai para o chão numa bolsa de saque, com aviso; não
  cancela nada (`UNWInventoryComponent::Grant`).
- Soltar no chão (arrastar para "Soltar itens" ou Ctrl + clique > Soltar) não destrói: cria uma bolsa de saque aos pés
  (`ANWLootBag`, saco `arc_sacks` do jogo). Só quem soltou vê e pega. Some em 3 minutos (o `@inv_discard_life_warning`
  do jogo diz 3; era 2 até 2026-10-14). Até 1000 kg no chão por jogador (o excesso de bolsa cheia não conta). Gravado
  no nw-core (etapa 2 das gravações, `docs/plano-servidor.md`): volta no mesmo lugar se o jogador sair e entrar ou o
  servidor reiniciar antes de vencer.
- Tela da bolsa (`catcontainer` no modo bolsa de saque): [E] Abrir abre o inventário com a janela "Itens Derrubados"
  no lugar do armazém (`FNWInventoryMenu::OpenLootBag`): ícone `icon_dropped_items`, aviso de tempo de vida e "N itens"
  no lugar da busca e da ordenação, peso da bolsa / 1000 kg. Pega item por item (arrastar para o inventário, clique
  direito, Ctrl + clique > Pegar com quantidade; `ServerLootTakeItem`), a seção inteira ou Pegar Tudo
  (`ServerLootTake`); não guarda na bolsa do chão. Bolsa vazia ou expirada: fica só o inventário. Captura:
  `tools/ui_shots/lootbag.json`. Diferença do jogo: a lista mantém as seções e a largura do armazém (no jogo a janela
  é estreita, 340, sem seções).

## 3. Armazém

Jogo: slice `settlement_storage_geo_master` (`@ui_global_storage`). Crescimento por standing do território:
`DT/javelindata_territory_standing` (301) → TB_GainStorage em `DT/javelindata_progressionpointdata`;
`DT/javelindata_categoricalprogression` id 4 = Everfall (MaxLevel 300).

Capacidades (`javelindata_encumbrancelimits`, décimos; `docs/analise-re/hud-inventario.md`): `StorageSmall` 150,
`StorageMedium` 1000, `StorageLarge` 1500, `StorageXLarge` 2000 kg. Tela do jogo: `container/catcontainer.lua`.

Port (esteira 4; armazém compartilhado decidido pelo dono em 2026-10-14):
- 2 despensas de Queda Eterna, lidas do slice (`settlement_storage_geo_master`, nome `ui_global_storage`), entram
  como estações do tipo `storage` (`make_crafting.py`). O cartão é "[E] Abrir".
- Um armazém só por personagem (`shared` no nw-core, migration 0010), o mesmo em qualquer despensa de qualquer cidade.
  Os itens que estavam em `settlement:*` e `house:*` foram juntados nele.
- Capacidade = soma das fontes, sem base: 1000 kg por cidade conhecida (`tools/itens.json` `armazem_por_cidade`); os
  baús das casas e o nível da cidade entram como fontes quando existirem (`UNWInteractorComponent::StorageCapacityNow`).
  O servidor manda a capacidade ao nw-core no salvamento do personagem e em toda gravação do armazém.
- Cidade conhecida: entrar na área da cidade (centro dos NPCs e estações do `ANWNpcField`, raio até o mais longe +
  40 m) ou abrir a despensa dela; aviso "Cidade conhecida: armazém +1000 kg"; salvo em `progression.cities`.
- 5 abas (`armazem_abas`): cada item guarda a aba; guardar vai para a aba aberta; o mercado entrega na aba 1. Soltar um
  item numa aba guarda nela (da bolsa) ou passa a pilha para ela (do armazém, `ServerStorageSetTab`); clique duplo na
  aba aberta renomeia (até 24 letras, `ServerStorageRenameTab`). Peso único para as 5 abas.
- Acima da capacidade (o mercado entrega mesmo cheio): retira e vende, não guarda nem compra.
- Servidor (`UNWInteractorComponent`, `ServerStorageOpen/Move/MoveMany/SetTab/RenameTab/Close`):
  - Depositar respeita o peso; retirar deixa no armazém o que não couber na bolsa.
  - Grava no nw-core bolsa e armazém juntos (`UNWBackend::PutStorage`, com versão). Se der 409, relê, refaz e grava
    de novo uma vez.
  - Sem backend (PIE), fica na memória.
- Tela: o inventário do jogo em modo armazém (`FNWInventoryMenu::OpenStorage`, `docs/hud/inventario.md`): segunda
  janela à direita, arrastar entre as duas, Shift + clique, dividir pilha, mover a seção inteira e "Pegar tudo"
  (`ServerStorageMoveMany`, uma gravação só).

## 4. Lojas e mercadores

Jogo: `DT/javelindata_shopdata` (11 lojas de facção/evento com moeda própria); lojas especiais vendem por receita
(`crafting_specialtyshops`, estações `Specialty_*Shop1`). Moedas em tabelas `*currency`. Não há tabela de vendedor
NPC com estoque e preço em ouro.

Os itens e preços das lojas de facção ficam no servidor do jogo (o cliente só tem o cabeçalho em `shopdata`).

Port (decisão do dono 2026-10-13: loja enxuta, só o essencial, sem itens genéricos):
- Moedas em centavos como no jogo (`UNWInventoryComponent::FormatCoins`, ÷100 do `GetLocalizedCurrency`).
- Poções: só a T5 existe no port (catálogo, saque do Lost e kit inicial). T1–T4 somem de quem já tinha ao entrar
  (item fora do catálogo não carrega).
- Mercador de Queda Eterna: `EF_Ambient_Merchant_00`, cartão "Loja · [E] Comprar" (`ui_shop_default_name`, `ui_buy`).
- Lista e preços em `tools/shops.json` → `Data/World/shops.json` (`make_crafting.py`): Poção de vida T5 2,00; os 5
  reagentes de refino T5 a 0,20 com limite semanal de 100 por personagem (número do port, ajustável no arquivo).
- Limite renova com os brindes (terça 08:00 UTC). Compras da semana em `progression.shop` (`{period, bought}`); sem
  mudança no backend.
- Servidor: `UNWInteractorComponent::ServerShopBuy` confere NPC a 4 m, limite, moedas e peso; paga só o que entrou na
  bolsa; grava na hora (`ANWGameMode::SavePlayer`).
- Tela `FNWShopMenu` com o canvas do jogo `lyshineui/shop/shop` (`tools/ui_screens/shop.json`): grade de cartões
  (`shopgriditem.lua`, espaço 11×12) e detalhe com quantidade e botão herói com o custo; no limite, relógio vermelho no
  cartão e "Próxima recarga" com o tempo até renovar. Sem painel de rank nem prévia 3D. Captura: `tools/ui_shots/shop.json`.

## 5. Mercado (posto de trocas)

Jogo: slice `Settlement_Trading_Post_Master`; `DT/javelindata_territorygovernance` (TradingPostLevel);
TB_TaxTradingPost no standing; TradingCategory/Family/Group nas definições de item.

Regras (`sistemas-003`, `docs/analise-re/posto-de-trocas.md`; valores em centavos, ids `market.*`):
- Durações de 1, 3, 7 e 14 dias (padrão 3), iguais para compra e venda.
- Taxa de listagem: `trunc(preço × qtd × pct) + fixo` por duração (50/2%, 100/2,5%, 200/3%, 400/3,5%), vezes os
  descontos. A ordem de compra paga preço × qtd + taxa na criação.
- Taxa de transação: 5% de preço × qtd, vezes os descontos e a taxa do território (1,0 nesta build). Quem compra paga
  preço + taxa; quem vende para uma ordem de compra recebe preço − taxa.
- Descontos multiplicados entre si:
  - standing `TB_TaxTradingPost`: soma de max(0,25%; 5% × 0,865^(k−1)) por ponto, 5% com 1 ponto, 55,3% com 101;
  - companhia dona do território: 30%;
  - buff de facção: 5%.
- Limite de 100 contratos de compra e venda.

Port (decisões do dono 2026-10-14; as taxas e o limite de 100 acima são do jogo e ficam só como referência):
- Um mercado só para todas as cidades: o servidor usa o território `global`.
- Sem taxas: nem de listagem, nem de transação (`fee` = 0).
- Até 50 pedidos abertos por personagem, compra e venda somadas (`mercado.max_ordens` em `tools/itens.json`;
  nw-core 409 `too_many_orders`).
- O que vai ao mercado (`FNWItemRules::CanTrade`, `mercado` em `tools/itens.json`): só item não vinculado e, dele,
  equipamento fabricado (arma, armadura, joia, ferramenta), gema lapidada (`Cut*`, vidro rúnico) e refinado final com
  limite diário (`mercado.itens`: Asmodeum e os outros T5.1). Matéria-prima e refinado intermediário (lingotes,
  tábuas, couros e tecidos abaixo do final) não vão. Venda de matéria-prima será para um NPC comprador (depois).
- Durações 1, 3, 7 e 14 dias (padrão 3).
- Tudo cai no armazém compartilhado. Compra (pedido de compra ou comprar de uma venda) só passa se armazém + compras
  abertas + a compra couberem na capacidade (nw-core, 409 `storage_full`; peso unitário em `weight` do pedido e do
  item do armazém). Venda não tem limite; venda vencida volta ao armazém mesmo acima da capacidade.
- Pedido de compra: preço × quantidade sai das moedas na criação. Cancelado ou vencido, o que sobrou vira crédito.
- Venda completada por outro jogador vira crédito. Créditos são resgatados ao abrir o posto (`GET /credits` e o
  resgate até `until`, gravados como operação).
- Vender para um pedido de compra: o item sai da bolsa e as moedas entram na hora, na mesma operação gravada.
- Só perto do posto de trocas da cidade (estação `tradingpost`, raio da interação), checado no servidor.
- Tela própria (dono 2026-10-14), primeira do port em Slate (`SNWMarket`), com as cores e fontes do jogo: abas
  Comprar, Vender e Meus Pedidos; tabela ordenável por coluna (nome, escala, GS, qualidade, vantagens, vendedor,
  quantidade, preço); filtros (busca por nome ou vantagem, categoria, escala, raridade, GS mínimo, atributo, vantagens
  que aparecem à venda, pode pagar); painel do item com atributo, qualidade, vantagens com o valor, gema e artesão.
  Vender sugere o menor preço anunciado do mesmo item.
- Pedido de compra fica fora da tela (quase nada pode ser pedido); o servidor ainda aceita.
- Resposta otimista: comprar, vender e cancelar mudam a tela na hora (spinner até o servidor gravar); recusado, volta
  ao que era e a linha pisca em vermelho com o motivo.
- Fora da primeira versão (dono 2026-10-14): preço médio de itens iguais (histórico de vendas, mediana).
- Depois: NPC comprador de matéria-prima.

## 6. Casas

Jogo:
- `DT/javelindata_housetypes` (5): custo, recarga de viagem, standing mínimo, limite de itens. Ex.: Tier1 500000,
  240 min, standing 1, 100 itens.
- `DT/javelindata_housingitems` (1084): PrefabPath, categoria, tags, StorageBonus, Tier, CraftingRecipe.
- Lotes: `slices/housing/plots/northwindsward/cityd_house_address_NNN` (17 endereços: 001, 002, 004–008, 010–014,
  016, 019, 020, 022, 023).

Port: nada (casas do mapa abertas).

## 7. Nível e XP do personagem

Jogo:
- `DT/javelindata_xpamountsbylevel` (níveis 0–69): XPToLevel, AttributePoints, Health, TradeSkillPoints,
  MaxEquippableGearScore, HouseTrophySlots.
- Fontes: `gameevents` (UniversalExpAmount, TerritoryStanding), inimigos (`vitalstables/javelindata_basevitals_*`,
  `javelindata_vitalsleveldata`, `javelindata_aileveldisparity` KillExpModifier), XpReward dos ofícios e do
  standing, missões (`territoryprogression_missions`, `questdata/objectives_quest_*`).

Port: `UNWProgressionComponent` (nível, XP, atributos, XP dos 4 ofícios de coleta), salvo em
`characters.progression`; telas de barra de XP, banner de nível, Atributos, Ofícios. XP de personagem só pelo F10.

Decisão do dono (2026-10-12): as missões e a lore do jogo não serão convertidas. O nível vem de um tutorial curto de
missões simples, do nível 1 ao 60 em cerca de uma hora:

- Tudo vira tutorial. As missões já nascem liberadas no personagem: não há NPC que entregue missão, e o
  jogador escolhe a ordem.
- A recompensa é em níveis inteiros: o XP que falta para subir N níveis na tabela `xpamountsbylevel`, cuja curva
  continua a do jogo.

| Tutorial | Exemplo | Recompensa |
|---|---|---|
| Arma (um por arma, cresce com a esteira 1) | matar 10 bonecos | 2 níveis |
| Coleta (um por ofício) | pegar 10 madeiras | 2 níveis |
| Montaria (esteira 3) | invocar e cavalgar | 2 níveis |
| Mesa de ofício (uma por estação) | fabricar 1 item na mesa | 1 nível |
| Facção (as 3 do jogo) | missão simples de matar ou coletar | 2 níveis |

- Depois dos tutoriais, se o jogador ainda não chegou ao 60, todo inimigo dá muito XP até o 60.
- Maestria de arma: rápida até o nível 10 e lenta depois disso, até a maestria completa.

Implementação:
- Missões: `tools/tutorials.json` (13 por enquanto, 21 níveis), copiado para `progression.json` pelo `make_progression.py`.
  - 4 de arma (Espada, Machadinha, Arco, Rapieira): derrotar 10 inimigos com a arma ativa, 2 níveis cada.
  - 4 de coleta: coletar 10 vezes, 2 níveis cada.
  - 5 de mesa de refino: refinar 1 item, 1 nível cada.
  - Montaria, mesas de ofício e facções entram quando existirem.
- Eventos: `FNWEvents` (`NWEvents.h`) no servidor. O abate sai de `UNWCombatComponent::Die` (1 linha, esteira 1), o
  XP de ofício de `UNWProgressionComponent::AddTradeXp` e o refino de `FNWCrafting::Refine`.
- Progresso: `UNWProgressionComponent::TutorialCount` (-1 = concluída), salvo em `progression.tut` e replicado ao
  dono.
- Ao concluir: o XP exato de N níveis (`XpForLevel`) e o aviso "Missão concluída".
- Depois de todas: cada abate dá 25% do XP do nível atual, até o 60 (regra do port, `apos_tutoriais`; a base do
  jogo é a pergunta `sistemas-004`).
- Campo de treino: o `ANWNpcField` cria os bonecos de `tools/tutorials.json` "bonecos" no servidor. Os 4 de Queda
  Eterna saíram do meio da cidade (pedido do dono 2026-10-13); a lista da cidade está vazia até haver um lugar para eles.
- F10 > "Missões" mostra quantas foram concluídas; o clique lista as que faltam.

## Sem fonte

- Tempo de fabricação (receitas só têm CooldownSeconds; o port refina na hora).
- Tier atual de cada estação de Queda Eterna (estado de servidor).
- Capacidade base do armazém e kg por ponto de TB_GainStorage.
- Valores da taxa de estação e de propriedade (as do posto de trocas vieram em `sistemas-003`).
- Vendedor NPC: estoque e preço em ouro (lista e preços da loja são do port, `tools/shops.json`).
- XP base por abate: calculado no servidor do jogo (`sistemas-004`). As tabelas dão base baixa (inimigo 60 comum
  ≈ 69 XP); a regra de XP depois dos tutoriais é do port.
- Qual HouseType cada lote usa.
- Posição de cada NPC: depende de extrair o slice da cidade para `extract/slicejson`.

## Brindes dos NPCs (decisão do dono 2026-10-12)

Sem missões, o que os NPCs têm a oferecer:
- **Grupos por ofício:** no máximo 6 ofícios por cidade (guarda, minério e metais, alfaiate e caçador, taverna,
  alquimia e ervas, madeira e porto), com 3 a 4 NPCs cada (`tools/npc_rewards.json`). A cada semana, um NPC de cada
  grupo, sorteado igual para todos (CRC de cidade + ofício + semana), entrega naquela cidade: até 6 NPCs por
  semana. Os outros do grupo não têm nada.
- **Itens iniciais:** uma vez por personagem por ofício. Só materiais e consumíveis, em faixas (ex. 1000 minério,
  100–300 de cada argueiro).
- **Brinde semanal:** uma vez por semana por cidade e ofício: 1 material sorteado da lista do ofício (faixa de
  quantidade) mais 1 argueiro (100–300). Renova terça 08:00 UTC (05:00 BRT).
- **Marcador:** só no NPC sorteado. `icon_questavailable_npc` quando há algo a receber, `icon_questcooldown_npc`
  depois. O cartão diz "[E] Receber" (`ui_claim`) ou "[E] Conversar" (`ui_talk`).

Peças:
- Dados: `tools/npc_rewards.json` → `Data/World/npc_rewards.json` e campo `tipo` em
  `Data/World/queda_eterna_npcs.json` (`tools/make_npcs.py`).
- Regras: `FNWNpcRewards` (`NWNpcRewards.h`): período, sorteio do NPC, itens e estado do NPC.
- Servidor: `UNWInteractorComponent::ServerNpcInteract` confere distância (4 m) e estado, entrega pelo
  `Inventory->Grant` e grava no nw-core (`UNWBackend::ClaimNpcReward`, bolsa e moedas na mesma transação).
- O que já foi pego (`RewardClaims`) é carregado do nw-core e replicado só ao dono. Sem backend (PIE), fica na
  memória do servidor.
- Backend: migration 0007 `npc_rewards`. `npc` = ofício (inicial, período `once`) ou `<cidade>_<ofício>`
  (semanal, período = terça da semana).
