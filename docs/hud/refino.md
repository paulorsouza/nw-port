# Tela de fabricação no modo refino

Como `craftingscreenv4` aparece numa estação de refino (Fundição, Tanoaria etc.). Fontes: Lua decompilado em
`extract/lua/lyshineui/crafting/` e `_common/`, canvas `extract/ui/lyshineui/crafting/craftingscreenv4.dynamicuicanvas.json`
convertido por `tools/ui_canvas.py`. As coordenadas são do layout convertido (1920×1080).

Abreviações de caminho:

- `S` = `_root/CraftingScreen/CraftingContainer`
- `CP` = `S/CraftingPanel`
- `SP` = `CP/CraftingStatsPanel`, `RP` = `CP/CraftingRecipePanel`, `TV` = `CP/CraftingTreeView`

O vínculo propriedade Lua → elemento foi tirado dos componentes de script do canvas (nomes das propriedades são CRC32
do nome em minúsculas). Resultado nas tabelas abaixo.

## 1. O que liga e desliga no refino

O modo é decidido por receita, não pela estação. `craftingstatspanel.lua:SetCraftingParams` faz
`isCraftAll = IsRecipeCraftAll(id) or IsRecipeRefining(id)` e, com `isCraftAll` verdadeiro (e `GetSkipGrantItems` falso):

| Liga | Desliga |
|---|---|
| `SP/Refining` (propriedade `RefiningInfo.Panel`) | `SP/QualityComplex/QualityBar` (gear score bar) |
| | `SP/QualityComplex/TopInfo` (rótulo e faixa de gear score) |
| | `SP/StatsMasterContainer/StatsScrollBox/StatContainer` (stats, perks, atributo, gema, azoth) |
| | `SP/OffsetContainer/AchievementRecipeDesc` (`CampSkinText`) |

`UpdateStatsInfo` (que clona linhas de stat/perk/atributo) não roda nesse modo.

Outros desligamentos no refino:

- `RP/Modifiers/AddPerkItem`, `AddPerkItem2..4`: ligados só se `isProcedural and canHavePerks`. Refino não é procedural → todos desligados.
- `RP/Modifiers/AddAzoth` (`CurrencyIngredient.Panel`): ligado só se procedural e `perkUpgradeCost > 0` → desligado.
- `RP/Modifiers/NoPerksText`: ligado (`showNoPerksText = true`). Texto `@crafting_no_bonus` = "Craft mods cannot be added to this item."
- `SP/Refining/GlobalBonus`: ligado só se a habilidade é de refino e há buff de facção `Crafting_Refining_Yield_Modifier > 0`.
- `SP/OffsetContainer/CooldownCount`: ligado só se a receita tem cooldown.
- `SP/OffsetContainer/RequiredStats`, `RP/PerkItemSelector`, `RP/CategoryItemSelector`, `RP/OptionalResourceSelector`, `RP/ResourceSelectorBlocker`, `CP/CraftingTreeFilterPopup`: desligados no canvas; só abrem por ação.
- `S/MultiCraft`, `S/CraftRoll`, `S/CraftDone`, `S/CraftingSummaryList`, `S/SummaryHeaderContainer`: desligados no estado de seleção. `S/CraftingSummaryListFrame` é desligado em `OnTransitionIn`. `S/BlackBg` fica com opacidade 0 no estado `STATE_CRAFTITEM`.
- `SP/HeaderTooltip` é só área de hover (sem imagem).

Prefixos a ligar no estado de seleção: `S/../BackgroundStep1` (opacidade 0), `TV`, `RP` (menos Modifiers/AddPerk*/AddAzoth e seletores), `SP` com `SP/Refining`, `SP/MainDivider`, `SP/LineLeft`, `SP/StatBg`, `SP/PinRecipeButton`, `SP/OffsetContainer/{Quantity,CraftingRequirements,CraftButton,TaxRate,SkillExpReward}`.

## 2. Áreas, dados e cores

