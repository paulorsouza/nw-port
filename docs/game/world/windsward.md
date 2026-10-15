# Windsward

Windsward is the first region of the port. The whole territory comes first, with NPCs, ores and the rest, before
any other region. Map: `L_SouthWindsward`. Everfall city is a separate map: [everfall.md](everfall.md).
Server phase: F4 in [server.md](../../tech/server.md). History: [notes/world.md](../../notes/world.md).
Tests: [tests/world.md](../../tests/world.md).

## The region in the game

Windsward is the whole territory of region `r_+04_+01` (x 8200–10200, y 2050–4090: 2 km × 2 km). The settlement is
`Settlement_SouthWindsward_Complete`, already in Unreal (`L_SouthWindsward`, with a cut of the terrain).

Layers in `extract/terrain/.../regions/r_+04_+01/capitals/`:

| Layer | Content |
|---|---|
| `06swindsward_gatherables` | 1,412 gatherables + 1,169 vegetation plots (`<PLOT>`) |
| `06sw01..09_*_ai` | 1,263 creature spawners in the 9 POIs, 180 loot chests |
| `06sw01..09_*_props`, `06swindsward_capitals`, `roaddressing`, `water` | POI props, roads, water |
| `06swindsward_persistence` | town, 5 teleporters, fort, outpost, NPCs, camp plots |

Tables extracted to `extract/data` (`--x-datasheet json`): `gatherables`, `variations_gatherables_*` (full and
depleted mesh, radius, icon), `loottables`, `variations_npcs`, `*_npcs`, `basevitals_*`, `crafting_*`.

### Coordinates and heights

- glTF from nwbt = (−x, z, y) of CryEngine; Unreal = (−x, y, z) × 100.
- The map uses the town's frame: rotated −94° around the capital `Settlement_SouthWindsward_Complete`.
- Heightmap height = value / 32.

### Gatherables

| Type | Trade skill | Count |
|---|---|---|
| Alchemy (fungi, elemental plants) | Harvesting / Mining | 454 |
| Plants (hemp, vegetables, fruit) | Harvesting | 343 |
| Ores (iron, silver, gold, stone, guano) | Mining | 312 |
| Loose stones | no tool | 124 |
| Bushes | Harvesting | 138 |
| Trees | Logging | 40 (the rest of the forest comes from the `<PLOT>` plots) |

161 distinct meshes (full + depleted). Rules and tools: [gathering.md](../gathering.md).

### Spawner creatures

| Creature | Spawners |
|---|---|
| Boar | 96 |
| Grey wolf | 72 |
| Lynx | 21 |
| Crawling skeleton | 64 |
| Skeleton with club | 115 |
| Skeleton with sword | 55 |
| Risen/Withered | 40 |
| Undead navigator | 29 |

The 541 generic spawners (`0wp_Wander`, `1wp_Stand`, ...) need the slice content to know the creature.
AI: [creatures-ai.md](../creatures-ai.md).

## Steps

Each step ends tested over the network, like phase F3 ([server.md](../../tech/server.md)). Heavy conversions
(nwbt on slices, Unreal import) run one at a time, never in parallel with a build.

### G1: gathering

- Generator `tools/make_gatherables.py` (capitals + variations + tables → `Data/World/guardaventos_gatherables.json`).
- Conversion of the 161 meshes.
- `ANWGatherable` authoritative on the server: interact with E, game time, depleted mesh, respawn, loot into the
  inventory.
- Tools (pickaxe, sickle, logging axe, skinning knife) and the game's gathering animations.
- Trade skills with level in the backend.

Details and done criteria per trade skill: [gathering.md](../gathering.md).

### G2: region terrain

- Whole heightmap (2048², height = value / 32) as a Landscape.
- Materials from the splat maps.
- Water, roads and the props of the 9 POIs.

Pieces in use:

- `tools/make_terrain.py` builds the terrain from `region.heightmap` (height = value / 32) and `roads_splat` (dirt on
  roads); `tools/ue/import_terrain.py` puts it in the map. The first cut was 768 × 768 m at 1 m under the town.
