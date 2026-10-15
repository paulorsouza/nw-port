# Crafting screen in refining mode

How `craftingscreenv4` looks at a refining station (Smelter, Tannery, etc.). Sources: the game's UI scripts in
`extract/lua/lyshineui/crafting/` and `_common/`, canvas
`extract/ui/lyshineui/crafting/craftingscreenv4.dynamicuicanvas.json` converted by `tools/ui_canvas.py`. Coordinates
are from the converted layout (1920×1080). In the port: `FNWRefiningMenu`, spec `tools/ui_screens/refining.json`,
capture `tools/ui_shots/refining.json`. Work cycle and LyShine rules: `docs/game/ui/README.md`.

Path abbreviations:

- `S` = `_root/CraftingScreen/CraftingContainer`
- `CP` = `S/CraftingPanel`
- `SP` = `CP/CraftingStatsPanel`, `RP` = `CP/CraftingRecipePanel`, `TV` = `CP/CraftingTreeView`

The Lua property → element binding was taken from the canvas script components (property names are the CRC32 of the
lowercase name). The result is in the tables below.

## 1. What turns on and off in refining

The mode is decided per recipe, not per station. `craftingstatspanel.lua:SetCraftingParams` does
`isCraftAll = IsRecipeCraftAll(id) or IsRecipeRefining(id)` and, with `isCraftAll` true (and `GetSkipGrantItems`
false):

| On | Off |
|---|---|
| `SP/Refining` (property `RefiningInfo.Panel`) | `SP/QualityComplex/QualityBar` (gear score bar) |
| | `SP/QualityComplex/TopInfo` (gear score label and range) |
| | `SP/StatsMasterContainer/StatsScrollBox/StatContainer` (stats, perks, attribute, gem, azoth) |
| | `SP/OffsetContainer/AchievementRecipeDesc` (`CampSkinText`) |

`UpdateStatsInfo` (which clones stat/perk/attribute rows) does not run in this mode.

Other things turned off in refining:

- `RP/Modifiers/AddPerkItem`, `AddPerkItem2..4`: on only if `isProcedural and canHavePerks`. Refining is not
  procedural → all off.
- `RP/Modifiers/AddAzoth` (`CurrencyIngredient.Panel`): on only if procedural and `perkUpgradeCost > 0` → off.
- `RP/Modifiers/NoPerksText`: on (`showNoPerksText = true`). Text `@crafting_no_bonus` = "Craft mods cannot be added
  to this item."
- `SP/Refining/GlobalBonus`: on only if the skill is a refining skill and there is a faction buff
  `Crafting_Refining_Yield_Modifier > 0`.
- `SP/OffsetContainer/CooldownCount`: on only if the recipe has a cooldown.
- `SP/OffsetContainer/RequiredStats`, `RP/PerkItemSelector`, `RP/CategoryItemSelector`,
  `RP/OptionalResourceSelector`, `RP/ResourceSelectorBlocker`, `CP/CraftingTreeFilterPopup`: off in the canvas; they
  open only on an action.
- `S/MultiCraft`, `S/CraftRoll`, `S/CraftDone`, `S/CraftingSummaryList`, `S/SummaryHeaderContainer`: off in the
  selection state. `S/CraftingSummaryListFrame` is turned off in `OnTransitionIn`. `S/BlackBg` stays at opacity 0 in
  the `STATE_CRAFTITEM` state.
- `SP/HeaderTooltip` is only a hover area (no image).

Prefixes to turn on in the selection state: `S/../BackgroundStep1` (opacity 0), `TV`, `RP` (except
Modifiers/AddPerk*/AddAzoth and the selectors), `SP` with `SP/Refining`, `SP/MainDivider`, `SP/LineLeft`,
`SP/StatBg`, `SP/PinRecipeButton`, `SP/OffsetContainer/{Quantity,CraftingRequirements,CraftButton,TaxRate,SkillExpReward}`.

## 2. Areas, data and colors

