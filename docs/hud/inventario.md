# Inventário, equipamento e armazém (spec da tela)

Fontes: scripts de interface do jogo (`extract/lua/lyshineui/`, decompilados) e canvases (`extract/ui/lyshineui/inventory/newinventory`,
`equipment/equipmentv3`, `container/catcontainer`, achatados com `tools/ui_canvas.py`). Prova: **C** = código Lua, **R** = canvas,
**H** = hipótese. Coordenadas em 1080p; o `ui_canvas.py` desenha os contêineres de tela destas telas 490 px acima, já
somados aqui. Prints do original: `inspiracao/02`–`10`. Regras de transferência e teclas: `re-local/hud-inventario.md`.

Quase tudo destas telas é montado pelo Lua (peças, seções e espaços são slices instanciados); a prévia do canvas sozinha
não mostra a tela.

## 0. Janela do inventário (R)

- `Content` x 966–1589 (com `SetLocalPositionX` 300; 0 quando o armazém abre, ver §4). Moldura `FrameMultiBg` 988,62
  587×932 com `headerbg.dds`. Ícone `icon_inventory.dds` 990,55; título 1058,56.
- Busca 1002,113 360×45 (`primarysearchbg.dds`); filtros 1367,113 45×45; 4 botões de ordenação 34×45 de x=1421 a 1523.
- Lista 1002,184 573×742. Moedas no rodapé y=942.
- Zona de largar no chão `GroundDropTarget` à direita da janela (x 1576–1899 com o inventário sozinho).

## 1. Peça e arrasto (itemlayout.lua, itemdraggable.lua, popup.lua, common.lua)

- Modos (itemlayout.lua 165-187): EQUIPPED=1, CRAFTING=2, CONTAINER=3, INVENTORY=6, TRADING_POST=7, EQUIPMENT_WEAPON=9, P2P=13, LOADOUT_EQUIPPED=16.
- INVENTORY/CONTAINER: raiz e ItemTier escala 1.06 (4253-4274, 4418-4439); EQUIPPED/EQUIPMENT_WEAPON ligam ItemFrame (4071-4109, 4354-4372).
- Highlight por layout: itemHighlightLarge/LargeWeapon/Circle/Square.dds (206-209). Lock salvagelock.dds (216). Novo: spritesheetnewitem{rectangle|square|circle}.dds 1,5 s; dano: spritesheetdamageditem* (218-226).
- SetLayout (2532-3776): quantidade some no SQUARE com qtd ≤ 1 (2797-2832); no RECTANGLE não há quantidade; CIRCLE mostra. Durabilidade só RECTANGLE ou SQUARE de armadura. Ícone = peça − 6 px (3085-3101); ItemRarityBg largura w − 1.
- Ícone vermelho #FF7171 se o jogador não cumpre requisito (3000-3029).
- ItemSortValueText (OnFilterChange 7128-7245): ordenar por peso → ícone icon_weight (scale 0.7) + peso unitário /10 com 1 casa, tier some; por gear score → icon_filter_gearscore (scale 1.2, yOffset 4) + GS se > 0, tier some; outros → texto some, tier volta.
- Tier: texto romano (GetRomanFromNumber); some em CRAFTING.
- Dimmer (6140-6198): ItemDimmer opacidade 0.65 em 0.3 s QuadOut quando não usável; fill linear de baixo (cooldown).
- Highlight (EnableHighlight 1199-1265): 0→1 em 0,1 s QuadOut, segura 0,1 s, pulsa 1↔0,5 (0,3 s / 0,35 s, hold 0,05 s; common.lua CreatePulse 491-678). Desliga em 0,1 s QuadIn. Só se arrastável.
- Tooltip (itemdraggable): 0,25 s depois de rolar ou 0,1 s depois de soltar; mouse lento (vel² < 10000) por 0,1 s; fade 0,05 s (instantâneo) ou 0,15 s; ancorado ao cursor (10), à direita.
- Arrasto (OnDragStart 3124-3741): canvas própria draw order 999; clone 75×75 (Properties.DraggingSize) centrado no cursor, sem opacidade; origem com fade 0,5 (3476); destino recebe Paperdoll.EquipmentSlotsToHighlight. OnDragEnd (3768-4005): origem volta a 1, sem animação. Drop inválido se o cursor não se moveu.
- Ctrl (split): no OnReleased chama InvokeStackSplitter (625-739) → popup @ui_split_stack_hint (Split_Stack_Popup). Popup (popup.lua ShowCustomPopup 3883-4040): não abre com qtd ≤ 1; slider 1..qtd, inicial q − floor(q/2); mostra fica/leva e pesos; dois ItemLayout escala 1,25; botões Cancelar (@ui_cancel), Largar/Guardar (@ui_tooltip_drop / @ui_move / @ui_tooltip_send / @ui_tooltip_take), Equipar (@ui_tooltip_equip, desabilitado @inv_cannotequipitem).
- Shift + clique: OnStoreItem (quick move). Alt: link. Clique simples: abre/trava o flyout de ações (2583-2641). Duplo clique: não existe no itemdraggable.
- Sem moldura "selecionado" (itemframeselected não é usado).

