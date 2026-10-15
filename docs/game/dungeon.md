# Dungeon (proposal)

A simple dungeon requested by the maintainer. Not started; it belongs to whichever area picks it up. This page holds
the request and what is already known. Economy context: [economy.md](economy.md).

## Requirements

- One dungeon at first, very simple, solo or group.
- It needs an entry item. The entry is crafted from materials that drop from enemies; it can be sold (Trading Post) or
  used.
- Chests inside the dungeon; one chest at the end.
- 500 fixed coins per completed dungeon. It is one of the port's sources of new coins (with PvP and the NPC weekly
  gifts, [economy.md](economy.md#rules)).
- Average duration of ~5 minutes for strong players.

## Links to existing systems

- The entry goes to the Trading Post: add it to the list of tradeable items (`mercado` in `tools/itens.json`,
  `FNWItemRules::CanTrade`; [city.md](city.md#trading-post)).
- Dungeon coins and items as recorded operations (`NWOps`), with a reason in `item_log` (example: `dungeon:<id>`).
- Entry material: a new item in the catalog and in the creatures' loot table.
- In the game, expeditions used an entry (tuning orb) made from enemy material: same idea, simpler.

## Open questions

- Map: reuse a converted expedition section or build a simple one.
- Enemies and boss.
- Chest contents.
- Entry recipe.
- Daily limit, if any.
- Maximum group size.
- Reward per run: the request says 500 coins; the second economy simulation round used 1,000 to reach the income
  target ([economy.md](economy.md#balancing)).
