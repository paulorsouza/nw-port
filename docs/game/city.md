# City systems

Systems of a city in the port: NPCs and their gifts, storage, shops, Trading Post, housing, character level and XP,
bag and ground drops. They run in every city of the table below. Crafting stations: [crafting.md](crafting.md). Items: [items.md](items.md). City ownership and prices: [economy.md](economy.md).
Tests: [tests/systems.md](../tests/systems.md).

Game tables live in `extract/data/sharedassets/springboardentitites/datatables/` (written `DT/` below).

## Cities

| City id | City | Map | Settlement slice | Governance region | NPCs |
|---|---|---|---|---|---|
| `queda_eterna` | Everfall ([world/everfall.md](world/everfall.md)) | `L_QuedaEterna` | `settlement_northwindsward_complete` (capital `ef_persistence`, `r_+04_+02`) | 1 | 30 within 300 m |
| `guardaventos` | Windsward ([world/windsward.md](world/windsward.md)) | `L_SouthWindsward` | `settlement_southwindsward_complete` (capital `06swindsward_persistence`, `r_+04_+01`) | 2 | 27 within 250 m |

- Each map has one `ANWNpcField` with its city's data file (`Data/World/<city>_npcs.json`) and asset root
  (`/Game/NW/Npc<Set>`). The field reads `city` from the file, spawns the NPCs and only that city's stations
  (`city` field of `Data/Crafting/refining.json`), and marks the city area.
- Both maps use the town frame (town-local, UE = (−x, y, z) × 100), so a station keeps its slice position.
- A new city: a row in `CITIES` of `tools/make_npcs.py` and `tools/make_crafting.py`, a region in
  `tools/governanca.json` (`cidade`, `territorio`), the NPC trade skills in `tools/npc_rewards.json`, a shop in
  `tools/shops.json`, `bonecos` in `tools/tutorials.json`, build and import steps in `tools/steps/systems.*`.

## How systems are built

- Survey each system before writing code: game tables, screens ([ui/original.md](ui/original.md), `tools/ui_canvas.py`
  for the layout), what the port already has. When the data has no source, ask the `nw-re` agent.
- A system is done when it works end to end with the dedicated server and nw-core (persistence), has the game's
  screens, and the maintainer has tested it.
- Screens: the game's layouts through `tools/ui_build_*.py` → `Data/UI/*.json`, drawn in C++ (pattern of
  `NWInventoryMenu`, `NWTradeskillMenu`). Follow [ui/README.md](ui/README.md): offline capture with
  `tools/linux/ui_shot.sh`, compare with a screenshot of the original. New screens that the game does not have use the
  port's Slate style (`NWSlate`, first used by `SNWMarket`); screens copied from the game stay as they are.
- Texts: `tools/strings/systems.json` (pt-BR and en-US).
- Persistence: the dedicated server decides, nw-core (`backend/`, Rust + Postgres) stores. A database change is a new
  migration ([tech/server.md](../tech/server.md)). Show the plan to the maintainer before changing the backend.
- Items and catalogs: `tools/make_items.py` and `python3 tools/make_data.py`.
- Testing with the backend: [tech/backend-testing.md](../tech/backend-testing.md) (`tools/linux/backend_dev.sh`
  before opening the editor). Network: [tech/network-test.md](../tech/network-test.md).
- Build and import with the editor closed, open the map and ask the maintainer for a screenshot. Do not run the game
  yourself. Heavy jobs go through the queue (`python3 tools/heavy.py`; on Windows, `python`). Commit and push only
  when the maintainer asks, and only the area's files. Area rules: [workflow.md](../workflow.md).

## Common base

- Dedicated server → nw-core over HTTP/JSON: `UNWBackend` (`NWBackend.cpp`), header `X-NW-Server-Secret`. Internal
  routes: `POST /internal/tickets/redeem`, `PUT /internal/characters/{id}/state` (saves the whole character in one
  transaction, deleting and reinserting the items). The save period and the switch to diff-only bag writes are in
  [tech/server.md](../tech/server.md).