Colors from `_common/uistyle.lua`: `COLOR_TAN_WARM #ebe3d6`, `COLOR_GRAY_NEW #b2aea1`, `COLOR_GRAY_50 #808080`,
`COLOR_GRAY_70 #aaaaaa`, `COLOR_GRAY_80 #cccccc`, `COLOR_GRAY_95 #e7e7e7`, `COLOR_TAN #9e947e`,
`COLOR_RED_MEDIUM #f25d5d`, `COLOR_GREEN #30e5ab`, `COLOR_YELLOW #ffdb7a`, `COLOR_BLUE_BRIGHT #63d5ff`.
`COLOR_INSUFFICIENT_QUANTITY = COLOR_RED_MEDIUM`. `COLOR_RARITY_GRAY = COLOR_GRAY_80`.

Uppercase: in the canvas these have `caseoverride = 2` (uppercase), among others: `RP/Header/ItemName`,
`RP/Header/ItemTier`, `SP/Refining/Label`, `RequirementsHeaderLeft`, `FrameHeaderText`, `HintText`,
`CraftRoll/.../ItemName`, `BonusLabel`, `BaseAmountLabel`, `ContinueText/Text2`. The Lua's Pica styles
(`FONT_STYLE_PICA_CORE`) also force uppercase, spacing 150 and shadow. `tools/ui_canvas.py` exports `caseoverride`.

### Center panel (`RP`, 636,151 715×860)

| Element | Data | Format / color |
|---|---|---|
| `RP/Header/ItemName` | name of the resulting item (`recipeData.name` or `displayName`); if output >1, appends ` x{n}` (`@ui_quantitywithx`) | nimbus_bold 36, uppercase; rarity color (`COLOR_TOOLTIP_RARITY_LEVEL_n`) or `#cccccc` if the item does not use rarity |
| `RP/Header/ItemName/Description` | `LocalizeWithDataSheetData(item.description)` → "Crafting Material." | nimbus_regular_italic 23, `FONT_STYLE_FLAVOR_TEXT` |
| `RP/Header/ItemRarityBg` | `crafting_itemraritybglarge{0..4}.dds` (index = rarity) | white |
| `RP/Header/ItemRarityBg/ItemIcon` | the item's hi-res icon | 312×140 |
| `RP/Header/ItemTier` | `"@cr_tier " + <img crafting_tier_{N}.dds scale=0.75>` → "TIER II" | pica_italic 24; Y=-20 when the item does not use rarity |
| `RP/Header/ItemRarity` | "@RarityLevel{n}_DisplayName" | off if the item does not use rarity |
| `RP/Header/HeaderRuneMask/HeaderRune1..3` | `headerrune1..3.dds`; Rune2 spins 0→359°, Rune3 0→−359°, 80 s per turn, loop | circular emblem behind the icon |
| `RP/RecipeRuneMask/RecipeRune1..3` | `reciperune1..3.dds`, same rotations (80 s) | background of the ingredients area |
| `RP/Header/ResourceSourceNotification/ResourceOriginText` | `@ui_crafting_resource_origin_message` | nimbus_regular_italic 20; icon `icon_exclamation_white_small.dds` `#ffda7a`; `#666666` lines above/below |
| `RP/IngredientDivider` | vertical line 1×496 at x=1019 | `#666666` |
| `RP/Modifiers/NoPerksText` | `@crafting_no_bonus` | 1034,466 284×100, `FONT_STYLE_CRAFTING_NO_PERKS` |
| `RP/Header/ItemName/BonusHintLayout` | first-craft bonus hint | only if the first-craft achievement was not earned |

`SP/PinRecipeButton` (691,317 240×42, buttonsimple) sits over the center header: `@ui_pin_recipe` /
`@ui_unpin_recipe`. The port hides it in the layout.

### Right panel (`SP`, 1350,151 490×860; background `crafting_panel_infobackground.dds` + `SP/StatBg` `craftingstatbg.dds`)

Refining block `SP/Refining` (1350,189 380×545):