Cores do `_common/uistyle.lua`: `COLOR_TAN_WARM #ebe3d6`, `COLOR_GRAY_NEW #b2aea1`, `COLOR_GRAY_50 #808080`,
`COLOR_GRAY_70 #aaaaaa`, `COLOR_GRAY_80 #cccccc`, `COLOR_GRAY_95 #e7e7e7`, `COLOR_TAN #9e947e`,
`COLOR_RED_MEDIUM #f25d5d`, `COLOR_GREEN #30e5ab`, `COLOR_YELLOW #ffdb7a`, `COLOR_BLUE_BRIGHT #63d5ff`.
`COLOR_INSUFFICIENT_QUANTITY = COLOR_RED_MEDIUM`. `COLOR_RARITY_GRAY = COLOR_GRAY_80`.

Caixa alta: o conversor não exporta `caseoverride`. No canvas têm `caseoverride = 2` (maiúsculas), entre outros:
`RP/Header/ItemName`, `RP/Header/ItemTier`, `SP/Refining/Label`, `RequirementsHeaderLeft`, `FrameHeaderText`, `HintText`,
`CraftRoll/.../ItemName`, `BonusLabel`, `BaseAmountLabel`, `ContinueText/Text2`. Estilos Pica do Lua (`FONT_STYLE_PICA_CORE`)
também forçam maiúsculas, espaçamento 150 e sombra.

### Painel central (`RP`, 636,151 715×860)

| Elemento | Dado | Formato / cor |
|---|---|---|
| `RP/Header/ItemName` | nome do item resultado (`recipeData.name` ou `displayName`); se saída >1, acrescenta ` x{n}` (`@ui_quantitywithx`) | nimbus_bold 36, maiúsculas; cor da raridade (`COLOR_TOOLTIP_RARITY_LEVEL_n`) ou `#cccccc` se o item não usa raridade |
| `RP/Header/ItemName/Description` | `LocalizeWithDataSheetData(item.description)` → "Crafting Material." | nimbus_regular_italic 23, `FONT_STYLE_FLAVOR_TEXT` |
| `RP/Header/ItemRarityBg` | `crafting_itemraritybglarge{0..4}.dds` (índice = raridade) | cor branca |
| `RP/Header/ItemRarityBg/ItemIcon` | ícone hi-res do item | 312×140 |
| `RP/Header/ItemTier` | `"@cr_tier " + <img crafting_tier_{N}.dds scale=0.75>` → "TIER II" | pica_italic 24; Y=-20 quando o item não usa raridade |
| `RP/Header/ItemRarity` | "@RarityLevel{n}_DisplayName" | desligado se o item não usa raridade |
| `RP/Header/HeaderRuneMask/HeaderRune1..3` | `headerrune1..3.dds`; Rune2 gira 0→359°, Rune3 0→−359°, 80 s por volta, loop | emblema circular atrás do ícone |
| `RP/RecipeRuneMask/RecipeRune1..3` | `reciperune1..3.dds`, mesmas rotações (80 s) | fundo da área de ingredientes |
| `RP/Header/ResourceSourceNotification/ResourceOriginText` | `@ui_crafting_resource_origin_message` | nimbus_regular_italic 20; ícone `icon_exclamation_white_small.dds` `#ffda7a`; linhas `#666666` acima/abaixo |
| `RP/IngredientDivider` | linha vertical 1×496 em x=1019 | `#666666` |
| `RP/Modifiers/NoPerksText` | `@crafting_no_bonus` | 1034,466 284×100, `FONT_STYLE_CRAFTING_NO_PERKS` |
| `RP/Header/ItemName/BonusHintLayout` | dica de bônus de primeira fabricação | só se a conquista de primeira fabricação não foi obtida |

`SP/PinRecipeButton` (691,317 240×42, buttonsimple) fica sobre o cabeçalho central: `@ui_pin_recipe` / `@ui_unpin_recipe`.

### Painel direito (`SP`, 1350,151 490×860; fundo `crafting_panel_infobackground.dds` + `SP/StatBg` `craftingstatbg.dds`)

Bloco de refino `SP/Refining` (1350,189 380×545):