- Safety floor `ChaoSeguranca` (invisible plane from `tools/ue/fix_city.py`): `import_terrain.py` puts it 10 m below the lowest
  terrain point. If it is above a valley, the character walks in the air.
- Water: `tools/ue/import_water.py`.
- Outside the terrain cut there is only the safety floor.

### G3: creatures

Requirement: spawner content → spawn table; one set (`tools/sets/*.json`) per family, starting with boar and wolf;
the game's patrol/wander AI; loot and XP.

Pieces:

- `tools/make_spawns.py`: 1216 spawns, 114 creatures. Chain: spawner → AIVariantProvider → `javelindata_vitals`.
  Waypoints carry tasks. Health and damage use the port's scale through `vitalsleveldata`.
- Sets `boar` and `wolf` (`tools/sets/boar.json`, `tools/sets/wolf.json`).
- `ANWCreature`: patrol, chase, return home. With a tree in `Data/AI/<set>.json` it runs the game's behavior tree
  ([creatures-ai.md](../creatures-ai.md)).
- `ANWSpawnField`: creatures spawn when the player is within 60 m, despawn at 90 m, respawn in 2–5 min.
- Carcass for skinning: the game's `Boar`/`Wolf` gatherables ([gathering.md](../gathering.md)).
- `/Game/NW/Creatures/ABP_NW_Creature`: template ABP (class `NWAnimInstance`). Its AnimGraph is built by hand once
  (steps in [tests/world.md](../../tests/world.md), G3).

Still open:

- Character XP per kill: the scale of `KillEvent` is unknown. The tables do not give the character XP formula for
  kills or gathering ([ui/original.md](../ui/original.md)).
- Loot from `CreatureLootMaster` (conditions/tags).
- Next families: turkey, rabbit, skeletons, Lost, Corrupted, and others.

### G4: living town

Town NPCs with idle animation, interaction with E (the game's card), town chest, merchant, teleporters.

### G5: loot chests and vegetation

The 180 POI chests; the forest from the `<PLOT>` plots (1,169) and the trees outside the gatherables layer.

## Town conversion

The town comes from the city conversion (`extract/cities/.../settlement_southwindsward_complete.glb`). The slice
has only buildings; the street ground is the world terrain. The nwbt conversion only produces static meshes and
lights. Outside it: 25 guild flags (`RenderGuildFlagComponent`), decals, particles (fire/smoke), water, loose
region vegetation (`region.vegetation`). Without animation: windmills, water wheels, flags, NPCs (static meshes).

## Pending

Known defects from tests ([tests/world.md](../../tests/world.md)):

- Ground uses a single texture. The game blends dozens of layers per `*_splat` (rough, soil, cliffs...).
- Town tree canopies show colored plates.
- The waterfall looks wrong (screenshot pending).
- Creature footsteps have no sound.

Town polish (found in the South Windsward town):

- Twisted cloth in the sky and blue lines on the ground: flags, clotheslines and ropes that are simulated cloth in
  the game became deformed static meshes. Filter or fix in the conversion.
- The South Windsward PlayerStart was inside the square's statue; `tools/ue/fix_spawn.py` moved it 4 m. The
  packaged server still has the old map: re-cook in the next package (`tools/linux/package_server.sh`).
- Test bots without rendering (`-nullrhi`) ignore `t.MaxFPS` (~460 fps) and fill the CharacterMovement move buffer
  ("Hit limit of 96 saved moves") with simulated latency. Only bots are affected; the 10-bot measurement of phase
  F0 is pessimistic (more traffic than real players).
- Death: the body returns to the standing pose when the Death montage ends (it should hold the last frame, as in
  `hold_montages`). The date in the character list should be day/month also in English.
- Language: game screens switch at once (HUD, mastery, death, login); text already drawn by the Slate of the dev
  menu only changes when the menu is reopened.