- Migrations used by these systems:

  | Migration | Content |
  |---|---|
  | 0001 | accounts and characters |
  | 0002 | `characters.coins` and `items` (container `bag|equip`) |
  | 0003 | `characters.progression` (`{level, xp, spent[5], trade[4]}`) |
  | 0004 | storage |
  | 0005 | market |
  | 0006 | houses |
  | 0007 | `npc_rewards` |
  | 0008 | item `props` |
  | 0009 | item serials (`item_serials`) |
  | 0010 | shared storage (`shared`) |
  | 0011, 0012 | recorded writes and ground bags ([tech/server.md](../tech/server.md)) |
  | 0013 | cities ([economy.md](economy.md)) |
  | 0015 | station tier and level, city weapons ([economy.md](economy.md)) |

- Storage, market and houses are outside the PUT state. Each action carries the bag and coins after it and writes
  everything in one transaction. Storage has a version (409 `storage_changed`). Routes and rules:
  [backend/README.md](../../backend/README.md). Tests: `backend/tests/api.rs` (`armazem_*`, `mercado_*`, `casa_*`).
- Inventory changes (storage, refine, craft, improve, drop, pick up, gather, loot, F10, shop, gems, destroy, gifts) are
  recorded operations (`NWOps`), each with a reason in the nw-core item log (`tools/item_log.py --character
  <name>`). Details: [tech/server.md](../tech/server.md).
- Interaction: the E key (`IA_Interact`). The reusable path, taken from gathering (`UNWGatherComponent`): focus →
  text → server RPC with a distance check (`InReach`), card `ANWHUD::DrawGather`, progress ring. Near a gatherable
  with no NPC in focus, the card and E stay the gathering ones.
- Game canvases without a build yet: `crafting/craftingscreenv4` (crafting mode), `stationproperties/stationproperties`,
  `contracts/contractbrowser`, `contracts/compensations`, `housing/*`, `conversation/conversationscreenv2`. The
  multi-option interaction card `hud/unifiedinteractcard/interactscreen` is also missing
  (`extract/ui/survey.md`, generated by `python3 tools/ui_survey.py`).

## NPCs

### Game data

- `DT/javelindata_variations_npcs` (1360 rows): VariantID, NPCId, Name, CharacterDefinition (`.cdf`),
  Greet/Bye/Idle/InteractTimeline, LookPoseTimeline. Example: `0407a_Commander` → `0407_commander.cdf`, idle
  `malek_standing_ambient.timeline`. Variants `_walkaway` (257) and `_clientpathing_walkaway` (6).
- Name, title, dialogue: `DT/questdata/npcdata_npc_NNNNN` (NPCId, GenericName, Title, ConversationStateId1..10).
- `DT/javelindata_npcs` is empty.
- Windsward: variants `06_*` (quest NPCs, several copies of each), `0425_HaplessHoweTownWW`, 9 `WW_Ambient_*`, and
  the mission slices `Characters/Mission/NPC_06*` (Verildis Jannsen, Dorothea Chloros).
- Everfall: variants `04xx` (Magistrate, HuntsmanLee, Constable, ArtificerPetrowski, Commander, Alchemist,
  Adjudicator, BarkeepPolly, Innkeep, Fisher, TownGuard and others, 41 in total) and 11 `EF_Ambient_*`.
- Position: entities of the city slice. Quest NPC slices in `slices/characters/mission/npc_04*`.

### Marker

From the UI scripts `lyshineui/markers/markerdata.luac` (type `Conversation`) and `staticworldmarker.luac`, read with
`tools/luac_dump.py`:

- No name: only an icon by conversation state (`eConversationState_*`):

  | State | Icon |
  |---|---|
  | `NoObjective` | `npc_noObjective.dds` |
  | `OnCooldown` | `icon_questCooldown_npc.dds` |
  | `HasObjective` | `icon_questAvailable_npc.dds` (main story: `icon_questMainStoryAvailable_npc.dds`) |
  | `HasTalkToTask` | `icon_questDialogue_npc.dds` |
  | `InProgress` | `icon_questInProgress_npc.dds` |
  | `TurnIn` | `icon_questReadyForTurnIn_npc.dds` |
  | `HasShop` | `icon_npcshop.png` |

