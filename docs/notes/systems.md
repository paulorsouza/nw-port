# Notes: systems and Everfall

History of the systems area: dated decisions, discarded ideas, measurements and state snapshots. Current rules:
[game/items.md](../game/items.md), [game/crafting.md](../game/crafting.md), [game/city.md](../game/city.md),
[game/economy.md](../game/economy.md), [game/dungeon.md](../game/dungeon.md),
[game/world/everfall.md](../game/world/everfall.md). Tests: [tests/systems.md](../tests/systems.md).

## 2026-10-12

- The maintainer decided that the game's quests and lore will not be converted. Level comes from a short tutorial of
  simple missions, level 1 to 60 in about one hour, rewarded in whole levels ([game/city.md](../game/city.md#character-level-and-xp)).
- The maintainer decided what NPCs offer without quests: groups by trade, starting items, weekly gift, marker only on
  the drawn NPC ([game/city.md](../game/city.md#quests-and-gifts)).

## 2026-10-13

### State at the start of the day

- Area items 1–4 done and tested by the maintainer: city, NPCs with gifts, refining stations with the game screen,
  storage. Level through tutorials (part of item 8). Market and housing backend ready, no screen.
- Next on screens: the refining fixes listed in [game/ui/README.md](../game/ui/README.md), then storage, attributes,
  trade skills, inventory and mastery (with the combat area).
- The refining screen (`craftingscreenv4` in refining mode, Lua styles and positions) was checked with the offline
  capture and tested by the maintainer.
- Early survey of interaction: the E key reached only `UNWGatherComponent`; there was no generic interactable. The
  reusable path (focus, text, RPC with `InReach`, `ANWHUD::DrawGather`, progress ring) is what later systems used.
- `fix_spawn.py` put the character inside the square's floor and it fell through the city and terrain; the spawn point
  is now set with `tools/ue/spawn_trace.py`.

### Item and crafting rules

- Item rules decided with the maintainer: damage types, one attribute, quality, material, perks with fixed ranges,
  slots, bind, signature ([game/items.md](../game/items.md)). They do not follow the original game.
- Perk decisions exported from the maintainer's "perk cut" page to `tools/perks_decisoes.json`.
- Gems: no 1% gem; the 4 cuts give 2–5%.
- Crafting rules decided with the maintainer: instant refining and crafting, training below level 50, refined material
  of the tier only, station progression by invested material ([game/crafting.md](../game/crafting.md)).
- Original crafting rule: with no room in the bag, crafting was refused (nothing lost). Superseded the same day by the
  bag and ground decision below (the item goes to a ground bag).
- Crafting at the Forge, Workshop and Arcane Repository implemented and tested by the maintainer.
- The 4 training dummies were removed from the middle of Everfall at the maintainer's request; the city list stays
  empty until there is a place for them.

### Discarded: crafting with time and spectres

Built, tested by the maintainer and discarded the same day ("the spectre idea is bad"). Recorded so it does not come
back:

- Crafting took hours (base 3 h at T2 to 12 h at T5, minus up to 50% by trade skill level and up to 25% by items).
- Without a spectre the character was stuck at the station; with a spectre (up to 5, one per station, unlocked at level
  50) the character could leave and the item went to the city storage at the end. Only the owner saw the spectre.
- Cancelling returned the material and lost the time. Delivery to storage used one key per craft in nw-core to avoid
  duplicates.
- Removed from code, data and nw-core (migration `0008` kept only `props`).

### Shop

- The maintainer decided on a lean shop, essentials only, no generic items ([game/city.md](../game/city.md#shops-and-merchants)).

### Armor and jewelry

- Survey of the game data (recipes, item definitions, appearance) for the area's item 2; port state then: one character
  mesh with the heroicheavy armor merged, `NWEquip` with 7 slots (weapons and tools), armor and jewelry slots shown as
  empty icons (`NWInventoryMenu.cpp` `CombatEmpty`) with no click, result screen without `Stats/Armor` and
  `Stats/Trinket`.
- Plan: 1) items and mechanics without visuals (60 armoring and 12 jewelcrafting recipes, Armoring index 13, equip slots
  7–14, click and drag, resistance in tooltip and `Stats/Armor`); 2) visuals (one mesh per slot); 3) effect of
  resistance and weight class in combat, with the combat area.
- Decisions: plate, leather and cloth (no dress); jewelry straight from the precious ingot; resistance by the game's
  proportion, physical = magic; mechanics before visuals (phase 1 first).
- Phase 1 done the same day, no visuals (details in [game/items.md](../game/items.md#implementation-mechanics-no-visuals)).
  Missing: visuals (phase 2) and combat effect (phase 3). A long ability perk description overflows its line on the
  result screen.

### Bag and ground

- The maintainer decided: 500-slot bag; a full bag sends incoming items to a ground loot bag; dropping creates a loot
  bag (`ANWLootBag`), 2 minutes (changed to 3 on 2026-10-14), 1000 kg per player.

### End of the first session

- Inventory with drag, split, drop on the ground and search; new itemization and crafting done and tested by the
  maintainer: crafting weapons, shields and tools at the Everfall stations with the game's result screen, "Improve
  items" tab (reroll attribute, reroll GS, unbind), serial number registry in nw-core. Tests 41–54.
- Next steps of the area, in this order (the maintainer asked to finish mechanics before refining filters and screens):
  1. Gems and runeglass (done the same day, waiting for the maintainer's tests 55–60).
  2. Armor and jewelry (phase 1 done, tests 66–69; phases 2 and 3 open).
  3. Effect of perks and attribute on the character, with the combat area.
  4. Reroll GS as a step around the current GS; balancing (curve GS → attribute, improvement costs, perk ranges,
     training material and XP, station progression with invested material).
  5. Shops, Trading Post and housing (the market already refused bound items and checked serials).
  6. Result screen polish: roll sounds and end buttons done (tests 64–65); rarity flipbooks (`RarityEffect1..4`) and
     recipe list filters open.

### End of the second session

- Gems and runeglass; editor Play with local nw-core and a fixed account
  ([tech/backend-testing.md](../tech/backend-testing.md)); result screen buttons and sounds; armor and jewelry
  (mechanics); 500-slot bag, overflow to the ground, ground loot bag (`ANWLootBag`, 2 min, 1000 kg); network test stack
  with Cloudflare Tunnel ([tech/network-test.md](../tech/network-test.md), `tools/linux/network.sh`). Commits `1d7fc8f`
  and `7667ce9`; the rest was left uncommitted.

### Session pending list (in order) and outcome

1. Commit and push what was left out (ask the maintainer): 500-slot bag and `ANWLootBag` (`NWLootBag.*`,
   `NWInventory*`, `NWInteractable.*`, `NWCrafting.cpp`, `NWHUD.cpp`), `MAX_BAG_SLOTS = 500` in `backend/`,
   `NWUIShotCommandlet.cpp` (`WITH_EDITOR`), `tools/linux/network.sh`, `deploy/compose.rede.yaml`, the network test doc,
   docs and tests 70–73. Not `bin/nwbt` nor files of other areas.
2. Maintainer tests still open: 55–69 (gems, buttons and sounds, armor and jewelry) and 71–73 (full bag to the ground,
   drop and pick up the sack, 2 minutes and 1000 kg). Scroll (70) and jewelry already tested.
3. Network test with 2 PCs: `tools/linux/network.sh up`, check `https://<NW_NETWORK_HOST>/health`, package the Windows client
   ([tech/network-test.md](../tech/network-test.md)) with `Jogar.bat` on `-nwconnect=<PC IP>:7777`, account through
   `tools/linux/network.sh invite`. Then, with friends: Zero Trust and WARP (the ISP uses CGNAT), reserve the PC's IP in
   DHCP, update `cloudflared` (2026.8.3 → 2026.10.0).
4. Ground bag screen like the game's: done 2026-10-14 ("Dropped Items" window, take item by item, lifetime warning;
   tests 76–78).
5. Reroll GS as a step around the current GS: done 2026-10-14, with the odds and the material choice on screen (tests
   74–75).
6. Polish: long ability perk description overflows on the result screen (example: Healing Breeze); rarity flipbooks;
   recipe list filters.
7. Armor phase 2 (visuals), agreeing with the other areas before touching `NWCharacter` and the player set; then
   shops, market and housing.
8. Shared storage: done 2026-10-14 (below).
9. Writes without rollback: see 2026-10-14.
10. Server performance: see 2026-10-14.
11. City governance: see 2026-10-14 and 2026-10-15.

## 2026-10-14

### Items and inventory

- The maintainer set the Refine GS rule: 51% success, steps of 1 weighted over 2, never outside the crafting range,
  caps shrinking with GS above the range minimum ([game/items.md](../game/items.md#attribute-and-gear-score)).
- Ground bag lifetime changed from 2 to 3 minutes (the game's `@inv_discard_life_warning` says 3).
- Ground bags saved in nw-core (stage 2 of the writes) so they come back after logout or a server restart.

### Storage and Trading Post

- The maintainer decided on a shared storage: one per character in any storage shed, 1000 kg per known city (houses and
  city level add when they exist), 5 named tabs (migration 0010). Tests 79–83.
- Trading Post decisions by the maintainer: one market for all cities, no fees, 50 orders, only crafted items, cut gems
  or final refined material (`mercado` in `tools/itens.json`), purchase limited by storage capacity. The game fees and
  the limit of 100 stay as reference.
- Own Slate screen (`SNWMarket`) with filters, seller, perks and optimistic response (tests 98–105, capture
  `tools/ui_shots/market.json`). The maintainer approved the style: new screens follow this Slate style; screens copied
  from the game stay. Done after: coin icon, item frame on hover, close button.
- Out of the first version: average price of identical items (sales history, median). Later: an NPC buyer for raw
  material (became part of city governance).

### Writes without rollback

See [tech/server.md](../tech/server.md).

- Stage 1: session and sequence number in every write, queue per character, item log, save every 5 s (migration 0011,
  tests 84–86).
- Stage 2: storage, refine, craft, improve, drop and pick up from the ground recorded before taking effect (`NWOps`);
  ground in nw-core until it expires (migration 0012, tests 89–93).
- Stage 3: gathering, loot, F10, shop, gems, destroy and gifts as operations; bag written only by difference (tests
  94–97).
- Maintainer tests deferred at the maintainer's request: 89–92 (sack comes back, sack expires, nw-core off, full bag
  while crafting), combat with reduced server animation, and the network test (movement at 30/s). Passed: 87 (F10 in the
  log) and crafting as a recorded operation (93).

### Server performance

- Load test (`nw.sim`, `nw.perf`), packaged server with 500 simulated players walking at ~32 ms; animation only with
  montages and camera/focus only for the local player on the server; player grid for AI; movement sent at 30/s.
  Maintainer tests (combat, network) and the open paths are in [tech/architecture.md](../tech/architecture.md).

### City governance: request and plan

- Origin (maintainer request): owner per region, an NPC that buys raw material and sells the list, treasury, city
  stations and buffs, simulated attacks, weekly reward. It includes the raw material buyer NPC from the Trading Post
  work.
- It started as a plan, not a spec: no code until the maintainer approved it. Numbers marked "proposal" come from the
  simulator.
- Maintainer rules on this date: city bought at a fixed price, one owner per region, prices in ranges, city buys only
  raw material and sells a fixed list, everything through the treasury, governor reward bands, dire = forced sale,
  initial treasury, new coins from dungeons, PvP and NPC gifts, target income ~40,000 per week in ~10 h.
- Auction rules: every sale with an owner is a 24 h auction (maintainer).

### Economy simulator: first results

`tools/sim_economy.py` with `tools/economia.json` ("proposal" parameters): 4 cities with active, average, greedy (buys
cheap, sells expensive, 10% tax) and absent governors; 10, 50 and 200 players per region; 12 weeks.

1. Inflation. Coins created ~21,000 per player per week (dungeon 18,800, gift 2,000, rewards); destroyed under 3,000
   (stations, buffs, weapons, city purchase). Coins pile up ~18,000 per player per week. Large sinks outside the city
   are missing (higher crafting cost, gear repair, dungeon entry, etc.).
2. Income. 10 h with half in dungeons (8 min, 500 each) = ~19,000; with gifts ~20,000. For 40,000: 1,000 per dungeon,
   almost all the time in dungeons, or PvP paying the other half.
3. A target based on activity can be gamed. Counting tax and list sales, the greedy governor builds up the treasury and
   ties with the active one; counting only spending, losing an attack and repairing becomes "activity". Proposal: target
   = raw material bought to upgrade and maintain + new projects + average station level; repairs and treasury income do
   not count.
4. Region size. The city's material need does not grow with players; divided per player, the target is small in a full
   region. Proposal: station maintenance proportional to use (crafts in the week), and a target not divided by player,
   comparing cities of similar size.
5. An initial treasury of 150,000 sustains an active governor (about 60,000 left in week 12 with 50 players); the
   average one gets close to zero. Attacks with ±30% variance knock stations down even with just-enough defense: a
   governor needs ~30% margin.

### Economy simulator: second round

The maintainer asked for dungeons paying 1,000 and no new costs. Changes: target = raw material bought to upgrade and
maintain (repairs do not count) + levels reached for the first time + buffs + average station level; station
maintenance proportional to use; band compared with the average of cities that were not dire; active governor keeps 30%
defense margin.

| Players per region | Active | Average | Greedy | Absent |
|---|---|---|---|---|
| 10 | 54,600/week | 32,100 | 11,300 | 5,000 (dire) |
| 50 | 54,200 | 27,900 | 12,500 | 5,000 (dire) |
| 200 | 28,300 | 28,800 | 27,900 | 5,000 (dire) |

- Regular player income: ~40,000 per week (dungeon at 1,000, half the time; gift 2,000). Matches the target.
- Up to 50 players, the active governor stays near the 50,000 band, the greedy one drops to bad and the absent one is
  dire.
- With 200 players the active city reaches level 3 on every station in a few weeks and the treasury passes 1 million
  (tax from many players) with nothing to spend on: a full region needs more goals (more levels, buffs or defense that
  grow with population).
- Coins: created ~40,000 per player per week, destroyed ~800. Refine GS consumes ingots, not coins; demand holds the
  value of ingots, but the coin supply grows ~39,000 per player per week.

### City governance: build

- Order of the plan: 1) simulator and numbers (ranges in `tools/precos_cidade.json`); 2) backend; 3) game server; 4)
  screens; 5) number tuning and maintainer tests.
- Backend done: regions, city, treasury, stock, prices, per-character limits, log, buying and selling the city, week
  rollover (maintenance, target, reward, dire), attacks every 2 days, tax in `state`. Migration 0013,
  `backend/src/routes/city.rs`, rules in `tools/governanca.json`, tests `cidade_compra_precos_limites_e_projetos` and
  `cidade_semana_tenebroso_e_venda_obrigatoria`. Upgrading a station was still immediate (the game's 24 h waited for a
  screen).
- Game server done: `NWCity.h/.cpp`, Town Project Board (`cityboard`) and Territory Planning Board (`governance`) from
  the city slice (`make_crafting.py`), selling/buying and buying the city as recorded operations, tax on craft save
  (`city_tax`), station level (+GS, +minimum quality) and buffs (XP, bonus item) on crafting, coins in the NPC weekly
  gift. The "economy" buff had no effect yet.
- Screens done: `SNWCity` (`NWSlate` style), capture `tools/ui_shots/city.json`, tests 106–115.

### Dungeon request

- The maintainer asked for a simple dungeon (entry item from enemy materials, chests, 500 coins per run, ~5 minutes).
  Recorded by the systems area alongside city governance and left for another area ([game/dungeon.md](../game/dungeon.md)).

## 2026-10-15

- City limits per character per week in the region set by the maintainer: buys up to 5,000 of each raw material, sells
  up to 1,000 of each listed item.
- Coin limits set by the maintainer: 20 million per character, 5 million unit price on the Trading Post, 7-day grace
  before auctioning the city.
- A forced auction with no bid leaves the city without a governor and the outgoing governor receives nothing
  (maintainer).
- v2 adjustments requested by the maintainer:
  1. Territory Planning Board: the weekly target (raw material, items sold, buffs) leaves the screen and stays only in
     nw-core.
  2. Coins: the board's top shows the city treasury (gold bag icon, own color); the player's coins show only where the
     player pays (buying the city, bidding).
  3. City weapons: the game's fort weapons, tier 1 to 3, upgraded with material + coins + construction time; defense =
     sum of tiers.
  4. Stations: tier first, up to 5, then very expensive city levels with a very small buff; tier changes the station
     mesh (separate art step).
- Finding: in the current game the station tier does not lock recipes (almost all extracted recipes need only a Tier 2
  station; project tier gave an azoth discount, `CraftingAzothDiscount`). In the port the tier gives a small bonus and
  visuals, without locking recipes.
- Decisions: stations start at Tier 2; 24 h construction (as in the game); a lost attack removes station levels and,
  with no level left, weapon tiers (the station tier never drops). Rules in `tools/governanca.json` (`mesa_escala`,
  `mesa_nivel`, `armas`, `obra_horas`).
- Backend done (migration 0015): station tier and level, construction in progress (end time), weapons by type with tier
  and construction, material baskets per tier and per level, construction rollover in `advance`, attacks against the
  sum of weapons. Next were the game server (recipes limited by the city station tier, effects), screens and, last,
  the station mesh swap. The recipe lock was dropped by the finding above (unclear in the original whether the game
  server still limits recipes by tier).
- State: first version of city governance closed by the maintainer (backend, game, screens, auction, coin caps, tiers,
  levels and weapons). Tests 106–121. Left for later: station mesh swap per tier on the map, the "economy" buff, number
  calibration with use.
