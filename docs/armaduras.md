# Armaduras e joias fabricadas (levantamento, esteira 4 item 2)

Levantamento de 2026-10-13. Regras decididas vão para `docs/itens.md`; aqui ficam os dados do jogo, o estado do port e
o plano.

## Jogo

### Receitas

- Armoraria (`javelindata_crafting_armorer`, ofício `Armoring`): 80 receitas T2–T5 (nível 0/50/100/150), 5 peças
  (cabeça, peito, mãos, pernas, pés) em 4 linhas:
  - placas (`Heavy*_Plate`): forja (`blacksmith2`), metal + couro + tecido;
  - couro (`Medium*_Leather`): estação de equipamentos (`outfitting2`), couro + tecido + metal;
  - tecido (`Light*_Cloth`) e vestido (`Light*_Dress`): estação de equipamentos, tecido + couro + metal.
- Quantidades (peito / cabeça / mãos / pernas / pés), material principal: 16 / 8 / 4 / 10 / 5 (placa), 22 / 11 / 4 /
  10 / 5 (couro e tecido).
- Joalheria (`javelindata_crafting_jeweler`, `Jewelcrafting`): amuleto, anel e brinco T2–T5. Cada um pede um
  componente (corrente, aro, gancho; feitos de lingote precioso), um engaste e metal precioso. Prata (T1), ouro (T2),
  platina (T3) e oricalco (`IngotT5`) no T5.

### Item

- `javelindata_itemdefinitions_master_crafting`: `ItemClass` (`EquippableChest+Armor+Heavy`), GS 200–300 ... 500–600,
  `ItemStatsRef`, `ArmorAppearanceM/F`, peso.
- Resistência (`javelindata_itemdefinitions_armor`): no jogo vem do GS × fração do slot × fator da classe.
  - Fração do slot (`ArmorRatingScaleFactor`): peito 0,35, pernas 0,20, cabeça, mãos e pés 0,15.
  - Fator da classe (`*ArmorSetScaleFactor`, variante Balanced): pesada 0,875, média 0,481, leve 0,265. As variantes
    Physical/Elemental trocam a proporção física/mágica (ex. pesada 1,05 / 0,70).
  - Peso da peça (`WeightOverride`): peito pesado 110,6, médio 62,4, leve 35,2.
- Joias: `TrinketBonus` e escala de vida (amuleto), dano (anel) e recarga (brinco) pelo GS; sem resistência e sem
  aparência (só ícone).

### Aparência

- `javelindata_itemappearancedefinitions`: por peça um `.skin` próprio (`Skin1`), material, ícone (`IconPath`, as
  armaduras não têm ícone no item) e flags `HideHead`, `HideHair`, `HideFacialHair`, `HideLegs`, `HairChop`.
- As 80 receitas usam 80 malhas distintas (só masculinas no port): conquistador T1–T3 e metal T5 (placa), militar,
  couro T1/T2 e conjunto A T5 (couro), tecido T1/T2/T4/T5, vestido básico e "feminine light" (vestido).
- Todas as peças usam o esqueleto do jogador (`player_male.chr`, 148 ossos).

## Port hoje

- Personagem: uma malha só (`SK_player_male`), com a armadura heroicheavy fundida no Blender (`tools/nwchar.py`
  `attach_armor`, `tools/export_ue.py`). Nenhum componente de armadura no C++.
- Inventário: `NWEquip` com 7 espaços (armas e ferramentas). Os espaços de armadura e joia aparecem só como ícones
  vazios (`NWInventoryMenu.cpp` `CombatEmpty`), sem clique.
- nw-core: `MAX_EQUIP_SLOTS = 16`; sobram 9 índices (7–15), o bastante para 5 peças + amuleto, anel e brinco sem
  migração.
- Regras já decididas (`docs/itens.md`): um atributo, qualidade, perks por equipamento (armadura, amuleto, anel,
  brinco), 1 slot de gema na armadura (gemas de armadura prontas), joias sem slot, anel com 2 perks.
- Tela de resultado: falta o painel `Stats/Armor` (físico e mágico) e o `Stats/Trinket` (só divisor)
  (`re-local/craftroll-equipamento.md` §6).

## Plano

1. Itens e mecânica (sem visual): 60 receitas de armoraria e 12 de joalharia, ofício Armoring (índice 13), espaços de equipamento
   7–14 (cabeça, peito, mãos, pernas, pés, amuleto, anel, brinco), clique e arrasto nesses espaços, resistência no
   tooltip e no painel `Stats/Armor`. O personagem continua com o visual atual.
2. Visual: exportar o corpo nu e cada peça como SkeletalMesh separado no mesmo esqueleto; no C++, um
   `USkeletalMeshComponent` por slot com `SetLeaderPoseComponent` no corpo, malha nua escondida quando o slot está
   ocupado, flags de cabelo e cabeça. Mexe no `NWCharacter` e no conjunto do jogador (compartilhados): combinar com as
   outras esteiras.
3. Efeito da resistência e da classe de peso no combate: com a esteira 1 (item 3 da esteira 4).

## Decisões (2026-10-13)

Registradas em `docs/itens.md` "Armaduras e joias": placas, couro e tecido (sem vestido); joias direto do lingote
precioso; resistência pela proporção do jogo, física = mágica; mecânica antes do visual (fase 1 do plano primeiro).

## Estado (2026-10-13, fase 1 feita, sem visual)

- Receitas: `tools/make_crafting.py` (`CRAFT_GEAR` com `(Plate|Leather|Cloth)(Head|Chest|Hands|Legs|Feet)` e
  `Amulet|Ring|Earring`; material fixo do tier; joia soma o lingote dos componentes). Placas na forja, couro, tecido e
  joias na estação de equipamentos. Ofício Armoring = índice 13 (`NWTradeskill`, ícone `icon_tradeskill_outfitting`).
- Catálogo: `tools/make_items.py` dá a armadura e joia o espaço (`set` Head…Earring), a classe de peso (`armor`) e o
  ícone da aparência.
- Espaços `NWEquip` 7–14 (cabeça, peito, mãos, pernas, pés, amuleto, anel, brinco) no painel do inventário, nas
  posições do jogo; `NWEquip::IsTool` separa as ferramentas. GS médio do painel conta armas, armadura e joias.
- Regras: `FNWItemRules::EquipClass` (armadura, amuleto, anel, brinco), `Resistance` (`tools/itens.json`
  `resistencia_peito`, `resistencia_slot`); tooltip com resistência física e mágica; raridade sorteada (não pelo tier).
- Tela de resultado: painel `Stats/Armor` (base com qualidade 0 → valor do item, Físico e Mágico) e `Stats/Trinket`.
- Dev (F10): "Armaduras e joias (níveis e materiais)".
- Capturas: `tools/ui_shots/armaduras.json` (receita e rolls) e `tools/ui_shots/vestuario.json` (painel e tooltips).
- Falta: visual (fase 2) e efeito no combate (fase 3). A descrição longa de perk de habilidade (ex. Ventos Curativos)
  passa da linha na tela de resultado.

