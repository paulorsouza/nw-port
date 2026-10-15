# Notes: world

History of the world areas: Windsward region, gathering, mounts, creatures and creature AI, lighting. Specs:
[windsward.md](../game/world/windsward.md), [gathering.md](../game/gathering.md), [mounts.md](../game/mounts.md),
[creatures-ai.md](../game/creatures-ai.md), [lighting.md](../game/lighting.md).

## 2026-10-11

### Windsward only (phase F4)

The maintainer asked for Windsward only, with NPCs, ores and the rest, before any other region. The conversion
queue of the other cities was canceled. Edengrove, already converted, stays stored without import.

### Town polish findings

Found in the first sessions in the South Windsward town. Open items are copied to
[windsward.md](../game/world/windsward.md), "Pending".

- Twisted cloth in the sky and blue lines on the ground (simulated cloth that became deformed static meshes).
- PlayerStart inside the square's statue; `tools/ue/fix_spawn.py` moved it 4 m. The packaged server still had the
  old map (re-cook with `tools/linux/package_server.sh`).
- Bots with `-nullrhi` ignore `t.MaxFPS` (~460 fps) and hit "Hit limit of 96 saved moves" with simulated latency;
  the 10-bot measurement of phase F0 came out pessimistic.
- Death pose returns to standing when the Death montage ends; date format in the character list.
- Language switching: Slate text of the dev menu only changes on reopen.

### Gathering polish (F4/G1)

State on this date:

- The gathering card already used the game's pieces (`markers/unifiedinteract`); the multi-option card
  (`hud/unifiedinteractcard/interactscreen`) was missing.
- Trade skills (Mining, Harvesting, Logging, Skinning) had no level and no XP. Loot used the roll without the trade
  skill bonus (`RollBonusSetting` = AddToRoll ignored). Tables with `Conditions` (tool tonics, season, PvP) and
  `[LBID]` buckets were left out.
- No depletion VFX/SFX (`DepletedVFX`/`DepletedSFX`) and no compass icon (`CompassIcon`).
- Vegetation from the 1,169 `<PLOT>` plots and trees outside the gatherables layer: deferred to G5.

## 2026-10-12

### Trade skills

`RequiredTradeskillLevel` started to apply to server characters (nw-core progression). The level's `RollBonus` was
added to the loot roll (AddToRoll adds, ClampMax caps). This closed the "no level, no XP, bonus ignored" item of
2026-10-11.

### G3 creatures in progress

State on this date:

- `tools/make_spawns.py`: 1216 spawns, 114 creatures (spawner → AIVariantProvider → `javelindata_vitals`;
  waypoints with tasks; health/damage on the port's scale through `vitalsleveldata`).
- Sets `boar` and `wolf` imported.
- `ANWCreature` (patrol, chase, return home), `ANWSpawnField` (spawns at 60 m, despawns at 90 m, respawns in
  2–5 min), carcass for skinning (the game's `Boar`/`Wolf` gatherables).
- Missing: the `ABP_NW_Creature` graph (manual), testing, character XP per kill (`KillEvent` scale unknown), loot
  from `CreatureLootMaster` (conditions/tags), next families (turkey, rabbit, skeletons, Lost, Corrupted...).

### G2 test round

Context for [tests/world.md](../tests/world.md), sections G2, new screens and G3:

- Already imported before the round: screen textures (import step 09) and boar/wolf (import step 12).
- Item 1 (character flying) resolved: the cause was `ChaoSeguranca`, the invisible plane from `fix_city`, above
  the valleys. It now sits 10 m below the lowest terrain.
- Item 4 (water): before the round, the loose waterfall curtain plates in the air, ~200 m east of the square, were
  removed, and the upper-level sections that covered the lower level were lowered.

### Round after compiling, and the combat round

Items 21–24 were added after a compile on the same day. Items 25–39 (arena, game rules, bow, rapier) were added the
same day; items 40–44 (fixes after the first test) and 45–48 (second bow fix) were added that evening.

### Creature AI from the game's trees (evening)

Wolf and boar switched to the game's behavior tree ([creatures-ai.md](../game/creatures-ai.md)). Test items 49–54.

## 2026-10-13

### Lighting state

State of the port on this date:

- Per map: sun (`DirectionalLight`, 8 lux, fixed), real-time sky light, atmosphere and fog
  (`tools/ue/import_city.py`). Lumen (GI and reflections), virtual shadow maps, Nanite, distance fields
  (`unreal/NWPort/Config/DefaultEngine.ini`).
- Auto exposure off in the project. `L_QuedaEterna` got the volume `ExposicaoDoJogo` (`tools/ue/city_exposure.py`,
  step `sistemas-04`): only dark areas brighten up to 10x (range 0.01–0.1, bias −2.32); the street stayed as
  before.
- No light from the game in the maps.
- A maintainer screenshot in Everfall showed the Lumen surface cache atlas warning (3–10% oversubscribed,
  `r.LumenScene.SurfaceCache.AtlasSize`). The city has ~5,000 mesh components.
- The first lighting survey was written (day cycle of Everfall, 744 city lights with all parameters, questions for
  reverse engineering).
- The nwbt `sliceLight` change was not committed in `vendor/nw-buddy` yet and `patches/nwbt-nwport.patch` needed an
  update. By 2026-10-15 the repository carries it in `patches/nwbt.patch`.

### Undone lighting attempt (systems area)

Do not repeat. What was done:

- The 744 city lights as `PointLight`/`SpotLight` (area lights became spots), falloff without inverse square,
  intensity = `DiffuseMultiplier` x a single scale, draw distance = range x a factor.

Result in the maintainer's screenshot: places too bright, lights turn on when the player gets close (pop by draw
distance), church and tavern still wrong. With a large draw distance the editor froze (GPU 95%, 6.5 GB VRAM).

Causes:

- A single scale with no source (the game does not use a physical unit for local lights).
- The CryEngine falloff differs from Unreal's.
- Invented draw distance.
- Area lights approximated by spots.
- None of the game's interior rules (`AffectsThisAreaOnly`, vis areas), no equivalent GI.

It was removed from the map and from the pipeline (step `sistemas-05`). The data stayed:
`extract/cities/luzes/settlement_northwindsward_complete.lights.json`.