## 2. Lista do inventário (DIL=dynamicitemlist, CS=catsection, IH=inventoryheader, IF=inventoryfilter, WB=weightbar, NI=newinventory)

### Seções (DIL OnInit 505-737, defaultSectionInfos; título ItemCommon:GetItemClassName, _common/itemcommon.lua 526-603)
- Esquerda: OutpostRush, PvPArenas, Catacombs, Rewards (owg_rewards_title), Weapon (ui_weapons), Armor (inv_apparrel), PatternItem, Runes, MountItem, Consumable (inv_utilities "Utilities"), BasicFood, AttributeFood, TradeSkillFood, Cooking, Bait (inv_baits), CraftingModifiers, Material (inv_resources).
- Direita: Tools (inv_tools), Instruments, TuningOrbs, Charms, JewelCrafting, Refining (inv_refining "Reagents"), Smelting, Leatherworking, Weaving, Woodworking, Stonecutting, Alchemy (inv_alchemy "Arcana"), Dye, Furniture, Quest, Lore.
- Item entra na seção se slot:HasItemClass(itemClass) (CS DoSlotUpdate 1233-1244). Seção vazia some (SetHiddenWhenEmpty).
- Colunas: padrão 2 (DIL 267); itens por linha = 8 / colunas (4 ou 8); célula 60×60, HorzSpacing 2, VertSpacing 5 → passo 62×65.
- Seção: cabeçalho 50 px; lista começa no fundo do cabeçalho + ListMargin 10, +41 em 2 colunas (twoColumnOffsetY) → lista em y=101 da seção em 2 colunas; altura = ceil(n/porLinha)×65; sem espaço entre seções; masonry: cada coluna empilha as suas; coluna direita em âncora X 0.5147; em 1 coluna desloca x=13; largura do cabeçalho 538/colunas.
- Rodapé de itens filtrados (25 px): @ui_filtereditems.

### Cabeçalho (IH, CS)
- Título FONT_STYLE_INVENTORY_SECONDARY_TITLE: Pica 27, #C6B694, spacing 30, sombra, maiúsculas. Peso FONT_STYLE_INVENTORY_SECTION_WEIGHT: Pica 27 #B2AEA1, "%.1f" de peso/10, com icon_weight.dds 12×12 #B2AEA1. Linha #FFF1CE.
- Seta: dropdownarrowwhite.dds 18×18 #C6B694 (foco #EBE3D6), sem fundo, rotação 0 aberto / −90 recolhido em 0,2 s QuadOut; botão 30×30 em (−8,18,22,48).
- Botões à direita (da direita p/ esquerda): TransferAll (icon_smallArrowNarrow.dds, scaleX −0,5 no inventário "@ui_dropall"/"@ui_storeall", +0,5 no armazém "@ui_takeAll"), RepairAll (oculto no inventário e armazém), OpenSalvage (icon_tooltip_salvage.dds). Passo 34; em 2 colunas, botões e peso vão para uma 2ª linha (+41).
- Arrastar seção: DragHighlight button_simple.dds #646464, 0,8 na arrastada / 0,2 nas outras.