- Height 1.9 m (`wZ`). Scale from 1 to 0.35 up to 40 m (`scaleDistance`); fades out from 125 to 250 m (`fadeStart`
  125, `fadeDistance` 125). Shows through walls (`allowWorldOcclusion` false). Interaction card at 1.1 m
  (`interactWorldZOffset`).
- The name shows only on the interaction card, with the NPC in focus.

### Port

- List: `python3 tools/make_npcs.py [city]` → `Data/World/<city>_npcs.json` and the animation set
  `tools/sets/npc_<city>.json` (skeleton `npc_skel`). Everfall: 30 NPCs within 300 m, 16 loops. Windsward: 27 NPCs
  within 250 m, 18 loops.
- Sources of the list: capitals of `r_+04_+01` and `r_+04_+02` within the radius (Everfall: `ef_lore`,
  `ef_persistence`, `04nwindsward_lore`, `ambient_4,1`, `ambient_4,2`; Windsward: `06swindsward_lore`,
  `06swindsward_persistence`, `ambient_4,1`). The variant comes from `variantName` (ambient) or from
  `m_slicevariant` in the spawners `coatgen/<capital>/MasterNpc_*` (build stage `sistemas-02` for Everfall,
  `sistemas-04` for Windsward). Timelines come from stage `sistemas-03`.
- One NPC per name: the game has several copies of a quest NPC (`06_LeovixSilva_03`, `_03a`, `_03B`, `_04`), each
  hidden by a quest step. The lowest VariantID stays.
- An idle animation outside `player/male` (Aelstrom: `g_male_loopidle`) becomes `settle_stand_idle`.
- With no quests, the initial state applies: `MasterNpc_SwapShown_*` is left out.
- Also left out: the `DEPRECATED` variant and Grace O'Malley (own skeleton, quest NPC).
- The IdleTimeline plays an alias (`animationalias`) that is the name of the `.caf` in `player/male/`. The
  `*_look_poses_15x` (look, additive) are not used yet.
- Appearances: `tools/convert_npcs.py [city]` (nwbt + `tools/export_npcs.py` in Blender) → `extract/ue/npcs/SK_*`
  (Everfall) or `extract/ue/npcs_<city>/SK_*`. The nwbt glb cache is shared; an appearance already exported for
  another city is copied.
- Import: `ue_import_all` stage `sistemas-03` (Everfall) and `sistemas-06` (Windsward): `import_nw.py` of the set +
  `tools/ue/import_npcs.py <npcs> <Set> <Map> [<data file>]`, which saves an `ANWNpcField` in the map with its data
  file and asset root.
- C++: `ANWNpc` (capsule, mesh, idle loop, name) and `ANWNpcField` (reads the JSON and creates the NPCs on each machine,
  not replicated, snapping each one to the ground with a trace).
- Talking (E): the NPC turns to face the character. Before talking, each NPC keeps its pose and facing.

### Quests and gifts

Game quests and lore are not converted. NPCs offer gifts instead:

- **Groups by trade:** at most 6 trades per city (the draw only counts the NPCs of the same city) (guard, ore and metals, tailor and hunter, tavern, alchemy and herbs,
  wood and port), with 3 to 4 NPCs each (`tools/npc_rewards.json`). Each week one NPC per group, drawn the same for
  everyone (CRC of city + trade + week), gives gifts in that city: up to 6 NPCs per week. The others in the group have
  nothing.
- **Starting items:** once per character per trade. Materials and consumables only, in ranges (example: 1000 ore,
  100–300 of each reagent).
