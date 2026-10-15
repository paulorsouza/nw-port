# Everfall

The capital of Everfall in its own map, `L_QuedaEterna`, with the terrain cut around it. City systems that run here:
[../city.md](../city.md). Stations and crafting: [../crafting.md](../crafting.md). City ownership:
[../economy.md](../economy.md). Tests: [../../tests/systems.md](../../tests/systems.md). Windsward stays as it is
([windsward.md](windsward.md)).

## Scope

- Only the capital of Everfall (slice `settlement_northwindsward_complete`, capital `ef_persistence` in `r_+04_+02`)
  and the terrain cut around it, in its own map. Other cities and the rest of the Everfall territory only when the
  maintainer asks.
- Systems for this map, in order: convert the city and build the playable map; city NPCs (slice position, appearance,
  idle animation, name); refining and crafting stations (interaction, recipes from the tables, time, XP, minimum
  level); city storage; shops and merchants; Trading Post (buy and sell orders in nw-core); housing (plots, purchase,
  interior, furniture, house storage); level and XP with sources; the screens each system lacks.

## Map

Game tables live in `extract/data/sharedassets/springboardentitites/datatables/` (written `DT/` below).

| Item | Value |
|---|---|
| Slice | `slices/structures/settlements/v2_settlements/settlement_northwindsward_complete.dynamicslice` (there is no `settlement_everfall`) |
| Capital | `ef_persistence`, region `r_+04_+02`, position (8931, 4224, 135) |
| Terrain | regions 4_1 and 4_2, a 768 m square (`make_terrain.py`) |
| Conversion | `tools/linux/build_all.sh sistemas-01` (`convert_city.sh settlement_northwindsward_complete`) → `extract/cities/limpa/settlement_northwindsward_complete.glb`, `extract/cities/northwindsward_terrain.glb` |
| Slices as JSON | `tools/linux/build_all.sh sistemas-02` → `extract/slicejson/slices/...` (city and the plots `housing/plots/northwindsward/`) |
| Import | `tools/linux/ue_import_all.sh sistemas-01` (city, `L_QuedaEterna`) and `sistemas-02` (terrain, spawn, materials); `tools/steps/systems.ue.sh` |
| Exposure | volume `ExposicaoDoJogo` in the map (`tools/ue/city_exposure.py`, stage `sistemas-04`): histogram from 0.1 to 1, from the 1x–10x range of the game's day cycle (`libs/timeofday/zones/everfall_a.xml`); the project disables auto exposure |
| Spawn point | with the editor open on the map (outside Play): `python3 tools/ue/remote.py tools/ue/spawn_trace.py`. Do not use `fix_spawn.py`: it put the character inside the square's floor and the character fell through the city and the terrain |

Conversion follows the city conversion procedure (skill `nw-extract`), then the `tools/ue/` scripts of import
stages 04 and 05 pointed at the new map. Other stages: `sistemas-03` (NPC appearances and animations), `sistemas-05` (UI sounds). Full steps:
[../../setup.md](../../setup.md). Asset types: [../../tech/asset-pipeline.md](../../tech/asset-pipeline.md).

Territory 4 = `@Everfall` (`DT/javelindata_territorydefinitions`): level 1–25, TerritoryStandingXpModifier 1.25,
PoiDiscoveryXpModifier 75. Standing track: `DT/javelindata_categoricalprogression` id 4 = Everfall (MaxLevel 300).

## What is in the city

| Place | In the port | Spec |
|---|---|---|
| NPCs | 30 NPCs within 300 m of the capital (game: 41 `04xx` variants and 11 `EF_Ambient_*`) | [../city.md](../city.md#npcs) |
| Refining stations | Loom, Smelter, Woodshop, Tannery, Stonecutting Table, all T5 | [../crafting.md](../crafting.md#refining) |
| Crafting stations | Forge, Workshop, Arcane Repository, Outfitting station | [../crafting.md](../crafting.md#crafting-gear) |
| Storage sheds | 2 (`settlement_storage_geo_master`) | [../city.md](../city.md#storage) |
| Merchant | `EF_Ambient_Merchant_00` | [../city.md](../city.md#shops-and-merchants) |
| Trading Post | station `tradingpost` (`Settlement_Trading_Post_Master`) | [../city.md](../city.md#trading-post) |
| Town Project Board, Territory Planning Board | `cityboard`, `governance` | [../economy.md](../economy.md#screens) |
| Housing plots | 17 addresses in `slices/housing/plots/northwindsward/` | [../city.md](../city.md#housing) |
| Training dummies | none (the list for the city is empty) | [../city.md](../city.md#character-level-and-xp) |

- Entering the city area marks it as a known city (+1000 kg storage).
- Interiors (church, tavern): the exposure adapts in 1 to 2 s when entering and leaving.
- Everfall has no rune station: runeglass is made at the Forge and the Stonecutting Table. The Kitchen is not used
  yet ([../crafting.md](../crafting.md#stations)).

## Open questions

- A place in the city for the training dummies.
- Housing on the map (houses are open, no system yet).
- Station meshes per tier (`station_t2..t5`), [../economy.md](../economy.md#stations).