| Element | Data | Format |
|---|---|---|
| `SP/Refining/Label` | `@cr_bonusPossibility` = "POTENTIAL BONUS" | nimbus_medium 24, uppercase, 1391,172 |
| `SP/Refining/Skill/LevelBg` | `icon_skilllevelbg.dds` 92×92 | level circle |
| `.../Skill/LevelBg/Level` (`SkillLevel`) | current trade skill level (`GetRank`); if the trade skill is `WildernessSurvival`, character level+1 | plain integer, pica 40 |
| `.../Skill/Description` (`SkillName`) | `"@ui_" + tradeskill` → "Smelting" | nimbus_medium 25 `#8a7c5e` |
| `SP/Refining/Bonus/LevelBg` | `crafting_refiningcircle.dds` 92×92 | |
| `.../Bonus/LevelBg/BonusAmt` (`BonusAmount`) | `GetChanceForBonusItems(recipe, ingredients)` | `"%.0f%%"` → "30%", pica 34 |
| `.../Bonus/Description` | `@ui_skill_bonus` = "Chance to craft additional items" | nimbus_medium 25 `#8a7c5e` |
| `.../GlobalBonus/LevelBg/BonusAmt` | faction buff ×100 | `"+%.0f%%"`; text `@ui_crafting_refining_yield_modifier_name` |

Y rows in the canvas: Skill 245, Bonus 331, GlobalBonus 417 (step 86).

Requirements `SP/OffsetContainer/CraftingRequirements/Container`:

| Element | Text | Color / icon |
|---|---|---|
| `RequirementsHeaderLeft` | `@ui_upgrade_requirements` = "Requirements" | pica 20 `#9e947d`, uppercase |
| `ResourcesRequirement/HasResourcesText` + `HasResourcesIcon` | `@ui_has_resources`; if out of space `@ui_inventoryfull`; if out of coins `@crafting_fee_low_no_fee` | `#30e5ab` + `icon_requirement_check.dds`; failure `#f25d5d` + `icon_requirement_x.dds` |
| `StationRequirement/Description` + `Icon` + `StationRequirementIcon` | `@ui_station_desc_text` = "{stationName} {tier} {current}" → "Smelter Tier 2"; if not met, appends "(Current: Tier n)" | green/red as above; `Icon` = station icon, text at x=23 |
| `SkillRequirement/Description` + `Icon` + `SkillRequirementIcon` | `@ui_station_skill_desc_text` = "{skillName} {level} {current}" → "Smelting Lv. 0" (`@cr_levelRequired`) | same; `Icon` = the trade skill's `smallIcon` |
| `VersionRequirement` | only for expansion recipes | |

Y position of the rows (Lua): station at 81, skill at 109 (step 28), relative to the container.

Panel footer:

| Element | Data | Color |
|---|---|---|
| `SP/OffsetContainer/TaxRate` (`CraftButtonTaxAmount`) | `@ui_craft_fee` = "{territoryName} Fee: <icon_crown> {fee}" (fee in Pica) | `#9e947e` when crafting is possible, `#808080` when not; off if the fee = 0 |
| `SP/OffsetContainer/SkillExpReward` (`SkillExpAmount`) | `@ui_craft_bonus` = "+{amount} {bonus} {bonus2}" → "+44 Smelting" | nimbus_regular 23, `#ffdb7a`; `QuestionMarkHint` next to it (tooltip with base and bonus) |

Fee and XP in the port: coins are stored in cents and shown ÷100 on every screen, like `GetLocalizedCurrency`
(`UNWInventoryComponent::FormatCoins`). XP = reward × Σ Qty of the ingredients × quantity (`sistemas-005`). First
craft ×3 (crafted recipes in `progression.crafted`). The port has no faction buffs or boosts.

## 3. List/tree (`TV`, 91,148 504×860)

- Header: `TV/FrameHeader` (`headerbg.dds`), `TV/FilterHeader/FilterInputField` (search; placeholder
  `@crafting_filter_placeholder` = "Search"; icon `icon_magnifying_glass.dds`), `FilterButton` (166×45, text
  `@ui_add_filter` = "Filters"), `SortDropdown` (299×45, `@crafting_sort_level` = "Sort by Level"),
  `FiltersIndicator` (counter).
- List: `TV/ScrollBox` has a `UiDynamicScrollBoxComponent` with prototype
  `TV/ScrollBox/Mask/Content/CraftingTreeViewItem`, fixed size computed from the prototype: 449×46. Items stack every
  46 px; the `ListItemBg` background is 42 px (bottom offset −4), which leaves a 4 px gap. Item x = Content + 3. Mask
  `rosterlistmask.dds`, bar `TV/ScrollBox/CraftingScrollbar`.
