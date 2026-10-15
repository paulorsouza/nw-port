# Inventory, equipment and storage (screen spec)

Sources: the game's UI scripts (`extract/lua/lyshineui/`) and canvases (`extract/ui/lyshineui/inventory/newinventory`,
`equipment/equipmentv3`, `container/catcontainer`, flattened with `tools/ui_canvas.py`). Evidence: **C** = Lua code,
**R** = canvas, **H** = hypothesis. Coordinates in 1080p. `ui_canvas.py` draws the screen containers of these screens
490 px higher; that offset is already added here. Screenshots of the original: `inspiracao/02`–`10`.

Almost everything in these screens is built by the Lua (tiles, sections and slots are instantiated slices); the canvas
preview alone does not show the screen. Work cycle and LyShine rules: `docs/game/ui/README.md`.

## 0. Inventory window (R)

- `Content` x 966–1589 (with `SetLocalPositionX` 300; 0 when storage opens, see §4). Frame `FrameMultiBg` 988,62
  587×932 with `headerbg.dds`. Icon `icon_inventory.dds` 990,55; title 1058,56.
- Search 1002,113 360×45 (`primarysearchbg.dds`); filters 1367,113 45×45; 4 sort buttons 34×45 from x=1421 to 1523.
- List 1002,184 573×742. Coins in the footer at y=942.
- Ground drop zone `GroundDropTarget` to the right of the window (x 1576–1899 with the inventory alone).

## 1. Tile and drag (itemlayout.lua, itemdraggable.lua, popup.lua, common.lua)

- Modes (itemlayout.lua 165-187): EQUIPPED=1, CRAFTING=2, CONTAINER=3, INVENTORY=6, TRADING_POST=7,
  EQUIPMENT_WEAPON=9, P2P=13, LOADOUT_EQUIPPED=16.
- INVENTORY/CONTAINER: root and ItemTier scale 1.06 (4253-4274, 4418-4439); EQUIPPED/EQUIPMENT_WEAPON turn on
  ItemFrame (4071-4109, 4354-4372).
- Highlight per layout: itemHighlightLarge/LargeWeapon/Circle/Square.dds (206-209). Lock salvagelock.dds (216). New:
  spritesheetnewitem{rectangle|square|circle}.dds 1.5 s; damaged: spritesheetdamageditem* (218-226).
- SetLayout (2532-3776): quantity is hidden on SQUARE with qty ≤ 1 (2797-2832); RECTANGLE has no quantity; CIRCLE
  shows it. Durability only on RECTANGLE or on an armor SQUARE. Icon = tile − 6 px (3085-3101); ItemRarityBg width
  w − 1.
- Red icon #FF7171 if the player does not meet a requirement (3000-3029).
- ItemSortValueText (OnFilterChange 7128-7245): sort by weight → icon icon_weight (scale 0.7) + unit weight /10 with
  1 decimal, tier hidden; by gear score → icon_filter_gearscore (scale 1.2, yOffset 4) + GS if > 0, tier hidden;
  others → text hidden, tier back.
- Tier: Roman numeral text (GetRomanFromNumber); hidden in CRAFTING.
- Dimmer (6140-6198): ItemDimmer opacity 0.65 in 0.3 s QuadOut when not usable; linear fill from the bottom
  (cooldown).
- Highlight (EnableHighlight 1199-1265): 0→1 in 0.1 s QuadOut, holds 0.1 s, pulses 1↔0.5 (0.3 s / 0.35 s, hold
  0.05 s; common.lua CreatePulse 491-678). Off in 0.1 s QuadIn. Only if draggable.
- Tooltip (itemdraggable): 0.25 s after scrolling or 0.1 s after release; slow mouse (vel² < 10000) for 0.1 s; fade
  0.05 s (instant) or 0.15 s; anchored to the cursor (10), to the right.
- Drag (OnDragStart 3124-3741): own canvas, draw order 999; 75×75 clone (Properties.DraggingSize) centered on the
  cursor, no opacity; source fades to 0.5 (3476); target receives Paperdoll.EquipmentSlotsToHighlight. OnDragEnd
  (3768-4005): source back to 1, no animation. Drop is invalid if the cursor did not move.