- **Weekly gift:** once per week per city and trade: 1 random material from the trade's list (quantity range) plus 1
  reagent (100–300), plus 200 to 400 coins ([economy.md](economy.md#coin-flow)). Renews Tuesday 08:00 UTC (05:00
  BRT).
- **Marker:** only on the drawn NPC. `icon_questavailable_npc` when there is something to claim,
  `icon_questcooldown_npc` after. The card says "[E] Claim" (`ui_claim`) or "[E] Talk" (`ui_talk`).

Implementation:

- Data: `tools/npc_rewards.json` → `Data/World/npc_rewards.json` and the `tipo` field in
  `Data/World/<city>_npcs.json` (`tools/make_npcs.py`).
- Rules: `FNWNpcRewards` (`NWNpcRewards.h`): period, NPC draw, items and NPC state.
- Server: `UNWInteractorComponent::ServerNpcInteract` checks distance (4 m) and state, grants through
  `Inventory->Grant` and writes to nw-core (`UNWBackend::ClaimNpcReward`, bag and coins in the same transaction).
- What was already claimed (`RewardClaims`) is loaded from nw-core and replicated only to the owner. Without backend
  (PIE) it stays in server memory.
- Backend: migration 0007 `npc_rewards`. `npc` = trade (starting items, period `once`) or `<city>_<trade>` (weekly,
  period = Tuesday of the week).

## Storage

### Game data

- Slice `settlement_storage_geo_master` (`@ui_global_storage`). Growth by territory standing:
  `DT/javelindata_territory_standing` (301) → TB_GainStorage in `DT/javelindata_progressionpointdata`;
  `DT/javelindata_categoricalprogression` id 4 = Everfall (MaxLevel 300).
- Capacities (`javelindata_encumbrancelimits`, in tenths): `StorageSmall` 150, `StorageMedium` 1000, `StorageLarge`
  1500, `StorageXLarge` 2000 kg. Game screen: `container/catcontainer.lua`.

### Port

- The storage sheds of each city (Everfall 2, Windsward 3), read from the slice (`settlement_storage_geo_master`, name `ui_global_storage`), are
  stations of type `storage` (`make_crafting.py`). The card is "[E] Open".
- One storage per character (`shared` in nw-core, migration 0010), the same in any storage shed of any city. Items
  that were in `settlement:*` and `house:*` were merged into it.
- Capacity = sum of sources, no base: 1000 kg per known city (`tools/itens.json` `armazem_por_cidade`). House chests
  and city level become sources when they exist (`UNWInteractorComponent::StorageCapacityNow`). The server sends the
  capacity to nw-core on character save and on every storage write.
- Known city: entering the city area (center of the NPCs and stations of `ANWNpcField`, radius up to the farthest one
  + 40 m) or opening its storage shed. Each city of the table above counts once (Everfall and Windsward: 2000 kg). Message "City discovered: storage +1000 kg"; saved in `progression.cities`.
- 5 tabs (`armazem_abas`): each item keeps its tab; storing goes to the open tab; the market delivers to tab 1.
  Dropping an item on a tab stores it there (from the bag) or moves the stack to it (from storage,
  `ServerStorageSetTab`). Double-click on the open tab renames it (up to 24 letters, `ServerStorageRenameTab`). One
  weight for all 5 tabs.
- Above capacity (the market delivers even when full): the player can withdraw and sell, not store or buy.
- Server (`UNWInteractorComponent`, `ServerStorageOpen/Move/MoveMany/SetTab/RenameTab/Close`):
  - Depositing respects weight; withdrawing leaves in storage what does not fit in the bag.
  - Writes bag and storage to nw-core together (`UNWBackend::PutStorage`, with version). On 409 it rereads, redoes and
    writes once more.
  - Without backend (PIE) it stays in memory.
- Screen: the game's inventory in storage mode (`FNWInventoryMenu::OpenStorage`, [ui/inventory.md](ui/inventory.md)):
  second window on the right, drag between the two, Shift + click, split stack, move a whole section and "Take all"
  (`ServerStorageMoveMany`, one write).

## Shops and merchants

### Game data

- `DT/javelindata_shopdata` (11 faction/event shops with their own currency). Specialty shops sell by recipe
  (`crafting_specialtyshops`, stations `Specialty_*Shop1`). Currencies in the `*currency` tables. There is no NPC
  vendor table with stock and gold prices.
- Faction shop items and prices live on the game server (the client only has the header in `shopdata`).

### Port

Lean shop, essentials only, no generic items:

- Coins in cents as in the game (`UNWInventoryComponent::FormatCoins`, ÷100 of `GetLocalizedCurrency`). Coins show
  with two decimals (50 coins of a new character = 0.50).
- Potions: only T5 exists in the port (catalog, Lost loot and starting kit; a new character starts with 5 T5 health
  potions). T1–T4 disappear from characters that had them on login (an item outside the catalog does not load).
- Merchants: `EF_Ambient_Merchant_00` (Everfall) and `WW_Ambient_Merchant_00` (Windsward), same list. Card "Shop · [E] Buy" (`ui_shop_default_name`, `ui_buy`), shop icon
  over the head.
- List and prices in `tools/shops.json` → `Data/World/shops.json` (`make_crafting.py`): T5 health potion 2.00; the 5
  T5 refining reagents at 0.20 with a weekly limit of 100 per character (port number, adjustable in the file).
- The limit is per item, shared by the two merchants. It renews with the NPC gifts (Tuesday 08:00 UTC). Purchases of the week in `progression.shop`
  (`{period, bought}`); no backend change.
- Server: `UNWInteractorComponent::ServerShopBuy` checks the NPC within 4 m, limit, coins and weight; charges only
  what entered the bag; saves at once (`ANWGameMode::SavePlayer`).
- Screen `FNWShopMenu` with the game canvas `lyshineui/shop/shop` (`tools/ui_screens/shop.json`): card grid
  (`shopgriditem.lua`, 11×12 spacing) and detail with quantity and a hero button with the cost. At the limit, red
  clock on the card and "Next cooldown" with the time until renewal. No rank panel, no 3D preview. Capture:
  `tools/ui_shots/shop.json`.
- The city's own shop list (potions and refining catalysts sold by the city): [economy.md](economy.md).

## Trading Post

### Game data (reference only)

- Slice `Settlement_Trading_Post_Master`; `DT/javelindata_territorygovernance` (TradingPostLevel); TB_TaxTradingPost
  in standing; TradingCategory/Family/Group in the item definitions.
- Game rules (values in cents, ids `market.*`, RE question `sistemas-003`):
  - Durations 1, 3, 7 and 14 days (default 3), same for buy and sell.
  - Listing fee: `trunc(price × qty × pct) + fixed` by duration (50/2%, 100/2.5%, 200/3%, 400/3.5%), times the
    discounts. A buy order pays price × qty + fee on creation.
  - Transaction fee: 5% of price × qty, times the discounts and the territory rate (1.0 in this build). The buyer pays
    price + fee; a seller filling a buy order gets price − fee.
  - Discounts multiply each other:
    - standing `TB_TaxTradingPost`: sum of max(0.25%; 5% × 0.865^(k−1)) per point, 5% at 1 point, 55.3% at 101;
    - company owning the territory: 30%;
    - faction buff: 5%.
  - Limit of 100 buy and sell contracts.

### Port

The game fees and the limit of 100 above are reference only. Port rules:

- One market for all cities: the server uses the territory `global`.
- No fees: no listing fee, no transaction fee (`fee` = 0).
- Up to 50 open orders per character, buy and sell combined (`mercado.max_ordens` in `tools/itens.json`; nw-core 409
  `too_many_orders`).
- Unit price at most 5 million in sell and buy orders (`preco_item_max`, [economy.md](economy.md#coin-limits)).
- What can be traded (`FNWItemRules::CanTrade`, `mercado` in `tools/itens.json`): only unbound items, and among them
  crafted gear (weapon, armor, jewelry, tool), cut gems (`Cut*`, runeglass) and final refined materials with a daily
  limit (`mercado.itens`: Asmodeum and the other T5.1). Raw material and intermediate refined material (ingots,
  timber, leather and cloth below the final one) cannot. Raw material is sold to the city instead
  ([economy.md](economy.md)).
- Durations 1, 3, 7 and 14 days (default 3).
- Everything goes to the shared storage. A purchase (buy order or buying from a sell order) passes only if storage +
  open purchases + this purchase fit in the capacity (nw-core, 409 `storage_full`; unit weight in `weight` of the order
  and of the storage item). Selling has no limit; an expired sell order returns to storage even above capacity.
- Buy order: price × quantity leaves the coins on creation. Cancelled or expired, the rest becomes credit.
- A sell order filled by another player becomes credit. Credits are redeemed when opening the Trading Post
  (`GET /credits` and redemption up to `until`, written as an operation).
- Selling to a buy order: the item leaves the bag and the coins arrive at once, in the same recorded operation.
- Only near the city's Trading Post (station `tradingpost`, interaction radius), checked on the server.
- Own screen in Slate (`SNWMarket`) with the game's colors and fonts: tabs Buy, Sell and My Orders; table sortable by
  column (name, tier, GS, quality, perks, seller, quantity, price); filters (search by name or perk, category, tier,
  rarity, minimum GS, attribute, perks currently on sale, can afford); item panel with attribute, quality, perks with
  value, gem and crafter. Coin icon on coins and prices, item frame on hover, close button. Selling suggests the lowest
  listed price of the same item.
- Buy orders are not on the screen (almost nothing can be ordered); the server still accepts them.
- Optimistic response: buying, selling and cancelling change the screen at once (spinner until the server writes). If
  refused, the screen goes back and the row flashes red with the reason.
- Capture: `tools/ui_shots/market.json`.

## Housing

### Game data

- `DT/javelindata_housetypes` (5): cost, travel cooldown, minimum standing, item limit. Example: Tier1 500000, 240
  min, standing 1, 100 items.
- `DT/javelindata_housingitems` (1084): PrefabPath, category, tags, StorageBonus, Tier, CraftingRecipe.
- Plots: `slices/housing/plots/northwindsward/cityd_house_address_NNN` (17 addresses: 001, 002, 004–008, 010–014,
  016, 019, 020, 022, 023).

### Port

- Backend ready (migration 0006, `casa_*` tests), no game feature and no screen. Houses on the map are open.
- Scope: plots, purchase, interior, furniture, house storage (house chests add to storage capacity).

## Character level and XP

### Game data

- `DT/javelindata_xpamountsbylevel` (levels 0–69): XPToLevel, AttributePoints, Health, TradeSkillPoints,
  MaxEquippableGearScore, HouseTrophySlots.
- Sources: `gameevents` (UniversalExpAmount, TerritoryStanding), enemies (`vitalstables/javelindata_basevitals_*`,
  `javelindata_vitalsleveldata`, `javelindata_aileveldisparity` KillExpModifier), XpReward of trade skills and
  standing, quests (`territoryprogression_missions`, `questdata/objectives_quest_*`).

### Port

- `UNWProgressionComponent` (level, XP, attributes, XP of the 4 gathering skills), saved in `characters.progression`.
  Screens: XP bar, level banner, Attributes, Trade skills.
- Game quests and lore are not converted. Level comes from a short tutorial of simple missions, from level 1 to 60 in
  about one hour:
  - Everything is a tutorial. Missions are unlocked on the character from the start: no NPC hands out missions, and the
    player picks the order.
  - The reward is in whole levels: the XP missing to rise N levels in the `xpamountsbylevel` table, whose curve stays
    the game's.

  | Tutorial | Example | Reward |
  |---|---|---|
  | Weapon (one per weapon, grows with the combat area) | kill 10 dummies | 2 levels |
  | Gathering (one per trade skill) | pick up 10 wood | 2 levels |
  | Mount ([mounts.md](mounts.md)) | summon and ride | 2 levels |
  | Crafting station (one per station) | craft 1 item at the station | 1 level |
  | Faction (the game's 3) | simple kill or gather mission | 2 levels |

  - After the tutorials, if the player has not reached 60, every enemy gives a lot of XP until 60.
  - Weapon mastery: fast up to level 10 and slow after that, up to full mastery.
- Implementation:
  - Missions: `tools/tutorials.json` (13 so far, 21 levels), copied to `progression.json` by `make_progression.py`.
    - 4 weapon missions (Sword, Hatchet, Bow, Rapier): defeat 10 enemies with the weapon active, 2 levels each.
    - 4 gathering missions: gather 10 times, 2 levels each.
    - 5 refining station missions: refine 1 item, 1 level each.
    - Mount, crafting stations and factions enter when they exist.
  - Events: `FNWEvents` (`NWEvents.h`) on the server. The kill comes from `UNWCombatComponent::Die` (1 line, combat
    area), trade XP from `UNWProgressionComponent::AddTradeXp` and refining from `FNWCrafting::Refine`.
  - Progress: `UNWProgressionComponent::TutorialCount` (-1 = done), saved in `progression.tut` and replicated to the
    owner.
  - On completion: the exact XP of N levels (`XpForLevel`) and the message "Mission complete".
  - After all of them: each kill gives 25% of the current level's XP, up to 60 (port rule, `apos_tutoriais`; the game
    base is RE question `sistemas-004`).
  - Training field: `ANWNpcField` creates the dummies from `tools/tutorials.json` "bonecos" on the server. The Everfall
    list is empty (the 4 dummies were removed from the city center) until there is a place for them.
  - F10 > "Missions" shows how many are done; clicking lists the missing ones.
  - Character XP otherwise only from F10.

## Bag and ground drops

- The bag has 500 slots (one stack per slot; `UNWInventoryComponent::BagSize`, `MAX_BAG_SLOTS` in nw-core). Weight
  gives encumbrance, as before.
- Bag full: whatever arrives (crafting, refining, gathering, F10) goes to the ground in a loot bag, with a warning
  ("Bag full: the item was dropped on the ground"). Nothing is cancelled (`UNWInventoryComponent::Grant`).
- Dropping on the ground (drag to "Drop items" or Ctrl + click > Drop) does not destroy: it creates a loot bag at the
  feet (`ANWLootBag`, the game's `arc_sacks` sack). Only the player who dropped it sees and picks it up. It disappears
  after 3 minutes (the game's `@inv_discard_life_warning` says 3). Up to 1000 kg on the ground per player (overflow from
  a full bag does not count). Saved in nw-core: it comes back in the same place if the player logs out and in or the
  server restarts before it expires ([tech/server.md](../tech/server.md)).
- Loot bag screen (`catcontainer` in loot bag mode): [E] Open opens the inventory with the "Dropped Items" window in
  place of storage (`FNWInventoryMenu::OpenLootBag`): icon `icon_dropped_items`, lifetime warning and "N items" in
  place of search and sorting, bag weight / 1000 kg. Take item by item (drag to the inventory, right-click, Ctrl + click
  > Take with a quantity; `ServerLootTakeItem`), a whole section, or Take All (`ServerLootTake`). Nothing can be stored
  in the ground bag. Empty or expired bag: only the inventory remains. Capture: `tools/ui_shots/lootbag.json`.

## Known differences from the game

- Loot bag window keeps the sections and the width of the storage window (the game window is narrow, 340, with no
  sections).
- Storage is one per character for all cities, with capacity from known cities.
- Trading Post: one market for all cities, no fees, 50 orders.

## Open questions

No source in the game data:

- Base storage capacity and kg per point of TB_GainStorage.
- Station fee and property tax values (the Trading Post ones came from RE question `sistemas-003`).
- NPC vendor stock and gold prices (the shop list and prices are the port's, `tools/shops.json`).
- Base XP per kill: computed on the game server (RE question `sistemas-004`). The tables give a low base (common level
  60 enemy ≈ 69 XP); the XP rule after the tutorials is the port's.
- Which HouseType each plot uses.
- Position of each NPC: depends on extracting the city slice to `extract/slicejson` (unclear in the original whether
  still open; the NPC list already reads stage `sistemas-02` output).

Pending:

- Housing: plots, purchase, interior, furniture, house storage, and the screens.
- Trading Post: average price of identical items (sales history, median) is out of the first version.
- NPC look poses (`*_look_poses_15x`).
- Multi-option interaction card (`hud/unifiedinteractcard/interactscreen`).
- A place for the training dummies in Everfall and Windsward.
- Windsward NPC trade groups (`tools/npc_rewards.json`) were chosen by role from the quest texts (innkeep, mason,
  outfitter, herbalist, captain); the game has no such groups. Housing stays out of Windsward.
- Tutorials for mount, crafting stations and factions.