- The same prototype serves for headers and recipes (`craftingtreeview.lua:OnElementBecomingVisible` calls
  `SetBackgroundType(depth < 3)` and `SetData`).
- Tree order: `CategorySortOrder`/`GroupSortOrder` from `craftingsortinfo.lua` (`make_crafting.py` writes
  `category_order`/`group_order`).

Depths (`craftingtreeviewitem.lua`): 0 = status (`@crafting_tree_learned` "Learned", `@crafting_tree_unlearned`,
`@crafting_quest_recipes`), 1 = family ("Refined Resources"), 2 = group ("Metals"), 3 = recipe.

| Depth | `StateIcon.x` | `TypeIcon.x` | `EntryText.x` | `EntryText.w` |
|---|---|---|---|---|
| 0 | 15 | — | 30 | 360 |
| 1 | 36 | 42 | 51 | 360 |
| 2 | 59 | 66 | 74 | 360 |
| 3 learned | 59 | 76 (RarityFrame 74) | 118 | 245 |
| 3 locked | 16 | 34 (RarityFrame 32) | 76 | 280 |

`StateIcon` at 0–2: `icons/misc/dropdownArrowWhite.dds` (open) or `dropdownArrowWhiteRight.dds` (closed).

Recipe row:

- `EntryText` = `"[{n}] " + name` (`@ui_bracket_amount`). `n` is `QueryInventoryCache(resultItemId)`, that is, the
  quantity of the resulting item the player already has (inventory/storage), not the craftable quantity. No bracket
  when 0.
- `RarityFrame` (`itembgsquare{r}.dds`), `RarityFrame/RarityBg` (`itemraritybgsquare{r}.dds`), `TypeIcon` = item icon.
- `GearScoreText` (right): when sorted by level or name it shows the required trade skill level;
  `GearScoreText/TradeskillIcon` = the trade skill's `smallIcon` (tooltip `@ui_crating_tree_item_tradeskill_desc`).
- `RarityFrame/FirstCraftBar` (1×36 `#ffff00`) on when the first-craft achievement was not earned.
- Font: `FONT_STYLE_BUTTON_VERTICAL_TAB_SMALL` (nimbus_medium 25).

Text colors:

- Headers (0–2): `#b2aea1` normal, `#ebe3d6` on focus; locked status `#808080`.
- Recipe: `#ebe3d6` when it has ingredients, station level, known recipe, trade skill level, version and is not on
  cooldown; `#f25d5d` when something is missing; `#808080` locked. `GearScoreText` and `TradeskillIcon` use the same
  color.

Background (`slices/listitembg.lua`):

- Header (`LIST_ITEM_STYLE_ACCORDION_CATEGORY`): `itemBg` opacity 0; `categoryBgAdd` (`#4a5d70`) on at opacity 0.3.
- Recipe (`LIST_ITEM_STYLE_ACCORDION`): `itemBg` color `#e7e7e7` opacity 0.07 (0.02 with HDR).
- Selected (`OnSelected`): plays the `SelectedSequence` flipbook
  (`listitembg/selectedsequence/listitem_long00..17.dds`; the last frame `listitem_long17.dds` is the blue strip),
  `FocusHash` (`focusnew2023.dds`) at 0.65, `focusGlow` at 1, `AuraSequence` and `ParticleSequence` with a 1 s fade.
- Hover: `FocusAdd` (`listitemfocusadd.dds`) and `HoverGlow`.

## 4. Ingredients (`RP/Ingredients`)

Fixed templates `RP/Ingredients/IngredientRow1..7` (script `ingredientrowv4.luac`), 336×60, Y = 72·(i−1) from
668,466. Rows beyond the number of ingredients stay off.

For a fixed ingredient (the refining case, e.g. Iron Ore):

| Element | State |
|---|---|
| `TypeName` | off (only for a category ingredient) |
| `IngredientSelectorBg` (`primaryingredientbg.dds`) | off |
| `CategoryIcon` (`itembgcirclenone.dds`) | off |
| `ItemLayout` | on, `SetItemByDescriptor`, mode `MODE_TYPE_CRAFTING`, its own quantity off |
| `Quantity` | `"<font color=#ffffff>{have}</font>/{need}"` → "24/4"; "have" in `#f25d5d` when short; Y = −10 |
| `Quantity/IngredientName` | item name; height 58 with wrap; width = 235 − width of `Quantity` − 7; color `#aaaaaa`, or `#f25d5d` when short |
| `ItemFocus` (`primaryingredientfocus.dds`) | pulse 0.45↔1 only on a category row |