| Elemento | Dado | Formato |
|---|---|---|
| `SP/Refining/Label` | `@cr_bonusPossibility` = "POTENTIAL BONUS" | nimbus_medium 24, maiúsculas, 1391,172 |
| `SP/Refining/Skill/LevelBg` | `icon_skilllevelbg.dds` 92×92 | círculo do nível |
| `.../Skill/LevelBg/Level` (`SkillLevel`) | nível atual do ofício (`GetRank`); se ofício é `WildernessSurvival`, nível do personagem+1 | inteiro sem formatação, pica 40 |
| `.../Skill/Description` (`SkillName`) | `"@ui_" + tradeskill` → "Smelting" | nimbus_medium 25 `#8a7c5e` |
| `SP/Refining/Bonus/LevelBg` | `crafting_refiningcircle.dds` 92×92 | |
| `.../Bonus/LevelBg/BonusAmt` (`BonusAmount`) | `GetChanceForBonusItems(recipe, ingredientes)` | `"%.0f%%"` → "30%", pica 34 |
| `.../Bonus/Description` | `@ui_skill_bonus` = "Chance to craft additional items" | nimbus_medium 25 `#8a7c5e` |
| `.../GlobalBonus/LevelBg/BonusAmt` | buff de facção ×100 | `"+%.0f%%"`; texto `@ui_crafting_refining_yield_modifier_name` |

Linhas Y no canvas: Skill 245, Bonus 331, GlobalBonus 417 (passo 86).

Requisitos `SP/OffsetContainer/CraftingRequirements/Container`:

| Elemento | Texto | Cor / ícone |
|---|---|---|
| `RequirementsHeaderLeft` | `@ui_upgrade_requirements` = "Requirements" | pica 20 `#9e947d`, maiúsculas |
| `ResourcesRequirement/HasResourcesText` + `HasResourcesIcon` | `@ui_has_resources`; se falta espaço `@ui_inventoryfull`; se falta moeda `@crafting_fee_low_no_fee` | `#30e5ab` + `icon_requirement_check.dds`; falha `#f25d5d` + `icon_requirement_x.dds` |
| `StationRequirement/Description` + `Icon` + `StationRequirementIcon` | `@ui_station_desc_text` = "{stationName} {tier} {current}" → "Smelter Tier 2"; se não atende, acrescenta "(Current: Tier n)" | verde/vermelho como acima; `Icon` = ícone da estação, texto em x=23 |
| `SkillRequirement/Description` + `Icon` + `SkillRequirementIcon` | `@ui_station_skill_desc_text` = "{skillName} {level} {current}" → "Smelting Lv. 0" (`@cr_levelRequired`) | idem; `Icon` = `smallIcon` do ofício |
| `VersionRequirement` | só para receitas de expansão | |

Posição Y das linhas (Lua): estação em 81, habilidade em 109 (passo 28), relativas ao container.

Rodapé do painel:

| Elemento | Dado | Cor |
|---|---|---|
| `SP/OffsetContainer/TaxRate` (`CraftButtonTaxAmount`) | `@ui_craft_fee` = "{territoryName} Fee: <icon_crown> {fee}" (fee em Pica) | `#9e947e` quando pode fabricar, `#808080` quando não; desligado se taxa = 0 |
| `SP/OffsetContainer/SkillExpReward` (`SkillExpAmount`) | `@ui_craft_bonus` = "+{amount} {bonus} {bonus2}" → "+44 Smelting" | nimbus_regular 23, `#ffdb7a`; `QuestionMarkHint` ao lado (tooltip com base e bônus) |

## 3. Lista/árvore (`TV`, 91,148 504×860)

- Cabeçalho: `TV/FrameHeader` (`headerbg.dds`), `TV/FilterHeader/FilterInputField` (busca; placeholder `@crafting_filter_placeholder` = "Search"; ícone `icon_magnifying_glass.dds`), `FilterButton` (166×45, texto `@ui_add_filter` = "Filters"), `SortDropdown` (299×45, `@crafting_sort_level` = "Sort by Level"), `FiltersIndicator` (contador).
- Lista: `TV/ScrollBox` tem `UiDynamicScrollBoxComponent` com protótipo `TV/ScrollBox/Mask/Content/CraftingTreeViewItem`, tamanho fixo calculado do protótipo: 449×46. Os itens empilham a cada 46 px; o fundo `ListItemBg` tem 42 px (offset inferior −4), o que dá 4 px de vão. x do item = Content + 3. Máscara `rosterlistmask.dds`, barra `TV/ScrollBox/CraftingScrollbar`.
- O mesmo protótipo serve para cabeçalho e receita (`craftingtreeview.lua:OnElementBecomingVisible` chama `SetBackgroundType(depth < 3)` e `SetData`).

