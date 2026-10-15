# Crafting

Port rules for refining, crafting and station progression. Crafted items (GS, quality, perks, bind, signature):
[items.md](items.md). Gathering trade skills: [gathering.md](gathering.md). Stations as places in the city:
[world/everfall.md](world/everfall.md). Station tier and level paid by the city: [economy.md](economy.md#stations).
Refining screen layout: [ui/refining.md](ui/refining.md).

Game tables live in `extract/data/sharedassets/springboardentitites/datatables/` (written `DT/` below).

## Trade skills

| Index (`NWTradeskill`) | Trade skill | Group |
|---|---|---|
| 0–3 | Mining, Logging, Harvesting, Skinning | gathering ([gathering.md](gathering.md)) |
| 4–8 | Smelting, Woodworking, Leatherworking, Weaving, Stonecutting | refining |
| 9–11 | Weaponsmithing, Engineering, Arcana | crafting |
| 12 | Jewelcrafting | crafting |
| 13 | Armoring (icon `icon_tradeskill_outfitting`) | crafting |

Indexes from `NWTradeskill` (`NWProgressionComponent.h`). Progression uses the game tables up to level 250
(`tools/make_progression.py`, `NWTradeskill::Num`), with the crafter GS bonus per tier for crafting skills.

## Stations

- Everfall stations that craft port items: Forge (weaponsmith), Workshop (engineering), Arcane Repository (arcana) and
  the Outfitting station (jewelcrafting: cut gems; armoring: leather, cloth and jewelry). Plate armor is made at the
  Forge. The Kitchen enters when there is food with an effect.
- Refining stations of Everfall (all T5): Loom, Smelter, Woodshop, Tannery and Stonecutting Table.
- Runeglass: the capsule comes from the Forge and the runeglass (capsule + Pristine gem) from the Stonecutting Table,
  because Everfall does not have the game's rune station.
- A station progresses with material invested in it. No item is crafted to upgrade a station. The city-paid station
  tier and level rules are in [economy.md](economy.md#stations).

## Crafting rules

- Refining and crafting are instant, as in the game. The crafted item goes to the bag.
- Weapons, shields and tools can be crafted only from level 50 of the station's trade skill. Below that the station
  only trains: it spends the material, gives trade skill XP and makes no item.
- Bag full: the item goes to a loot bag on the ground, with a warning; nothing is cancelled ([city.md](city.md#bag-and-ground-drops)).
  An earlier rule refused crafting with a full bag; test 49 in [tests/systems.md](../tests/systems.md) still describes
  the refusal (unclear in the original which one test 49 expects now; test 92 expects the ground bag).

## Materials

- Crafting uses only refined material, one per recipe slot, of the item's tier: ingot, timber and cured leather
  (example: a T3 sword = T3 ingot, T3 timber, T3 leather). There is no choice among alternative materials as in the
  game.

## Bind

- Unbinding an item that is bound on equip is a crafting action at the station of the item's trade skill. The cost
  rises with the item's quality ([items.md](items.md#bind)).

## Game data

- Recipes: `DT/javelindata_crafting_*` (14 files, 8294 rows; `javelindata_crafting` is empty). Columns: RecipeID,
  CraftingCategory, Tradeskill, RecipeLevel, StationType1..4, ItemID, OutputQty, Ingredient1..7, Type1..7
  (`Item`/`Category_Only`), Qty1..7, CraftingFee, UseCraftingTax, BaseGearScore, BaseTier, GameEventID,
  CooldownSeconds, BonusItemChance. Example: `IngotT4`, Smelting 100, `smelting2`, OreT4×6 + IngotT3×2 + FluxT5 +
  CharcoalT1, fee 7.
- StationType = type + minimum tier (`smelting2`, `blacksmith5`, `camp1..5`, `Specialty_*Shop1`).
- Categories: `DT/javelindata_craftingcategories` (1466).
- Levels: `DT/javelindata_tradeskill<skill>` (21 tables, levels 0–500): MaximumInfluence, RecipeLevel, RollBonus,
  GearScoreBonus, XpReward (character XP on level up).
- XP per craft: GameEventID → `DT/javelindata_gameevents` (CategoricalProgressionId/Reward, UniversalExpAmount,
  TerritoryStanding).
- Station tier in the city: `DT/javelindata_territoryprogression` (upgrade projects per station).
- Slices: `slices/structures/settlements/stations/settlement_station_<type>_t2..t5|_alt|master`.
- In the current game, station tier does not lock recipes: almost every extracted recipe needs only a Tier 2 station
  (orichalcum ingot, Asmodeum, T5 sword). The project tier gave an azoth discount (`CraftingAzothDiscount`).

## Refining

- Data: `tools/make_crafting.py` → `Data/Crafting/refining.json`.
  - The 5 Everfall stations, T5, at the slice position: Loom, Smelter, Woodshop, Tannery and Stonecutting Table. The
    name comes from `<Type>T5_MasterName`.
  - 125 recipes known by default from the 5 refining skills, without `TerritoryStandingSupplies*` (they need azoth).
  - Ingredient categories from `IngredientCategories`.
  - Trade skill XP from `gameevents`.
  - Runs in `make_data.sh` before `make_items.py`, which adds the recipe items to the catalog.
- World: `ANWStation`, created by `ANWNpcField`, shows the card "[E] Refine" (`ui_prefab_refinery_interact`).
- Server: `UNWInteractorComponent::ServerRefine` → `FNWCrafting::Refine`.
  - Checks the station within 5 m, tier, trade skill level (free in PIE without backend), materials and fee.
  - Category ingredient: spends first the item the player has most of.
  - Grants the product and the trade skill XP.
  - Instant.
  - Bonus item: chance = `BonusItemChance` + reagent tier adjustment + `RollBonus` of the level × 0.0001 (RE question
    `sistemas-001`, ids `crafting.bonus_*`; Smelting 250 = 30%, same as the game). Quantity: floor(chance) + 1 with
    the fractional part (H, game server).
- Screen: `FNWRefiningMenu`, built from the `craftingscreenv4` pieces with the Lua styles and positions
  ([ui/refining.md](ui/refining.md), `tools/ui_screens/refining.json`), checked by the offline capture
  (`tools/ui_shots/refining.json`). List on the left; on the right product, trade skill level, have/need materials,
  fee, quantity (−, +, Max) and Refine. What is missing on the screen: [ui/README.md](ui/README.md).
- `NWHUD` exposes the station screens to the controller (`IsStationMenuOpen`, `CloseStationMenus`,
  `StationMenuClick`, `StationMenuWheel`).
- Dev: F10 > "Dev bag (weight)" toggles max weight between 1,000 and 10,000,000 (not saved).

## Crafting gear

- Stations: Forge (`blacksmith`, Weaponsmithing), Workshop (`engineering`, Engineering) and Arcane Repository
  (`alchemy`, Arcana) from the city slice. 33 recipes for weapons, shields and tools T2–T5 (`gear` in
  `refining.json`, with fixed refined material of the tier) and 9 alchemy recipes (T5 health potion, T2 reagents),
  instant like refining. Armor and jewelry recipes: [items.md](items.md#implementation-mechanics-no-visuals).
- Ice Gauntlet T2–T5 (`Procedural_GauntletIceT*`, Arcana, Arcane Repository): leather, cloth and ingot of the tier and
  the game's `ArcanaWater` as the water of the tier (T2 `AlchemyWaterT1`, T3 `AlchemyWaterT2`, T4 `AlchemyWaterT3`, T5
  `EssenceWaterT1`, the waters of the game's named gauntlets of each tier). `AlchemyWaterT3` and `EssenceWaterT1` are
  crafted at the same station (game recipes); `AlchemyWaterT1` comes from gathering.
- Bags AT2–AT5 (`Procedural_BagAT*`, Armoring, outfitting station): leather, cloth and ingot of the tier (fixed, like
  gear) and a Holding Rune of the tier. The rune comes from the game's faction shops; in the port the town merchants sell
  it at the game's coin price (`BuyCurrencyCost`: 250.00 / 500.00 / 1,000.00 / 1,500.00, without faction tokens). The
  bag comes out instant, without gear score (`EncumbrancePerGS` is not used).
- Server: `FNWCrafting::Craft` / `UNWInteractorComponent::ServerCraft`. Below level 50 it is training (XP only). From
  50 on the item is rolled (`FNWItemRules::Roll`, range `FNWCrafting::GsRange`) and goes to the bag. A signed item
  (quality ≥ 80%) gets its serial number from nw-core first (migration 0009, `backend/src/routes/serials.rs`).
- Improve ("Improve items" tab of the station): `ServerImprove` / `FNWCrafting::Improve` with `ENWImprove`:
  - `Attribute`: another attribute;
  - `GearScore`: refine GS (51% up, otherwise down, 1 more likely than 2, inside the crafting range,
    `FNWCrafting::GsOdds`; rules in [items.md](items.md#attribute-and-gear-score));
  - `Unbind`.

  Paid in the material chosen on the screen (`FNWCrafting::ImproveMaterials`, `materiais_melhoria`). Provisional cost
  in `FNWCrafting::ImproveCost` (`tools/itens.json` `_custos`, `custo_melhoria`). Capture: `tools/ui_shots/melhorar.json`.
- Result: `ClientCraftResult` (item before and after) → `UNWInteractorComponent::CraftResult`. The screen waits for
  the `Seq` and plays the game's gear roll (`FNWRefiningMenu::DrawGearRoll`).
- Capture: `tools/ui_shots/forja.json` (recipes and the roll midway and at the end). Dev (F10): "+50 levels in crafting
  skills" and "Crafting materials".

## Gems and runeglass

- Game recipes (`javelindata_crafting_jeweler`, `_weapon`, `_refining`): cutting at the Outfitting station
  (`outfitting`, Jewelcrafting, index 12 of `NWTradeskill`); runeglass capsule at the Forge and runeglass at the
  Stonecutting Table, tier 5, level 180 (the game uses the rune station, which Everfall does not have). Same path as
  refining (`FNWCrafting::Refine`).
- Gem effects, socketing and removal: [items.md](items.md#slot-and-gems).

## Result screen

- End of the gear roll: buttons Continue to Craft, Equip (only with an equippable item in the bag; tools show "Equip
  Tool") and Exit Station, in the game's `ActiveButtonsContainer` (x 232, y 520, 370 wide column, spacing 8; reverse
  engineering, ask the `nw-re` agent). They replace "Click anywhere"; clicking outside still returns to the station.
- Sounds (same RE source): `tools/extract_audio.py` group `ui` → `extract/audio/ui/crafting` (102 files, 21 events);
  `tools/make_ui_sounds.py` reads the `.txtp` files and writes layers and variations to `Data/Audio/ui_sounds.json`;
  `tools/ue/import_ui_sounds.py` (stage `sistemas-05`) imports into `/Game/NW/Audio/UI`. `FNWRefiningMenu::Sound`
  picks one variation per layer (rarity plays two layers together). They play on: opening and closing the station,
  picking a recipe, the Craft/Refine button, refining done (1.4 s), rarity (0.5 s), each perk and the gem. Skipping the
  roll does not play the pending ones.
- Recipe list shows 16 visible rows (the 17th was cut).

## Open questions

- Material and XP for training (today: the recipe's).
- Material and levels for station progression (city-paid tiers and levels: [economy.md](economy.md#stations)).
- Crafting time: no source (recipes only have CooldownSeconds; the port crafts instantly).
- Current tier of each Everfall station in the game: no source (game server state).
- Kitchen: enters when food has an effect.
- Result screen polish: rarity flipbooks (`RarityEffect1..4`, textures outside the layout) and recipe list filters.