"Have" = `InventoryCache:GetItemCount(itemId)` (inventory before storage). "Need" = `entry.quantity` per unit.

## 5. Quantity (`SP/OffsetContainer/Quantity`, `craftquantitywidget.lua`)

- Fixed minimum 1 (`minimumQuantityToCraft`). Maximum = `CraftingRecipePanel:GetMaxQuantity()` (limited by
  ingredients, fee coins, cooldown and `javelin.max-crafting-jobs`). If the recipe blocks re-crafting by achievement,
  maximum 1.
- If maximum ≤ 1, `Slider`, `PlusButton`, `MinusButton`, `Minimum` and `Maximum` are off.
- `Minimum` (1452,761) and `Maximum` (1642,761): pica 32 white, `tostring`.
- Slider `SliderWithTextInput/Slider` 300×22, `snapToIntegers`, style `SLIDER_STYLE_2`: the slider text is hidden and
  the value field (`SliderTextInput`) moves to x=0, y=−50, 250×40, Caslon 36 centered. It is the number above the bar.
  The marker can be dragged.
- `MinusButton` (1385,729) and `PlusButton` (1753,729), 50×46, icons `icons/crafting/icon_crafting_minus.png` /
  `icon_crafting_plus.png`, repeat while held (`button.lua` `repeatParamsDefault`). `+` turns off at the maximum, `−`
  at the minimum.
- Initial value 1. Space (`setCraftMax`, `@crafting_set_max` = "Max Quantity") goes to the maximum.

## 6. CRAFT button and the sequence

Button `SP/OffsetContainer/CraftButton` (1385,854 420×90), `button.luac` in `BUTTON_STYLE_HERO`, font 40:

- Normal: text `@ui_craft` (Pica, `#ebe3d6`, uppercase); `buttonBg` (`buttonbg.dds`) tinted black;
  `heroImageSequence` plays the blue flipbook `slices/button/blue_loop/blue_00..14.dds`; `frame` = `heroframe2023.dds`;
  `buttonBottomGlow` y = −11 color `#63d5ff`; `heroBorder` (`heroborder.dds`) pulses 0.3↔1.
- Disabled: `heroImageSequenceDisabled`, `heroframedisabled2023.dds`; text `@cr_cantcraft` ("Craft"),
  `@cr_alreadycrafted` or `@cr_itemdisabled`.
- Flipbook timing: the game's loop is 30 frames at 30 fps. The port plays `blue_00..29` and, when disabled,
  `gray_00..29`, loaded when the screen opens. (unclear in the original: the texture list in this section names
  `blue_00..14`.)

Sequence on click (`craftingscreenv4.lua:OnCraftClicked` → `SetState` → `craftroll.lua`):

1. Requires `GetCanCraft()` and quantity > 0. Turns on `Hud.LocalPlayer.Inventory.SuppressNotificationsWhileCrafting`.
   Sends `CraftRecipe`. If nothing arrives in 10 s, `OnCraftingFailed` (`@crafting_failed`) and goes back.
2. `CraftRoll.Window:SetCraftDisplayType`: `RefiningAmount` off with text "0". `SetRollData` with type `"Quantity"`:
   turns off `Stats/Weapons`, `Stats/Armor`, `Stats/Trinket`, `Stats/PerkContainer`, `QualityInfo`; turns on
   `Stats/Quantity` and `Header/HeaderOffset/ItemName`; `BaseValue`/`BonusValue` opacity 0; `HeaderOffset` y = 80;
   `ProgressionOffset` y = −150. CraftDone header `@cr_base_amount`.