- Ctrl (split): OnReleased calls InvokeStackSplitter (625-739) → popup @ui_split_stack_hint (Split_Stack_Popup).
  Popup (popup.lua ShowCustomPopup 3883-4040): does not open with qty ≤ 1; slider 1..qty, initial q − floor(q/2);
  shows stays/takes and weights; two ItemLayout at scale 1.25; buttons Cancel (@ui_cancel), Drop/Store
  (@ui_tooltip_drop / @ui_move / @ui_tooltip_send / @ui_tooltip_take), Equip (@ui_tooltip_equip, disabled
  @inv_cannotequipitem).
- Shift + click: OnStoreItem (quick move). Alt: link. Single click: opens/locks the actions flyout (2583-2641). Double
  click: does not exist in itemdraggable.
- No "selected" frame (itemframeselected is not used).

## 2. Inventory list (DIL=dynamicitemlist, CS=catsection, IH=inventoryheader, IF=inventoryfilter, WB=weightbar, NI=newinventory)

### Sections (DIL OnInit 505-737, defaultSectionInfos; title ItemCommon:GetItemClassName, _common/itemcommon.lua 526-603)

- Left: OutpostRush, PvPArenas, Catacombs, Rewards (owg_rewards_title), Weapon (ui_weapons), Armor (inv_apparrel),
  PatternItem, Runes, MountItem, Consumable (inv_utilities "Utilities"), BasicFood, AttributeFood, TradeSkillFood,
  Cooking, Bait (inv_baits), CraftingModifiers, Material (inv_resources).
- Right: Tools (inv_tools), Instruments, TuningOrbs, Charms, JewelCrafting, Refining (inv_refining "Reagents"),
  Smelting, Leatherworking, Weaving, Woodworking, Stonecutting, Alchemy (inv_alchemy "Arcana"), Dye, Furniture, Quest,
  Lore.
- An item goes into the section if slot:HasItemClass(itemClass) (CS DoSlotUpdate 1233-1244). An empty section is
  hidden (SetHiddenWhenEmpty).
- Columns: default 2 (DIL 267); items per row = 8 / columns (4 or 8); cell 60×60, HorzSpacing 2, VertSpacing 5 →
  step 62×65.
- Section: header 50 px; the list starts at the bottom of the header + ListMargin 10, +41 in 2 columns
  (twoColumnOffsetY) → list at y=101 of the section in 2 columns; height = ceil(n/perRow)×65; no space between
  sections; masonry: each column stacks its own; right column at X anchor 0.5147; in 1 column it shifts x=13; header
  width 538/columns.
- Footer for filtered items (25 px): @ui_filtereditems.

### Header (IH, CS)

- Title FONT_STYLE_INVENTORY_SECONDARY_TITLE: Pica 27, #C6B694, spacing 30, shadow, uppercase. Weight
  FONT_STYLE_INVENTORY_SECTION_WEIGHT: Pica 27 #B2AEA1, "%.1f" of weight/10, with icon_weight.dds 12×12 #B2AEA1. Line
  #FFF1CE.
- Arrow: dropdownarrowwhite.dds 18×18 #C6B694 (focus #EBE3D6), no background, rotation 0 open / −90 collapsed in
  0.2 s QuadOut; 30×30 button at (−8,18,22,48).
- Buttons on the right (right to left): TransferAll (icon_smallArrowNarrow.dds, scaleX −0.5 in the inventory
  "@ui_dropall"/"@ui_storeall", +0.5 in storage "@ui_takeAll"), RepairAll (hidden in inventory and storage),
  OpenSalvage (icon_tooltip_salvage.dds). Step 34; in 2 columns, buttons and weight move to a 2nd row (+41).
- Dragging a section: DragHighlight button_simple.dds #646464, 0.8 on the dragged one / 0.2 on the others.

### Sorting (IF 823-882)

- CHRONO=1 (default, newest first), WEIGHT=2, GEARSCORE=3, TIER=4; clicking the active one reverses the direction.
  Saved in Options.Misc.InventorySortMode/Direction.
- Inside a section: containers first, then the key (largest first), ties by arrival.
- 34×45 buttons: icon_filter_chrono.dds 33×33, icon_weight.dds 22×20, icon_filter_gearscore.dds 30×30,
  icon_filter_tier.dds 30×30; color #A5987D, selected/hover #EBE3D6 (0.1 s); 34×2 bar #EBE3D6 under the selected one.
- Tooltips @ui_sort_chrono ("Sort by Time: Newest - Oldest") etc.

### Search and filters