Profundidades (`craftingtreeviewitem.lua`): 0 = status (`@crafting_tree_learned` "Learned", `@crafting_tree_unlearned`, `@crafting_quest_recipes`), 1 = família ("Refined Resources"), 2 = grupo ("Metals"), 3 = receita.

| Profundidade | `StateIcon.x` | `TypeIcon.x` | `EntryText.x` | `EntryText.w` |
|---|---|---|---|---|
| 0 | 15 | — | 30 | 360 |
| 1 | 36 | 42 | 51 | 360 |
| 2 | 59 | 66 | 74 | 360 |
| 3 aprendida | 59 | 76 (RarityFrame 74) | 118 | 245 |
| 3 bloqueada | 16 | 34 (RarityFrame 32) | 76 | 280 |

`StateIcon` em 0–2: `icons/misc/dropdownArrowWhite.dds` (aberto) ou `dropdownArrowWhiteRight.dds` (fechado).

Linha de receita:

- `EntryText` = `"[{n}] " + nome` (`@ui_bracket_amount`). `n` é `QueryInventoryCache(resultItemId)`, ou seja, a quantidade do item resultado que o jogador já tem (inventário/armazém), não a quantidade fabricável. Sem colchete quando 0.
- `RarityFrame` (`itembgsquare{r}.dds`), `RarityFrame/RarityBg` (`itemraritybgsquare{r}.dds`), `TypeIcon` = ícone do item.
- `GearScoreText` (direita): com ordenação por nível ou nome mostra o nível de ofício exigido; `GearScoreText/TradeskillIcon` = `smallIcon` do ofício (tooltip `@ui_crating_tree_item_tradeskill_desc`).
- `RarityFrame/FirstCraftBar` (1×36 `#ffff00`) ligado quando a conquista de primeira fabricação não foi obtida.
- Fonte: `FONT_STYLE_BUTTON_VERTICAL_TAB_SMALL` (nimbus_medium 25).

Cores do texto:

- Cabeçalhos (0–2): `#b2aea1` normal, `#ebe3d6` no foco; status bloqueado `#808080`.
- Receita: `#ebe3d6` quando tem ingredientes, nível de estação, receita conhecida, nível de ofício, versão e não está em cooldown; `#f25d5d` quando falta algo; `#808080` bloqueada. `GearScoreText` e `TradeskillIcon` usam a mesma cor.

Fundo (`slices/listitembg.lua`):

- Cabeçalho (`LIST_ITEM_STYLE_ACCORDION_CATEGORY`): `itemBg` opacidade 0; `categoryBgAdd` (`#4a5d70`) ligado com opacidade 0,3.
- Receita (`LIST_ITEM_STYLE_ACCORDION`): `itemBg` cor `#e7e7e7` opacidade 0,07 (0,02 com HDR).
- Selecionada (`OnSelected`): toca o flipbook `SelectedSequence` (`listitembg/selectedsequence/listitem_long00..17.dds`; último quadro `listitem_long17.dds` é a faixa azul), `FocusHash` (`focusnew2023.dds`) a 0,65, `focusGlow` a 1, `AuraSequence` e `ParticleSequence` com fade 1 s.
- Hover: `FocusAdd` (`listitemfocusadd.dds`) e `HoverGlow`.

## 4. Ingredientes (`RP/Ingredients`)

Modelos fixos `RP/Ingredients/IngredientRow1..7` (script `ingredientrowv4.luac`), 336×60, Y = 72·(i−1) a partir de 668,466. Linhas além da quantidade de ingredientes ficam desligadas.

Para ingrediente fixo (caso do refino, ex. Iron Ore):