3. `STATE_CRAFTROLL`: `CP` off; `S/../BackgroundStep1` (`crafting_background_step3.dds`) 0→1 in 0.35 s;
   `CraftRoll/Background/Mask/RuneCircleA` and `RuneCircleB` (`crafting_wheelcirclea/b.dds`) spin +359° and −359°
   every 10 s in a loop; `CraftRoll/Background` and `CraftRoll/CraftRollContainer` 0→1 in 0.3 s with a 0.25 s delay;
   depth of field 6 m / blur 0.4 in 0.5 s; station header hidden; camera `UI_CraftingStep3`.
4. Items arrive (`OnSlotUpdate`, `ItemCreated`). In a CraftAll recipe it waits for the inventory to grow by
   `jobs × output`, then `SetQuantityCrafted(gain, jobs, output)` and `StartRefiningBarAnimation`:
   - `ProgressionContainer` 0→1 in 0.5 s, delay 0.5 s; y = −68.
   - Default total duration 1 s (`duration`). On each tick `RefiningAmount` (pica 90, 1226,480) =
     `floor(quantity × t / 1 s)`, from 0 to the total.
   - Rarity 0: `ItemName` `#9e947e`, `Cloud` (`crafting_runecloud.dds`) `#dddddd`. With rarity: colors
     `COLOR_RARITY_LEVEL_n(_BRIGHT/_BG)` on `RarityText`, `ItemName`, `Cloud`, `RarityGlow1`.
   - `RarityOffset` y 100→0 in 0.4 s with a 1 s delay; when done, sound `Crafting_Done`.
   - `Header` 0→1 in 0.4 s; `Cloud` 0→0.25 in 1 s; `Stats/Quantity` 0→1 in 0.4 s.
   - At the end: `BaseValue` = requested quantity (`@cr_base_amount` "Base"), `BonusValue` = received − requested
     (`@cr_bonus_amount` "Bonus"), both 0→1 in 0.5 s with a 0.2 s delay. `RefiningAmount` has the label `@ui_total`.
   - A non-CraftAll recipe with quantity > 1 (and without `javelin.crafting-enable-craft-all-override`): crafts 1 at
     a time and repeats `OnCraftClicked` counting down; `MultiCraft/CraftProgressMessage` shows `@ui_craftprogress`
     "Crafting {amount} of {maximum}".
5. Trade skill XP: `ProgressionContainer/.../SkillProgression` shows `NumberText` "+N" (pica 24), `SkillText`
   "@ui_" + trade skill, `BonusText` multiplier (`#fdca43`) and the circle `SkillLevelCircleProgress2`
   (`skilllevelcircleprogressbg.dds` + `skilllevelcircleprogressfill.dds`, pips `skillcirclepip.dds`), animated by
   `OnCategoricalProgressionPointsChanged`; `StandingProgression` does the same for territory standing (bar
   `#829b71`). Level up: `TradeSkillLevelUpCelebration` (0.3 s / 1.5 s).
6. `AdvanceStep` 0.2 s after the bar starts (and a safety `Delay(2)`) moves to `STATE_CRAFTDONE`: the roll window
   stays; `CraftDone/CraftDoneWindow/ContinueText` shows `@ui_clickanywhere` (nimbus 24 `#d3c9b9`) and
   `@cr_continue_crafting` (pica 30 `#a09784`) with `icon_arrow_continue.dds`; the Continue/Exit buttons
   (`@cr_continue_crafting`, `@cr_exit_station`) are reparented to `S/ExitButtonParent/ActiveButtonsContainer`.
7. Click anywhere (`S/ClickableArea`) or Esc: `CraftCompleteReturnToStation` → `STATE_CRAFTITEM`: `BackgroundStep1`
   1→0 in 0.35 s, the roll disappears, DOF 0 / 0.95 in 0.5 s, `CP` comes back and the list/recipe are refreshed.

Skip (Space/Esc during the roll): `TrySkip` advances through the `SKIP_STATE_*` states; `SkipAnimation` finishes the
count in 0.1 s. Space is the `tryskip` action (`uishared.xml`): maximum on the selection screen, skips the count,
returns to the station.

## 7. Title bar and footer

They are not in `craftingscreenv4`. The bar comes from
`extract/ui/lyshineui/stationproperties/stationproperties.dynamicuicanvas.json` (script `stationproperties.lua` +
`slices/screenheader.lua`), which crafting shows alongside (`DynamicBus.StationPropertiesBus`).