- Search 360×45, placeholder @ui_searchitem "Search" Nimbus medium 25 #B2AEA1, icon_magnifying_glass.dds; "?" with
  @ui_filterDescription.
- Filters button icon_mapfilter.dds; yellow badge #FFDB7A with the number of visible sections when < total.
- Popup: background 0→0.8 in 0.2 s; box y −10→0 and 0→1 in 0.1 s; "Columns" (1/2), "Filters"
  Equipment/Materials/All, list of sections with a checkbox, column (filteraligntoggle_left/right.dds), move up/down.

### Weight at the top (WB, NI)

- "888.7" FONT_STYLE_WEIGHT_VALUE_EQUIPMENT_2023 Pica 28 #EBE3D6 + "/ 1476.4" Pica 28 #B2AEA1; icon icon_weight.dds
  16×16; above 100%: #FF7171 and pulses 0.4↔1; text "Encumbered" Pica 23 #FF2E2E.
- In the inventory the horizontal bar is off: the radial encumbrance/encumbrancefill.dds 68×68 appears around the
  inventory icon.
- Overload: EncumberedHighlight #B42F0E at 0.35 in 0.3 s.

### Footer and title

- Coins: GetLocalizedCurrency, Pica 28, icon_crown.dds 16×18, x≈1005; azoth/umbral left out in the port.
- Title @ui_inventory: Pica 36 #EBE3D6 spacing 100 shadow uppercase; icon_inventory.dds 68×49 #EBE3D6;
  LineHorizontal #FFF1CE at 0.7.

## 3. Equipment (equipmentv3.lua, equipmentitemdroptarget.lua, weightbar.lua)

Rectangles [x,y,w,h] in 1080p with +490; canvas X (the Lua moves it: contentPosX −330, contentPosXLeft −630,
viewPosX 30).

### Slots

All 63×63; ItemBg equipment/equipmentbg.dds 70×69 9-slice spilling 5 px right/down; empty icon at 0.20; lock
equipmentlock.dds 14×18 #aaa; item at scale 1.06.

- Armor, left column x=30, step 68: head 216 (iconhead), chest 284 (iconchest), hands 352 (iconhand), legs 420
  (iconlegs), feet 488 (iconfeet), shield 556 (iconshield). Background ArmorBg [22,202,98,436]
  equipmentstatsbackdrop.dds at 0.5.
- Right column x=548: weapon 1 216, weapon 2 284 (iconweapon), heart gem 352 (iconrune), amulet 438, ring 506,
  earring ("token") 574. Backgrounds WeaponsBg [542,205,80,226], TrinketsBg [542,427,80,226] at 0.5.
- Quick consumables 2×2: [30,664] [100,664] [30,732] [100,732] (iconquickslot); background [20,656,155,151] at 0.5.
- Tools tab: instruments on the left (216..488), bags 592/660/728 (iconbag); on the right x=548: logging 351
  (iconlogging), mining 419 (iconpickaxe), skinning 487 (iconskinning), harvesting 555 (iconsickle), fishing 623
  (iconfishing), azoth staff 728. Backgrounds ToolsBg [540,338,82,366].
- Mount tab: buffs [30,379] [30,447], accessories [30,557] [100,557], selection button [28,270,70,71]
  mountframe.dds.

### Tabs

ViewsTabbedList [-2,74,170,56], 55×55 buttons with a 64×64 icon, margin 9;
equipment/tab{WeaponsArmor|Tools|Mounts}{Selected|Unselected}.dds; switch 0.05 s; hover pulses 0.65↔1.

### Equip load (weightbar)

- Text "Light/Medium/Heavy/Over-Equipped" [29,144] Nimbus medium 25 (green #30e5ab / #fff1ce / #ffdb7a / #ff7171,
  0.3 s); icon icon_weight_dropshadow.dds [27,174,20,24]; value [49,168] Pica 28 #ebe3d6; background
  [19,145,100,65] backdrop2023 at 0.15.
- Vertical bar 405×5 (x 105–110, y 217–622), background tilingbarbg.dds, fill vitalsbarfill.dds #9e947e from bottom
  to top, frame 1pxframe #8f8473; maximum 50.0 at the top [126,209]; dividers weightbardivider.dds 2×7 #e2cc99 at
  13.0 and 23.0 (load ratios); color of the current segment (green/cream/yellow/red).

### Level and gear score (right)