| Elemento | Estado |
|---|---|
| `TypeName` | desligado (só para ingrediente de categoria) |
| `IngredientSelectorBg` (`primaryingredientbg.dds`) | desligado |
| `CategoryIcon` (`itembgcirclenone.dds`) | desligado |
| `ItemLayout` | ligado, `SetItemByDescriptor`, modo `MODE_TYPE_CRAFTING`, quantidade própria desligada |
| `Quantity` | `"<font color=#ffffff>{tenho}</font>/{preciso}"` → "24/4"; o "tenho" em `#f25d5d` quando falta; Y = −10 |
| `Quantity/IngredientName` | nome do item; altura 58 com quebra; largura = 235 − largura de `Quantity` − 7; cor `#aaaaaa`, ou `#f25d5d` quando falta |
| `ItemFocus` (`primaryingredientfocus.dds`) | pulso 0,45↔1 só em linha de categoria |

"Tenho" = `InventoryCache:GetItemCount(itemId)` (inventário antes do armazém). "Preciso" = `entry.quantity` por unidade.

## 5. Quantidade (`SP/OffsetContainer/Quantity`, `craftquantitywidget.lua`)

- Mínimo fixo 1 (`minimumQuantityToCraft`). Máximo = `CraftingRecipePanel:GetMaxQuantity()` (limitado por ingredientes, moeda da taxa, cooldown e `javelin.max-crafting-jobs`). Se a receita bloqueia refabricação por conquista, máximo 1.
- Se máximo ≤ 1, `Slider`, `PlusButton`, `MinusButton`, `Minimum` e `Maximum` ficam desligados.
- `Minimum` (1452,761) e `Maximum` (1642,761): pica 32 branco, `tostring`.
- Slider `SliderWithTextInput/Slider` 300×22, `snapToIntegers`, estilo `SLIDER_STYLE_2`: o texto do slider fica oculto e o campo de valor (`SliderTextInput`) vai para x=0, y=−50, 250×40, Caslon 36 centralizado. É o número acima da barra.
- `MinusButton` (1385,729) e `PlusButton` (1753,729), 50×46, ícones `icons/crafting/icon_crafting_minus.png` / `icon_crafting_plus.png`, repetição ao segurar. `+` desliga no máximo, `−` no mínimo.
- Valor inicial 1. Espaço (`setCraftMax`, `@crafting_set_max` = "Max Quantity") leva ao máximo.

## 6. Botão CRAFT e a sequência

Botão `SP/OffsetContainer/CraftButton` (1385,854 420×90), `button.luac` em `BUTTON_STYLE_HERO`, fonte 40:

- Normal: texto `@ui_craft` (Pica, `#ebe3d6`, maiúsculas); `buttonBg` (`buttonbg.dds`) tingido de preto; `heroImageSequence` toca o flipbook azul `slices/button/blue_loop/blue_00..14.dds`; `frame` = `heroframe2023.dds`; `buttonBottomGlow` y = −11 cor `#63d5ff`; `heroBorder` (`heroborder.dds`) pulsa 0,3↔1.
- Desabilitado: `heroImageSequenceDisabled`, `heroframedisabled2023.dds`; texto `@cr_cantcraft` ("Craft"), `@cr_alreadycrafted` ou `@cr_itemdisabled`.

Sequência ao clicar (`craftingscreenv4.lua:OnCraftClicked` → `SetState` → `craftroll.lua`):

