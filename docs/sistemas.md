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
  atributo), GearScore (sorteia de novo na faixa da fabricação, sobe ou desce) e Unbind. Custo provisório em
  `FNWCrafting::ImproveCost` (`tools/itens.json` `_custos`).
- Resultado: `ClientCraftResult` (item antes e depois) → `UNWInteractorComponent::CraftResult`; a tela espera o `Seq`
  e mostra o roll de equipamento do jogo (`FNWRefiningMenu::DrawGearRoll`, `docs/analise-re/craftroll-equipamento.md`).
- Item: `FNWItem` com atributo, qualidade, perks, gema, vínculo e assinatura (`props` no nw-core, migração 0008);
  regras em `Data/Items/regras_itens.json` (`tools/make_itens.py`, `tools/itens.json`, `tools/perks_decisoes.json`).
  Tooltip com perks com ícone e o slot de gema (moldura do soquete).
- Captura: `tools/ui_shots/forja.json` (receitas e o roll no meio e no fim). Dev (F10): "+50 níveis nos ofícios de
  fabricar" e "Materiais de fabricar".

Tela (2026-10-13): `craftingscreenv4` no modo refino com estilos e posições do Lua (`docs/hud/refino.md`,
`tools/ui_screens/refining.json`), conferida pela captura sem o jogo (`tools/ui_shots/refining.json`) e testada pelo
dono. O que falta na tela: `docs/hud/README.md`.

## 3. Armazém

Jogo: slice `settlement_storage_geo_master` (`@ui_global_storage`). Crescimento por standing do território:
`DT/javelindata_territory_standing` (301) → TB_GainStorage em `DT/javelindata_progressionpointdata`;
`DT/javelindata_categoricalprogression` id 4 = Everfall (MaxLevel 300).

Capacidades (`javelindata_encumbrancelimits`, décimos; `docs/analise-re/hud-inventario.md`): `StorageSmall` 150,
`StorageMedium` 1000, `StorageLarge` 1500, `StorageXLarge` 2000 kg. Tela do jogo: `container/catcontainer.lua`.

Port (esteira 4):
- 2 despensas de Queda Eterna, lidas do slice (`settlement_storage_geo_master`, nome `ui_global_storage`), entram
  como estações do tipo `storage` (`make_crafting.py`). O cartão é "[E] Abrir".
- Servidor (`UNWInteractorComponent`, `ServerStorageOpen/Move/Close`):
  - Um armazém por personagem e cidade: `settlement:queda_eterna`.
  - Capacidade de 1000 kg por enquanto (`StorageMedium`; o acréscimo por standing é a pergunta `sistemas-002`).
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

Port: backend pronto (migration 0005, rotas em `backend/README.md`). O servidor de jogo ainda não chama: falta
calcular `fee` e `duration_hours` com essas regras, recusar outras durações e cobrar a taxa de quem completa.

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