### Ordenação (IF 823-882)
- CHRONO=1 (padrão, mais novo primeiro), WEIGHT=2, GEARSCORE=3, TIER=4; clicar no ativo inverte a direção. Salvo em Options.Misc.InventorySortMode/Direction.
- Dentro da seção: contêineres primeiro, depois a chave (maior primeiro), empate por chegada.
- Botões 34×45: icon_filter_chrono.dds 33×33, icon_weight.dds 22×20, icon_filter_gearscore.dds 30×30, icon_filter_tier.dds 30×30; cor #A5987D, selecionado/hover #EBE3D6 (0,1 s); barra 34×2 #EBE3D6 embaixo do selecionado.
- Tooltips @ui_sort_chrono ("Sort by Time: Newest - Oldest") etc.

### Busca e filtros
- Busca 360×45, placeholder @ui_searchitem "Search" Nimbus médio 25 #B2AEA1, icon_magnifying_glass.dds; "?" com @ui_filterDescription.
- Botão filtros icon_mapfilter.dds; selo amarelo #FFDB7A com nº de seções visíveis quando < total.
- Popup: fundo 0→0,8 em 0,2 s; caixa y −10→0 e 0→1 em 0,1 s; "Columns" (1/2), "Filters" Equipment/Materials/All, lista de seções com checkbox, coluna (filteraligntoggle_left/right.dds), subir/descer.

### Peso no alto (WB, NI)
- "888.7" FONT_STYLE_WEIGHT_VALUE_EQUIPMENT_2023 Pica 28 #EBE3D6 + "/ 1476.4" Pica 28 #B2AEA1; ícone icon_weight.dds 16×16; acima de 100%: #FF7171 e pulsa 0,4↔1; texto "Encumbered" Pica 23 #FF2E2E.
- No inventário a barra horizontal fica desligada: aparece o radial encumbrance/encumbrancefill.dds 68×68 em volta do ícone do inventário.
- Sobrecarga: EncumberedHighlight #B42F0E a 0,35 em 0,3 s.

### Rodapé e título
- Moedas: GetLocalizedCurrency, Pica 28, icon_crown.dds 16×18, x≈1005; azoth/umbral fora no port.
- Título @ui_inventory: Pica 36 #EBE3D6 spacing 100 sombra maiúsculas; icon_inventory.dds 68×49 #EBE3D6; LineHorizontal #FFF1CE a 0,7.

## 3. Equipamento (equipmentv3.lua, equipmentitemdroptarget.lua, weightbar.lua). Retângulos [x,y,w,h] em 1080p com +490; X do canvas (o Lua move: contentPosX −330, contentPosXLeft −630, viewPosX 30).

### Espaços (todos 63×63; ItemBg equipment/equipmentbg.dds 70×69 9-slice vazando 5 px à dir./baixo; ícone vazio a 0,20; trava equipmentlock.dds 14×18 #aaa; item com escala 1,06)
- Armadura, coluna esquerda x=30, passo 68: cabeça 216 (iconhead), peito 284 (iconchest), mãos 352 (iconhand), pernas 420 (iconlegs), pés 488 (iconfeet), escudo 556 (iconshield). Fundo ArmorBg [22,202,98,436] equipmentstatsbackdrop.dds a 0,5.
- Coluna direita x=548: arma 1 216, arma 2 284 (iconweapon), gema do coração 352 (iconrune), amuleto 438, anel 506, brinco ("token") 574. Fundos WeaponsBg [542,205,80,226], TrinketsBg [542,427,80,226] a 0,5.
- Consumíveis rápidos 2×2: [30,664] [100,664] [30,732] [100,732] (iconquickslot); fundo [20,656,155,151] a 0,5.
- Aba ferramentas: instrumentos à esquerda (216..488), bolsas 592/660/728 (iconbag); à direita x=548: corte 351 (iconlogging), mineração 419 (iconpickaxe), esfola 487 (iconskinning), colheita 555 (iconsickle), pesca 623 (iconfishing), cajado de azoth 728. Fundos ToolsBg [540,338,82,366].
- Aba montaria: buffs [30,379] [30,447], acessórios [30,557] [100,557], botão de seleção [28,270,70,71] mountframe.dds.