1. Exige `GetCanCraft()` e quantidade > 0. Liga `Hud.LocalPlayer.Inventory.SuppressNotificationsWhileCrafting`. Envia `CraftRecipe`. Se nada chegar em 10 s, `OnCraftingFailed` (`@crafting_failed`) e volta.
2. `CraftRoll.Window:SetCraftDisplayType`: `RefiningAmount` desligado com texto "0". `SetRollData` com tipo `"Quantity"`: desliga `Stats/Weapons`, `Stats/Armor`, `Stats/Trinket`, `Stats/PerkContainer`, `QualityInfo`; liga `Stats/Quantity` e `Header/HeaderOffset/ItemName`; `BaseValue`/`BonusValue` opacidade 0; `HeaderOffset` y = 80; `ProgressionOffset` y = −150. Cabeçalho do CraftDone `@cr_base_amount`.
3. `STATE_CRAFTROLL`: `CP` desligado; `S/../BackgroundStep1` (`crafting_background_step3.dds`) 0→1 em 0,35 s; `CraftRoll/Background/Mask/RuneCircleA` e `RuneCircleB` (`crafting_wheelcirclea/b.dds`) giram +359° e −359° a cada 10 s em loop; `CraftRoll/Background` e `CraftRoll/CraftRollContainer` 0→1 em 0,3 s com atraso 0,25 s; profundidade de campo 6 m / blur 0,4 em 0,5 s; cabeçalho da estação oculto; câmera `UI_CraftingStep3`.
4. Itens chegam (`OnSlotUpdate`, `ItemCreated`). Em receita CraftAll espera o inventário subir `jobs × saída`, então `SetQuantityCrafted(ganho, jobs, saída)` e `StartRefiningBarAnimation`:
   - `ProgressionContainer` 0→1 em 0,5 s, atraso 0,5 s; y = −68.
   - Duração total padrão 1 s (`duration`). A cada tick `RefiningAmount` (pica 90, 1226,480) = `floor(quantidade × t / 1 s)`, de 0 até o total.
   - Raridade 0: `ItemName` `#9e947e`, `Cloud` (`crafting_runecloud.dds`) `#dddddd`. Com raridade: cores `COLOR_RARITY_LEVEL_n(_BRIGHT/_BG)` em `RarityText`, `ItemName`, `Cloud`, `RarityGlow1`.
   - `RarityOffset` y 100→0 em 0,4 s com atraso 1 s; ao terminar, som `Crafting_Done`.
   - `Header` 0→1 em 0,4 s; `Cloud` 0→0,25 em 1 s; `Stats/Quantity` 0→1 em 0,4 s.
   - No fim: `BaseValue` = quantidade pedida (`@cr_base_amount` "Base"), `BonusValue` = recebido − pedido (`@cr_bonus_amount` "Bonus"), ambos 0→1 em 0,5 s com atraso 0,2 s. `RefiningAmount` tem rótulo `@ui_total`.
   - Receita não CraftAll com quantidade > 1 (e sem `javelin.crafting-enable-craft-all-override`): fabrica 1 por vez e repete `OnCraftClicked` decrementando; `MultiCraft/CraftProgressMessage` mostra `@ui_craftprogress` "Crafting {amount} of {maximum}".
5. XP do ofício: `ProgressionContainer/.../SkillProgression` mostra `NumberText` "+N" (pica 24), `SkillText` "@ui_" + ofício, `BonusText` multiplicador (`#fdca43`) e o círculo `SkillLevelCircleProgress2` (`skilllevelcircleprogressbg.dds` + `skilllevelcircleprogressfill.dds`, pips `skillcirclepip.dds`), animados por `OnCategoricalProgressionPointsChanged`; `StandingProgression` faz o mesmo para reputação do território (barra `#829b71`). Subida de nível: `TradeSkillLevelUpCelebration` (0,3 s / 1,5 s).
6. `AdvanceStep` 0,2 s após o início da barra (e um `Delay(2)` de segurança) passa para `STATE_CRAFTDONE`: a janela do roll continua; `CraftDone/CraftDoneWindow/ContinueText` mostra `@ui_clickanywhere` (nimbus 24 `#d3c9b9`) e `@cr_continue_crafting` (pica 30 `#a09784`) com `icon_arrow_continue.dds`; botões Continue/Exit (`@cr_continue_crafting`, `@cr_exit_station`) são reparentados para `S/ExitButtonParent/ActiveButtonsContainer`.
7. Clique em qualquer lugar (`S/ClickableArea`) ou Esc: `CraftCompleteReturnToStation` → `STATE_CRAFTITEM`: `BackgroundStep1` 1→0 em 0,35 s, roll some, DOF 0 / 0,95 em 0,5 s, `CP` volta e lista/receita são atualizadas.

Pular (Espaço/Esc durante o roll): `TrySkip` avança pelos estados `SKIP_STATE_*`; `SkipAnimation` termina a contagem em 0,1 s.

## 7. Barra de título e rodapé