`_root/StationProperties/ScreenHeader` (top-center anchor, offsets −960,0,960,104):

| Element | Detail |
|---|---|
| `ScreenHeader/MainBg` | `slices/screenheader/screen_header_bg_2023.dds`, 1920×100 |
| `ContentHolder/HeaderHolder` | offsets 56,27 → 1864,90 |
| `HeaderHolder/BackButton` | `icons/misc/icon_back_dropshadow.dds` 23×23 `#c6b694` (the "<"), offsets 0,5 |
| `HeaderHolder/HeaderText` | `@ui_stationName` = "{station}" with the station name (e.g. "Smelter Tier 3"); offsets 34,−16 → 834,46; `lyshineui/fonts/pica_regular.font`; style `FONT_STYLE_SCREEN_HEADER_2023` = Pica 42, `#ebe3d6`, uppercase, spacing 150, shadow (canvas: 44, `#d0c7b4`) |
| `RightSideHolder/CurrencyHolder/CurrencyDisplay/CurrencyAmount` | coins, pica 32 `#d0d0d0` (`FONT_STYLE_SCREEN_HEADER_DATA`: Caslon 32 `#ebe3d6`), icon `icon_crowns.dds` 22×22 on the left; anchor x = 0.976 |
| `RightSideHolder/SkillHolder` | trade skill circles; off in the canvas |

The game (`stationproperties.lua` → `screenheader.lua`) shows azoth, coins and the station's trade skill circle
(`SetSkill1Circle`: level, progress ring, icon). The circle is the 300×300 slice inside `SkillContainer1` (60×60),
scaled by the Lua. The port has no azoth.

Station names: `@<type>_station` + `ui_tier` (the `<TradeSkill>T<n>_MasterName` keys are misaligned in the game:
T2 = "Tier 3").

Footer: `extract/ui/lyshineui/helpbar/helpbar.dynamicuicanvas.json` (`helpbar.lua`, `actiondisplay.lua`).

- `Helpbar/ActionsDisplays` and `Helpbar/GlobalActionDisplays`: anchored to the bottom right corner, height 58. Each
  `ActionDisplay` 200×56; `Hint` 32×32 with `slices/hint/hintframefilled.dds` and `HintText` (nimbus_regular 20,
  uppercase: "ENTER", "SPACE", "ESC"); `Text` nimbus_semibold 25 white at x+44. Space between actions
  `DEFAULT_ACTION_SPACING = 50`, margin `LIST_MARGIN = 56`. `FullBackground` (black, line `tooltip_divider.dds`) off.
- Default globals: Chat and `ui_cancel` → `@ui_back` "Back".
- Crafting stacks `CraftingTreeOptions` (`OnCanCraftUpdated`): `toggleFilterPopup` (`@crafting_view_filters`),
  `toggleCraftingTreeExpandCollapse` (`@crafting_expand_all` / `@crafting_collapse_all`, hold), and, if crafting is
  possible, `setCraftMax` (`@crafting_set_max`, only with maximum > 1) and `craftItem` (`@ui_craft`, hold). The
  screenshot shows only Chat, Max Quantity and Back; the others do not appear for keyboard/mouse in that screenshot
  (H: probably no key assigned).

## 8. Textures for refining mode

Paths relative to `lyshineui/images/`, converted in `extract/ui/tex`. Textures not referenced by the canvas
(`heroframe2023`, `blue_loop/*`) are extracted through the spec's `extra_textures`.

Selection state:

- crafting: `crafting_background_step3.dds`, `crafting_panel_infobackground.dds`, `craftingstatbg.dds`,
  `crafting_refiningcircle.dds`, `crafting_itemraritybglarge0..4.dds`, `crafting_tier_0..5.dds`,
  `headerrune1..3.dds`, `reciperune1..3.dds`, `primaryingredientbg.dds`, `primaryingredientfocus.dds`,
  `itembgcirclenone.dds`, `crafting_filter_indicator_bg.dds`