### Abas: ViewsTabbedList [-2,74,170,56], botões 55×55 com ícone 64×64, margem 9; equipment/tab{WeaponsArmor|Tools|Mounts}{Selected|Unselected}.dds; troca 0,05 s; hover pulsa 0,65↔1.

### Carga (weightbar): texto "Light/Medium/Heavy/Over-Equipped" [29,144] Nimbus médio 25 (verde #30e5ab / #fff1ce / #ffdb7a / #ff7171, 0,3 s); ícone icon_weight_dropshadow.dds [27,174,20,24]; valor [49,168] Pica 28 #ebe3d6; fundo [19,145,100,65] backdrop2023 a 0,15.
- Barra vertical 405×5 (x 105–110, y 217–622), fundo tilingbarbg.dds, preenchimento vitalsbarfill.dds #9e947e de baixo para cima, moldura 1pxframe #8f8473; máximo 50.0 no topo [126,209]; divisores weightbardivider.dds 2×7 #e2cc99 em 13.0 e 23.0 (razões das cargas); cor do segmento atual (verde/creme/amarelo/vermelho).

### Nível e gear score (direita): nível [576,114] Pica 48 #ebe3d6 com reward_xp.dds 48×48 [524,103], glow preto 0,3 atrás; gear score [553,158] Pica 48 com icon_gearscore_dropshadow.dds 40×40.

### Atributos e resistências: fundo [10,810,174,202] backdrop2023 a 0,29; rótulo x=32 Nimbus médio 25 #c6b694 (@ui_*_short), valor x=106 Pica 28 #ebe3d6; linhas STR 822, DEX 846, INT 870, FOC 894, CON 918, PHYS 950, ELEM 974. Aba ferramentas: níveis de ofício a cada 24 px a partir de 823 com iconstat*.dds 48×48.

### Sem painel/moldura: só fundos pretos translúcidos por grupo.

### Arrasto: os espaços compatíveis acendem com icon_item_selected.dds 70×69 branco (vermelho #ff2e2e se travado); incompatíveis não escurecem. Erro: pulso vermelho 2×. Alvo universal (todo o lado do equipamento): dropzoneglow.dds a 0,1 (0,2 vermelho se não equipável), 0,15 s; soltar equipa no melhor espaço (EquipItemToBestSlot).

## 4. Armazém e alvos de soltar (catcontainer.lua, newinventory.lua, equipmentv3.lua, *droptarget.lua). y já com +490.

