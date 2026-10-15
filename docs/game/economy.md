# Economy and territory governance

City ownership without war, city prices and treasury, station progression paid by the city, simulated attacks, the
governor's weekly reward, and the coin limits. Numbers marked "proposal" come from the simulator and are not final.
Trading Post: [city.md](city.md#trading-post). Shops: [city.md](city.md#shops-and-merchants). Crafting:
[crafting.md](crafting.md). Dungeons as a coin source: [dungeon.md](dungeon.md).

## Rules

- A city is bought with coins at a fixed price (whoever pays first takes it) and can be sold to another player. No war.
- A region has one owner: channels 1.1 and 1.2 are region 1, with the same governor and the same limits for players.
- The governor sets prices inside ranges: how much the city pays for each raw material and how much it charges for the
  items it sells. Without a governor, the middle of the range applies.
- The city buys only raw material, nothing crafted. It sells a fixed list: potions and refining catalysts (flux and the
  others).
- Limits per character, per week, in the region: the city buys up to 5,000 of each raw material and sells up to 1,000
  of each listed item. An item can have its own limit (`limite` in `tools/precos_cidade.json`).
- Everything goes through the city treasury. It receives listed-item sales and the crafting tax, and pays for the raw
  material it buys, buffs, defense and equipment. The governor cannot withdraw.
- Raw material bought goes to the city stock and is spent to upgrade and repair stations.
- Station level improves crafting results a little: very little GS and the minimum in %.
- City buffs apply at the stations of that city (in the game they came from the player's house).
- After the governor's first week, every 2 days the city suffers a simulated attack. With weak defense, the city's
  stations and weapons lose levels.
- Governor's weekly reward by result band: dire 5,000, bad 10,000, good 25,000, very good 50,000, excellent 100,000,
  best governor 200,000. Targets are hard and proportional to the other cities.
- Dire: the city goes to a forced sale at the starting price, so it does not stand still. It is not based on hours
  played but on minimum activity (below).
- Every city starts with an initial treasury (value calibrated in the simulator).
- New coins enter the game from: dungeons (solo and group, [dungeon.md](dungeon.md)), PvP, and the NPCs' weekly gifts,
  which give some coins besides raw material (`tools/npc_rewards.json`). Target income for a regular player: ~40,000
  per week in ~10 h.

## Region and channels

- `region` in nw-core: id, name, seat city (Everfall = region 1). Each game server knows its region from the map
  (config). All channels of a region read and write the same state in nw-core.
- Week = the NPC gift period (`FNWNpcRewards::Period`, Tuesday 08:00 UTC).

## City state

Per region: owner, city purchase price, treasury, raw material stock, price table, crafting tax, level of each station,
defense equipment (weapons), active buffs, weekly numbers and an append-only log (like `item_log`).

## Prices

`tools/precos_cidade.json`: range (min–max) and default (middle of the range) per item, in cents.

- Range = 0.4× to 2.1× the game's central market price (`tools/precos_referencia.json`, median of ~36 servers,
  nwmp.gaming.tools), rounded to multiples of 0.05. Examples: iron ore 0.20–1.00; orichalcum ore 0.35–1.85.
- Health potion: 0.20–2.00. Mana: the port has no mana potion yet; it joins the list when it exists.
- Raw T5 in the late game market was cheaper than T4 (plenty of T5 nodes): the port uses 1.2× T4.
- nw-core refuses a price outside the range.

## Coin flow

| Action | From | To |
|---|---|---|
| Player sells raw material to the city | treasury | player |
| Player buys a listed item | player | treasury |
| Crafting tax at the city's stations | player | treasury |
| Buff, defense, equipment | treasury | destroyed |
| Upgrade or repair a station | stock (raw material) + treasury | destroyed |
| Governor reward | new coins | governor |
| NPC weekly gift | new coins | player |
| Initial city treasury | new coins (once) | treasury |
| City purchase | buyer | destroyed (first purchase) or seller (resale, the 10% fee is destroyed) |

- The city does not create coins: the treasury starts with the initial value and then only moves what players pay.
  With no coins in the treasury, the city stops buying raw material. The governor's game is balancing buy and sell
  prices.
- The treasury never goes negative.
- The governor can also sell raw material to their own city, with the same limit as others and receiving at most the
  default (middle of the range), not the price they set.

## Stations

- Stations start at Tier 2. First the tier, up to 5. Each step needs material of every type (ore, wood, fiber,
  leather, stone) in growing quantity, plus coins and construction time (24 h, as in the game). The game projects are
  in `javelindata_territoryprogression` (forge level 1/2/3 = 100 / 5,000 / 10,000 coins).
- The tier gives a small bonus and changes the station visuals; it does not lock recipes (in the current game it does
  not either, [crafting.md](crafting.md#game-data)).
- Tier changes the station mesh on the map (the game's `station_t2..t5` meshes): a separate art step (conversion and
  mesh swap per tier).
- After tier 5: city levels, a very small buff per level (example: +1 GS and +0.5 points of minimum quality), very
  expensive in material, so the city has to buy a lot.
- Costs are smaller for the tiers up to 5; levels are very expensive.
- Effect in the current build (test 120): tier 3 = +1 GS, tier 5 = +3 GS, each level +1, and slightly higher minimum
  quality. Earlier proposal per level: +2 GS on the roll and +1 point of minimum quality, up to level 3.
- Repairing a station (after a lost attack): raw material from stock, less than upgrading. A lost attack removes
  station levels; the station tier never drops.
- Station maintenance proportional to use (crafts in the week).
- Rules in `tools/governanca.json` (`mesa_escala`, `mesa_nivel`, `armas`, `obra_horas`).

## Buffs and tax

- City buff (lasts the week; applies to whoever uses the city's stations): +trade XP, bonus item chance in refining,
  less material ("economy"), lower tax (proposal). Paid from the treasury.
- Crafting tax: % over the reference price of the result, range 0–10% (proposal). Charged when the craft is saved
  (`city_tax`).

## Defense and simulated attacks

- City defense = equipment bought (guard weapons, with tier) + defense upgrades, paid from the treasury.
- City weapons are the game's fort weapons (`javelindata_territoryprogression`, `ProjectType` Fortress): Ballista,
  Repeater, Explosive, Burning Oil Vat and Horn of Resilience (`siege_turret_*`). In the game each one has a tier (Tier 2
  = 500 coins, Tier 3 = 10,000, 24 h). Port: tier 1 to 3 per weapon, upgraded with material (ore, wood, stone) +
  coins + construction time. Defense = sum of the weapon tiers. Natural cap: 5 weapons × 3 tiers, so the treasury
  cannot be spent without end.
- After the governor's first week, every 2 days: a simulated attack with strength that grows with the city level and
  the week (proposal). Defense ≥ strength: nothing happens. Below: 1 to 3 stations lose a level and, with no level
  left, weapon tiers drop.
- Result in the city log and a notice to the region's players.

## Reward and targets

- The weekly band compares the city with the average of the cities that were not dire (proposal): bad < 0.8; good ≥
  0.8; very good ≥ 1.2; excellent ≥ 1.6; best governor = highest score, if excellent. While there is only one city, the
  average is its own over the last 4 weeks.
- Score (simulator, second round): raw material bought to upgrade and maintain (repairs do not count) + levels
  reached for the first time + buffs + average station level. The target is not divided by player; cities of similar
  size are compared. The plan's first definition was coins moved (raw material bought + list sales + taxes) + completed
  projects (stations, defense, buffs), per active player of the region. Unclear in the original which definition the
  backend uses.
- Dire (below bad): in the week the city bought less than 5,000 raw material in total (all types summed), sold less
  than 500 listed items in total, or no buff was bought. If nobody else trades, the governor has to do it.
- Dire: reward of 5,000 and a forced 24 h auction from the starting price (below).

## Buying and selling a city

- No owner: fixed price, whoever pays first takes it; the amount is destroyed.
- With an owner, every sale is a 24 h auction:
  - by the governor: after a 7-day grace period, with the starting bid they choose; they can cancel while there is no
    bid; at the end they receive the highest bid minus the 10% fee;
  - forced (dire): starts at the city's starting price, the owner cannot cancel, and at the end receives the whole
    highest bid.
- Minimum bid: the starting bid or the highest bid + 5%. The bid leaves the bidder's coins and returns as credit when
  someone outbids. The governor cannot bid on their own city.
- No bid by the end: a governor auction ends with no change; a forced auction leaves the city without a governor (back
  to the fixed price) and the outgoing governor receives nothing.
- Treasury, stock, stations and defense stay with the city.
- No owner: prices at the middle of the range, default tax, no attacks and no reward.

## Coin limits

- Per character: at most 20 million coins (`moedas_max` in `tools/governanca.json`).
- Per item: unit price at most 5 million on the Trading Post, sell and buy orders (`preco_item_max`).
- Income above the cap does not enter and does not become credit: the game server cuts it in `AddCoins`, warns the
  player and logs it (server log and `coins_cap` in the nw-core item log,
  `POST /internal/characters/{id}/coins-cap`). nw-core also cuts any write above the cap and logs the excess
  (safeguard).
- Grace period before putting the city up for auction: 7 days.

## Screens

- Town Project Board (`cityboard`) and Territory Planning Board (`governance`), taken from the city slice
  (`make_crafting.py`). Screen `SNWCity` (`NWSlate` style), capture `tools/ui_shots/city.json`.
- Town Project Board: Sell to Town, Buy, Town.
- Territory Planning Board: Summary and Government without a governor; Summary, Prices, Stations, Defense and
  Government for the governor. The top shows the city treasury (gold bag icon, own color). The weekly target
  (raw material, items sold, buffs) is not shown; it stays in nw-core. Player coins show only where the player pays
  (buying the city, bidding).

## Implementation

- Backend: migration 0013 (regions, city, treasury, stock, prices, per-character limits, log, buying and selling the
  city, week rollover with maintenance, target, reward and dire, attacks every 2 days, tax in `state`) and migration
  0015 (station tier and level, construction in progress with end time, weapons by type with tier and construction,
  material baskets per tier and per level, construction rollover in `advance`, attacks against the sum of weapons).
  Routes in `backend/src/routes/city.rs`, rules in `tools/governanca.json`. Tests
  `cidade_compra_precos_limites_e_projetos` and `cidade_semana_tenebroso_e_venda_obrigatoria`. Time can be advanced
  with `POST /internal/cities/advance` and `now`.
- Game server: `NWCity.h/.cpp` (rules, copy of the city per region, RPCs). Selling, buying and buying the city are
  recorded operations (`city_sell:*`, `city_buy:*`, `city_buy_city` in the item log). Station level (+GS, +minimum
  quality) and buffs (XP, bonus item) apply when crafting. Coins in the NPC weekly gift.

## Balancing

Simulator: `tools/sim_economy.py` with `tools/economia.json` (parameters marked "proposal"). Inputs: players per week,
gathering per hour, dungeons, raw material bought by the city, list, tax, projects, attacks and reward. Output: coins
created and destroyed per week, regular player income (target ~40,000 per week in ~10 h), treasury and governor band.

Current reading:

- Since the city creates no coins, the target income comes from dungeons, PvP and the coins of the weekly gifts
  (40,000 = 80 dungeons of 500 per week); the simulator splits how much comes from each. Selling material to the city
  only brings coins that other players spent on the list.
- With dungeons at 1,000 coins, half of 10 h in dungeons and gifts of ~2,000, a regular player earns ~40,000 per week.
- Initial treasury of 150,000 sustains an active governor. Attacks with ±30% variance knock stations down even with
  just-enough defense: a governor needs ~30% margin.
- Coins created ~40,000 per player per week, destroyed ~800. Refine GS consumes ingots, not coins. Large coin sinks
  outside the city are missing (higher crafting cost, gear repair, dungeon entry, etc.).
- With 200 players, an active city reaches level 3 on every station in a few weeks and the treasury passes 1 million
  with nothing to spend on: a full region needs more goals (more levels, buffs or defense that grow with population).

Simulation runs: [notes/systems.md](../notes/systems.md).

## Open questions

- Station mesh swap per tier on the map.
- "Economy" buff (less material) has no effect yet.
- Calibrate the numbers with use (proposal values above).
- Coin sinks to offset inflation.
- Goals for full regions.
- PvP as a coin source.
- Dungeon reward: 500 in the dungeon request, 1,000 in the second simulation round ([dungeon.md](dungeon.md)).