Level [576,114] Pica 48 #ebe3d6 with reward_xp.dds 48×48 [524,103], black glow 0.3 behind; gear score [553,158]
Pica 48 with icon_gearscore_dropshadow.dds 40×40.

### Attributes and resistances

Background [10,810,174,202] backdrop2023 at 0.29; label x=32 Nimbus medium 25 #c6b694 (@ui_*_short), value x=106
Pica 28 #ebe3d6; rows STR 822, DEX 846, INT 870, FOC 894, CON 918, PHYS 950, ELEM 974. Tools tab: trade skill levels
every 24 px from 823 with iconstat*.dds 48×48.

### No panel or frame

Only translucent black backgrounds per group.

### Drag

Compatible slots light up with icon_item_selected.dds 70×69 white (red #ff2e2e if locked); incompatible ones are not
dimmed. Error: red pulse 2×. Universal target (the whole equipment side): dropzoneglow.dds at 0.1 (0.2 red if not
equippable), 0.15 s; dropping equips into the best slot (EquipItemToBestSlot).

## 4. Storage and drop targets (catcontainer.lua, newinventory.lua, equipmentv3.lua, *droptarget.lua)

y already includes +490.

### Storage (catcontainer SetContainerMode 1821-2593, CONTAINER_MODE_STORAGE)

- Window: width 623, X -15 (anchor/pivot on the right) → x 1282..1905. FrameMultiBg x 1304..1891, y 63..995; footer
  FooterBg 68 px (y 927..995) black at 0.5; FrameHeader height 121 (y 63..184); LineHorizontal2 598 px at y≈908,
  #fff1ce at 0.7. Enter: fade 0.3 s QuadOut; exit 0.2 s.
- Title: PrimaryTitle x 1374, y 57..97, Pica 36, white, uppercase; text = Hud.LocalPlayer.InteractName (default
  @inv_Storage "Storage"; "Storage Shed" = @ui_global_storage).
- Icon: lyshineui/images/catcontainer/icon_storage_shed.dds, #ebe3d6, x 1321 y 59, 36×36. It can pulse red #ff7171
  (likely: slots full). Tooltip @ui_inventoryslots_tooltip.
- Weight: WeightBar Set{x=36,y=100} width 556 → x 1318..1874; radial encumbrance/encumbrancefill.dds 68×68 centered
  on the icon (~1339,77). Value "%.1f" Pica 33 white + "/ %.1f" Pica 24 #c8c8c8, on the right (value x≈1763..1810,
  max x≈1814..1865, y≈61..92). Overage 3%, text @ui_storage_is_full.
- List: the same DynamicItemList, Set{x=36,y=185,w=573,h=742} → x 1318..1891, y 185..927, with sections.
  Search/sort bar 63 px above (y 122..152): search 360×45 + 4 buttons 34×45.
- Settlement selector (personal storage only): x 1317..1695, y 935..985; buttonbg.dds #2c251b,
  primarybuttonframe.dds, dropdownarrow.dds; Nimbus 32 white; "Name <font color=#edc63e>(@ui_current_location)</font>";
  faction icon 50 px; the list opens upward, 560 wide.
- Loot bag mode (CONTAINER_MODE_LOOT_BAG, 1821-1990): Content width 340, X −7; WeightBar 246; title y 57 w 270;
  FrameHeader 183; list Set{x=41,y=246,w=284,h=681} without sections, without SortAndFilterBar; icon
  icon_dropped_items.dds #COLOR_TAN_WARM; ButtonTakeAll x 4, text @interact_takeAll; LootBagInfoContainer (canvas,
  already with +490/−298): LifeTimeWarning x 1324..1585 y 104..184, @inv_discard_life_warning, Nimbus 23 #c8c8c8,
  wrapped, line spacing −4; ItemsText x 1324..1424 y 201..251, @ui_loot_bag_items / @ui_loot_bag_1_item;
  LootBagDivider. Title = InteractName (port: @inv_DroppedItems).
- Shared storage tabs (port only; the game has none): 5 buttons buttonbg #2c251b + primarybuttonframe at y 174..212
  between the search and the list (list from y 222), width split with 6 px gaps; the open tab with the full frame and
  text #ebe3d6, the others at 0.4 and #b2aea1; Nimbus medium 22; while dragging, the tab under the mouse lights up;
  double click opens a Slate field on top for the name. Captures: `inventory.json` `armazem_aba2`, `arrasto_aba`.