### Armazém (catcontainer SetContainerMode 1821-2593, CONTAINER_MODE_STORAGE)
- Janela: largura 623, X -15 (âncora/pivô à direita) → x 1282..1905. FrameMultiBg x 1304..1891, y 63..995; rodapé FooterBg 68 px (y 927..995) preto a 0,5; FrameHeader altura 121 (y 63..184); LineHorizontal2 598 px em y≈908, #fff1ce a 0,7. Entrada: fade 0,3 s QuadOut; saída 0,2 s.
- Título: PrimaryTitle x 1374, y 57..97, Pica 36, branco, maiúsculas; texto = Hud.LocalPlayer.InteractName (padrão @inv_Storage "Storage"; "Storage Shed" = @ui_global_storage).
- Ícone: lyshineui/images/catcontainer/icon_storage_shed.dds, #ebe3d6, x 1321 y 59, 36×36. Pode pulsar vermelho #ff7171 (provável: espaços cheios). Tooltip @ui_inventoryslots_tooltip.
- Peso: WeightBar Set{x=36,y=100} largura 556 → x 1318..1874; radial encumbrance/encumbrancefill.dds 68×68 centrado no ícone (~1339,77). Valor "%.1f" Pica 33 branco + "/ %.1f" Pica 24 #c8c8c8, à direita (valor x≈1763..1810, máx x≈1814..1865, y≈61..92). Overage 3%, texto @ui_storage_is_full.
- Lista: mesmo DynamicItemList, Set{x=36,y=185,w=573,h=742} → x 1318..1891, y 185..927, com seções. Barra de busca/ordenação 63 px acima (y 122..152): busca 360×45 + 4 botões 34×45.
- Seletor de assentamento (só armazém pessoal): x 1317..1695, y 935..985; buttonbg.dds #2c251b, primarybuttonframe.dds, dropdownarrow.dds; Nimbus 32 branco; "Nome <font color=#edc63e>(@ui_current_location)</font>"; ícone de facção 50 px; lista abre para cima, 560 de largura.
- Modo bolsa de saque (CONTAINER_MODE_LOOT_BAG, 1821-1990): Content largura 340, X −7; WeightBar 246; título y 57 w 270; FrameHeader 183; lista Set{x=41,y=246,w=284,h=681} sem seções, sem SortAndFilterBar; ícone icon_dropped_items.dds #COLOR_TAN_WARM; ButtonTakeAll x 4, texto @interact_takeAll; LootBagInfoContainer (canvas, já com +490/−298): LifeTimeWarning x 1324..1585 y 104..184, @inv_discard_life_warning, Nimbus 23 #c8c8c8, quebra, entrelinha −4; ItemsText x 1324..1424 y 201..251, @ui_loot_bag_items / @ui_loot_bag_1_item; LootBagDivider. Título = InteractName (port: @inv_DroppedItems).
- Abas do armazém compartilhado (port, dono 2026-10-14; o jogo não tem): 5 botões buttonbg #2c251b + primarybuttonframe
  em y 174..212 entre a busca e a lista (lista a partir de y 222), largura dividida com 6 px de espaço; aberta com a
  moldura cheia e o texto #ebe3d6, as outras a 0,4 e #b2aea1; Nimbus médio 22; arrastando, a aba sob o mouse acende;
  clique duplo abre um campo do Slate por cima para o nome. Captura: `inventory.json` `armazem_aba2`, `arrasto_aba`.
- Pegar tudo: ButtonTakeAll 170×50, x 1707.5..1877.5, y 935..985, @interact_takeAll, Pica 32; buttonbg #2c251b, heroframedisabled2023, heroborder aditivo, focusnew a 0,1. Callback ContainerTakeAll (3715-3778).

### Inventário e equipamento com o armazém aberto
- newinventory OnTransitionIn (~4405-4430): Content X = 0 com armazém (sozinho = 300). Inventário x 648.5..1271.5 (sozinho 948.5..1571.5). Sem tween de posição, só fade 0,5 s.
- Equipamento continua visível, 300 px à esquerda: equipmentv3 contentPosXLeft −630 (normal −330); ArmorContainer 120 (normal 25); TradeSkillStats/MountSkillStats 107 (normal 12).

### Peso na peça
- Só ao ordenar por peso (inventário e armazém, cada lista com sua ordenação): ItemSortValueText no canto superior esquerdo, Pica 24 branco.