- icons: `icons/misc/icon_skilllevelbg.dds`, `icon_requirement_check.dds`, `icon_requirement_x.dds`,
  `icon_exclamation_white_small.dds`, `dropdownArrowWhite.dds`, `dropdownArrowWhiteRight.dds`,
  `icons/crafting/icon_crafting_plus.png`, `icon_crafting_minus.png`, `icons/tradeskills/icon_tradeskill_*.dds`,
  `icons/items_hires/*` (icon of the result), `icon_crown.dds`, `icon_crowns.dds`
- slices/button: `buttonbg.dds`, `buttonbgfocus2023.dds`, `buttonbottomglow2023.dds`, `buttonfocusglow.dds`,
  `focusnew.dds`, `focusnew2023.dds`, `heroborder.dds`, `heroframe2023.dds`, `heroframedisabled2023.dds`,
  `primarybuttonframe.dds`, `primarybuttonframefocus2023.dds`, `blue_loop/blue_00..14.dds`
- slices/buttonsimple: `button_simple.dds`, `button_simple_bgmask1.dds`
- slices/listitembg: `listitemfocusadd.dds`, `focusglow.dds`, `selectedsequence/listitem_long00..17.dds`
- slices/itemlayout: `itembgsquare{r}.dds`, `itemraritybgsquare{r}.dds`, `spritesheetnewitemrectangle.dds`
- slices/slider: `sliderfillleft/right/slice.dds`, `sliderfilltantiled.dds`,
  `sliderframeleft/right/slice(.dds|hover.dds)`, `sliderhandle2023.dds`, `sliderhandlearrowleft/right.dds`,
  `sliderhandlefocus.dds`, `slidermask.dds`, `slidertexture.dds`
- frames: `slices/framemultibg/framecorner.dds`, `frametexture.dds`, `linegradient.dds`,
  `framestyles/framebgdefaultnooutline.dds`, `slices/frameheader/headerbg.dds`,
  `slices/drawnline/drawnlinethin1slice.dds`, `slices/scrollbar/scrollbarbg.dds`, `scrollbarhandle.dds`,
  `slices/dropdown/dropdownarrow.dds`, `slices/frame/frametexture2023.dds`, `framethin.dds`, `frame_textinput.dds`,
  `frame_lootticker.dds`, `guildmenu/rosterlistmask.dds`
- other: `socialpane/icon_magnifying_glass.dds`, `socialpane/icon_clear_field.dds`, `socialpane/groupbuttonglow.dds`,
  `hud/chat/settingsbuttonbg.dds`, `icons/misc/gear.dds`, `slices/hint/hintframefilled.dds`

Roll and end: `crafting/crafting_wheelcirclea.dds`, `crafting_wheelcircleb.dds`, `crafting_runecloud.dds`,
`crafting_itemraritybglarge{r}.dds`, `crafting_rune_middle_static.dds`, `crafting_rune_clockwise.dds`,
`crafting_rune_counter_clockwise.dds`, `landingscreen/mapmask.dds`, `hud/tutorialmessage/tutorialmessagemask.dds`,
`slices/drawnline/drawnlinethin1right.dds`, `skills/tradeskills/skilllevelcircleprogressbg.dds`,
`skilllevelcircleprogressfill.dds`, `skillcirclepip.dds`, `lootticker/lootticker_pulse.dds`,
`icons/objectives/icon_territorystanding_2.dds`, `icons/misc/icon_arrow_continue.dds`.

Bar and footer: `slices/screenheader/screen_header_bg_2023.dds`, `icons/misc/icon_back_dropshadow.dds`,
`icon_crowns.dds`, `slices/hint/hintframefilled.dds`, `tooltip/tooltip_divider.dds`.

Fonts: `lyshineui/fonts/pica_regular.font`, `pica_italic.font`, `nimbus_medium.font`, `nimbus_regular.font`,
`nimbus_regular_italic.font`, `nimbus_semibold.font`, `nimbus_bold.font`; Caslon for the slider value and the header
data.

## Open questions

- The exact color of the recipe text when only the trade skill level is missing: the script has ambiguous nesting;
  the screenshot confirms red.
- `ItemName` of the resource: depends on the item's `UsesRarity()`; if false, `#cccccc` without `ItemRarity`.
- Title bar: the port shows only coins. The trade skill circle is still to do, with a screenshot of the original to
  compare. Remaining fixes and the maintainer's pending tests: `docs/notes/ui.md`.