- Take all: ButtonTakeAll 170×50, x 1707.5..1877.5, y 935..985, @interact_takeAll, Pica 32; buttonbg #2c251b,
  heroframedisabled2023, additive heroborder, focusnew at 0.1. Callback ContainerTakeAll (3715-3778).

### Inventory and equipment with storage open

- newinventory OnTransitionIn (~4405-4430): Content X = 0 with storage (alone = 300). Inventory x 648.5..1271.5
  (alone 948.5..1571.5). No position tween, only a 0.5 s fade.
- Equipment stays visible, 300 px to the left: equipmentv3 contentPosXLeft −630 (normal −330); ArmorContainer 120
  (normal 25); TradeSkillStats/MountSkillStats 107 (normal 12).

### Weight on the tile

- Only when sorting by weight (inventory and storage, each list with its own sort): ItemSortValueText in the top left
  corner, Pica 24 white.

### Drop targets

- Container/inventory area: dropzoneglow.dds at 0.1 (9-slice), lights up only on hover over the target: 0→1 in 0.15 s
  QuadOut; fades out in 0.25 s. Inventory: x≈652..1273, y 41..1020 (with storage); storage x≈1285..1908.
- Ground (grounddroptarget): only while dragging an item from the inventory/equipment and only with no container
  open; appears in 0.3 s; text @inv_dropTargetMessage "Drop items" Pica italic 30 #dddddd, between
  dropitemtrim.dds 220×27 (on top and flipped below); with the inventory alone it is at x 1576..1899. Drop: a bound
  item asks "Destroy Item?"; a locked/quest item cannot be dropped. With storage open, "drop all" becomes
  @ui_storeall.

## 5. In the port

Implemented in `NWInventoryMenu` (capture `tools/ui_shots/inventory.json`): items 1 to 7 of the plan below, plus
search by name (a Slate field on top of the canvas, no uppercase or accents), drop on the ground (destroys, with
confirmation; the port has no loot bag; `ServerDestroy` with a quantity) and clipping of tiles in the scroll area.

Priority: mechanics before refining the filters.

Known differences from the game:

- Section header lower than the Lua's 101 (measured on screenshot 02).
- The drop target is a border (`dropzoneglow` is 9-slice).
- No lock, actions menu, Equip in the split dialog, settlement selector or filters window.
- "Arrival" uses the slot number (the port does not store the time).

Plan, in order:

1. **60×60 tile**, square or round by type (round: resource, lore, dye, housing item), `itemlayout` layers
   (background and frame by rarity, icon − 6 px, Roman tier, quantity only > 1, lock), highlight that pulses on focus.
2. **The game's sections** by the item's `UiItemClass` (`make_items.py` writes `ui_class`), two masonry columns, 4
   tiles per row, step 62×65, header with arrow, title, weight and move-all button; empty section hidden; collapse.
3. **Sorting** arrival/weight/gear score/tier with the game's icons, clicking again reverses, value in the tile corner
   (weight or gear score). Search with the syntax from reverse engineering. Filters window comes later.
4. **Equipment without a panel**, at the `equipmentv3` positions (only what the port has: weapons 1 and 2, shield,
   quick consumables; tools in the tools tab), 63×63 slots with `equipmentbg.dds`, translucent black backgrounds per
   group, icon tabs, level and gear score at the top, attributes at the bottom. Armor, jewelry, mount and equip load
   stay out until they exist.
5. **The game's drag**: 75×75 copy at the cursor, source at 0.5; compatible slots light up with `icon_item_selected`
   (without dimming the others); the container/inventory area lights up only with the mouse over it
   (`dropzoneglow`); "Drop items" zone when dragging with no container open.
6. **Split stack** with the game's dialog (Ctrl + click; bar that starts at half; stays/takes with weight; Cancel,
   Drop/Store, Equip). Removed: "Ctrl takes half" and double click (the game has neither); click opens the actions
   menu later.
7. **Storage as a second window** on the right (x 1282–1905), inventory at x 648–1271, equipment 300 px to the left;
   title, icon, weight with the ring, the same list with sections, "Take All" (needs a new server action) and
   settlement selector (only Everfall for now).

## Open questions (no source)

- Final size and X position of the equipment after the Lua's `SetLocalPositionX` (check against screenshot 02).
- Texture `icon_item_selected.dds` not found in the extraction: check the path in the pak.
- Exact condition for the storage icon pulsing red (ambiguous decompilation; likely: slots full).