### Alvos de soltar
- Área do contêiner/inventário: dropzoneglow.dds a 0,1 (9-slice), acende só no hover sobre o alvo: 0→1 em 0,15 s QuadOut; apaga em 0,25 s. Inventário: x≈652..1273, y 41..1020 (com armazém); armazém x≈1285..1908.
- Chão (grounddroptarget): só durante o arrasto de item do inventário/equipamento e só sem contêiner aberto; aparece em 0,3 s; texto @inv_dropTargetMessage "Drop items" Pica itálico 30 #dddddd, entre dropitemtrim.dds 220×27 (em cima e invertido embaixo); com o inventário sozinho fica em x 1576..1899. Soltar: item vinculado pede "Destroy Item?"; travado/missão não larga. Com armazém aberto, o "largar tudo" vira @ui_storeall.

## 5. Para o port

Feito em 2026-10-13 (`NWInventoryMenu`, captura `tools/ui_shots/inventory.json`, testado pelo dono): itens 1 a 7
abaixo, mais busca por nome (campo do Slate por cima do canvas, sem maiúsculas nem acentos), largar no chão (destrói,
com confirmação; o port não tem saco de saque; `ServerDestroy` com quantidade) e recorte das peças na rolagem.
Diferenças: cabeçalho da seção mais baixo que o 101 do Lua (medido no print 02); alvo de soltar é uma borda (o
`dropzoneglow` é 9-slice); sem cadeado, menu de ações, Equipar no diálogo, seletor de assentamento nem janela de
filtros; "chegada" usa o número do espaço (o port não guarda a hora). Prioridade do dono: mecânicas antes de refinar
filtros.

Plano original, em ordem:
1. **Peça 60×60** quadrada ou redonda pelo tipo (redonda: recurso, lore, tintura, item de casa), camadas do `itemlayout`
   (fundo e moldura por raridade, ícone − 6 px, tier romano, quantidade só > 1, cadeado), destaque que pulsa no foco.
2. **Seções do jogo** pela `UiItemClass` do item (`make_items.py` grava `ui_class`), duas colunas em masonry, 4 peças por
   linha, passo 62×65, cabeçalho com seta, título, peso e botão de mover tudo; seção vazia some; recolher.
3. **Ordenação** chegada/peso/gear score/tier com os ícones do jogo, clicar de novo inverte, valor no canto da peça (peso
   ou gear score). Busca com a sintaxe da RE. Janela de filtros fica para depois.
4. **Equipamento sem painel**, nas posições do `equipmentv3` (só o que o port tem: armas 1 e 2, escudo, consumíveis
   rápidos; ferramentas na aba de ferramentas), espaços 63×63 com `equipmentbg.dds`, fundos pretos translúcidos por
   grupo, abas em ícone, nível e gear score no alto, atributos embaixo. Armadura, joias, montaria e carga de equipamento
   ficam fora até existirem.
5. **Arrasto do jogo**: cópia 75×75 no cursor, origem a 0,5; espaços compatíveis acendem com `icon_item_selected` (sem
   escurecer os outros); área do contêiner/inventário acende só com o mouse em cima (`dropzoneglow`); zona "Largar itens"
   ao arrastar sem contêiner aberto.
6. **Dividir pilha** pelo diálogo do jogo (Ctrl + clique; barra que começa na metade; fica/leva com peso; Cancelar,
   Largar/Guardar, Equipar). Sai o "Ctrl leva metade" e o duplo clique (o jogo não tem); clique abre o menu de ações
   depois.
7. **Armazém como segunda janela** à direita (x 1282–1905), inventário em x 648–1271, equipamento 300 px à esquerda;
   título, ícone, peso com o anel, mesma lista com seções, "Pegar tudo" (precisa de ação nova no servidor) e seletor de
   assentamento (só Queda Eterna por enquanto).

## Sem fonte

- Tamanho e posição final do X do equipamento depois dos `SetLocalPositionX` do Lua (conferir com o print 02).
- Textura `icon_item_selected.dds` não achada na extração: conferir o caminho no pacote.
- Condição exata do ícone do armazém pulsar vermelho (decompilação ambígua; provável: espaços cheios).