Não estão em `craftingscreenv4`. A barra vem de `extract/ui/lyshineui/stationproperties/stationproperties.dynamicuicanvas.json` (script `stationproperties.lua` + `slices/screenheader.lua`), que a fabricação mostra junto (`DynamicBus.StationPropertiesBus`).

`_root/StationProperties/ScreenHeader` (âncora topo-centro, offsets −960,0,960,104):

| Elemento | Detalhe |
|---|---|
| `ScreenHeader/MainBg` | `slices/screenheader/screen_header_bg_2023.dds`, 1920×100 |
| `ContentHolder/HeaderHolder` | offsets 56,27 → 1864,90 |
| `HeaderHolder/BackButton` | `icons/misc/icon_back_dropshadow.dds` 23×23 `#c6b694` (o "<"), offsets 0,5 |
| `HeaderHolder/HeaderText` | `@ui_stationName` = "{station}" com o nome da estação (ex. "Smelter Tier 3"); offsets 34,−16 → 834,46; `lyshineui/fonts/pica_regular.font`; estilo `FONT_STYLE_SCREEN_HEADER_2023` = Pica 42, `#ebe3d6`, maiúsculas, espaçamento 150, sombra (canvas: 44, `#d0c7b4`) |
| `RightSideHolder/CurrencyHolder/CurrencyDisplay/CurrencyAmount` | moedas, pica 32 `#d0d0d0` (`FONT_STYLE_SCREEN_HEADER_DATA`: Caslon 32 `#ebe3d6`), ícone `icon_crowns.dds` 22×22 à esquerda; âncora x = 0,976 |
| `RightSideHolder/SkillHolder` | círculos de ofício; desligado no canvas |

Rodapé: `extract/ui/lyshineui/helpbar/helpbar.dynamicuicanvas.json` (`helpbar.lua`, `actiondisplay.lua`).

- `Helpbar/ActionsDisplays` e `Helpbar/GlobalActionDisplays`: âncora canto inferior direito, altura 58. Cada `ActionDisplay` 200×56; `Hint` 32×32 com `slices/hint/hintframefilled.dds` e `HintText` (nimbus_regular 20, maiúsculas: "ENTER", "SPACE", "ESC"); `Text` nimbus_semibold 25 branco em x+44. Espaço entre ações `DEFAULT_ACTION_SPACING = 50`, margem `LIST_MARGIN = 56`. `FullBackground` (preto, linha `tooltip_divider.dds`) desligado.
- Globais padrão: Chat e `ui_cancel` → `@ui_back` "Back".
- A fabricação empilha `CraftingTreeOptions` (`OnCanCraftUpdated`): `toggleFilterPopup` (`@crafting_view_filters`), `toggleCraftingTreeExpandCollapse` (`@crafting_expand_all` / `@crafting_collapse_all`, segurar), e, se pode fabricar, `setCraftMax` (`@crafting_set_max`, só com máximo > 1) e `craftItem` (`@ui_craft`, segurar). O print mostra só Chat, Max Quantity e Back; as demais não aparecem no teclado/mouse nesse print (provavelmente sem tecla atribuída).

## 8. Texturas do modo refino

Caminhos relativos a `lyshineui/images/`, convertidos em `extract/ui/tex`.

Estado de seleção:

- crafting: `crafting_background_step3.dds`, `crafting_panel_infobackground.dds`, `craftingstatbg.dds`, `crafting_refiningcircle.dds`, `crafting_itemraritybglarge0..4.dds`, `crafting_tier_0..5.dds`, `headerrune1..3.dds`, `reciperune1..3.dds`, `primaryingredientbg.dds`, `primaryingredientfocus.dds`, `itembgcirclenone.dds`, `crafting_filter_indicator_bg.dds`
- icons: `icons/misc/icon_skilllevelbg.dds`, `icon_requirement_check.dds`, `icon_requirement_x.dds`, `icon_exclamation_white_small.dds`, `dropdownArrowWhite.dds`, `dropdownArrowWhiteRight.dds`, `icons/crafting/icon_crafting_plus.png`, `icon_crafting_minus.png`, `icons/tradeskills/icon_tradeskill_*.dds`, `icons/items_hires/*` (ícone do resultado), `icon_crown.dds`, `icon_crowns.dds`
- slices/button: `buttonbg.dds`, `buttonbgfocus2023.dds`, `buttonbottomglow2023.dds`, `buttonfocusglow.dds`, `focusnew.dds`, `focusnew2023.dds`, `heroborder.dds`, `heroframe2023.dds`, `heroframedisabled2023.dds`, `primarybuttonframe.dds`, `primarybuttonframefocus2023.dds`, `blue_loop/blue_00..14.dds`
- slices/buttonsimple: `button_simple.dds`, `button_simple_bgmask1.dds`
- slices/listitembg: `listitemfocusadd.dds`, `focusglow.dds`, `selectedsequence/listitem_long00..17.dds`
- slices/itemlayout: `itembgsquare{r}.dds`, `itemraritybgsquare{r}.dds`, `spritesheetnewitemrectangle.dds`
- slices/slider: `sliderfillleft/right/slice.dds`, `sliderfilltantiled.dds`, `sliderframeleft/right/slice(.dds|hover.dds)`, `sliderhandle2023.dds`, `sliderhandlearrowleft/right.dds`, `sliderhandlefocus.dds`, `slidermask.dds`, `slidertexture.dds`
- molduras: `slices/framemultibg/framecorner.dds`, `frametexture.dds`, `linegradient.dds`, `framestyles/framebgdefaultnooutline.dds`, `slices/frameheader/headerbg.dds`, `slices/drawnline/drawnlinethin1slice.dds`, `slices/scrollbar/scrollbarbg.dds`, `scrollbarhandle.dds`, `slices/dropdown/dropdownarrow.dds`, `slices/frame/frametexture2023.dds`, `framethin.dds`, `frame_textinput.dds`, `frame_lootticker.dds`, `guildmenu/rosterlistmask.dds`
- outros: `socialpane/icon_magnifying_glass.dds`, `socialpane/icon_clear_field.dds`, `socialpane/groupbuttonglow.dds`, `hud/chat/settingsbuttonbg.dds`, `icons/misc/gear.dds`, `slices/hint/hintframefilled.dds`

Roll e fim: `crafting/crafting_wheelcirclea.dds`, `crafting_wheelcircleb.dds`, `crafting_runecloud.dds`, `crafting_itemraritybglarge{r}.dds`, `crafting_rune_middle_static.dds`, `crafting_rune_clockwise.dds`, `crafting_rune_counter_clockwise.dds`, `landingscreen/mapmask.dds`, `hud/tutorialmessage/tutorialmessagemask.dds`, `slices/drawnline/drawnlinethin1right.dds`, `skills/tradeskills/skilllevelcircleprogressbg.dds`, `skilllevelcircleprogressfill.dds`, `skillcirclepip.dds`, `lootticker/lootticker_pulse.dds`, `icons/objectives/icon_territorystanding_2.dds`, `icons/misc/icon_arrow_continue.dds`.

Barra e rodapé: `slices/screenheader/screen_header_bg_2023.dds`, `icons/misc/icon_back_dropshadow.dds`, `icon_crowns.dds`, `slices/hint/hintframefilled.dds`, `tooltip/tooltip_divider.dds`.

Fontes: `lyshineui/fonts/pica_regular.font`, `pica_italic.font`, `nimbus_medium.font`, `nimbus_regular.font`, `nimbus_regular_italic.font`, `nimbus_semibold.font`, `nimbus_bold.font`; Caslon no valor do slider e nos dados do cabeçalho.

## Pendências

- A cor exata do texto da receita quando falta só nível de ofício: o decompilado tem aninhamento ambíguo; o print confirma vermelho.
- `ItemName` do recurso: depende de `UsesRarity()` do item; se falso, `#cccccc` sem `ItemRarity`.
- Feito em 2026-10-13: `caseoverride` no `tools/ui_canvas.py`; `heroframe2023` e `blue_loop/*` extraídos pelas
  `extra_textures` do spec. Acertos que faltam na tela: `docs/hud/README.md`, "Anotado para a próxima sessão".
