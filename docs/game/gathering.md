# Gathering

Gathering trade skills: Mining, Logging, Harvesting, Skinning. Fishing comes last (it has its own system).
Goal: each one equal to the game in animation, sound, effects, HUD and rules, with no visual bugs.
Region and gatherable counts: [world/windsward.md](world/windsward.md). Tests: [tests/gathering.md](../tests/gathering.md),
map `L_SouthWindsward`. History: [notes/world.md](../notes/world.md).

## Done criteria per trade skill

| Part | Done when |
|---|---|
| Animation | start, loop and end per tool; tool in hand; weapon stowed during gathering and back afterwards |
| Rules | speed per tool and level, XP, loot bonus, minimum level, loot table conditions |
| Sound | tool hit per material, depletion, loot |
| Effects | impact per material, depletion (`DepletedVFX`), respawn |
| HUD | interaction card (including the multi-option one), progress bar, loot notice, compass and tracking |
| Visual | gatherable on the ground (not floating), correct foliage and canopies, no LOD pop |

## Rules

- Gathering is an actionlist action, like the combat actions. Start, loop and end follow the actionlist rules
  ([combat.md](combat.md)): exit by `DeactivationCondition`, `NoAutoBlendOut`, interruption by the `.grid`. Never
  by a fixed time.
- Every number comes from the game data, with its source (file and ID) in this file or in a short code comment.
  No source in the data: ask reverse engineering (ask the `nw-re` agent).
- `RequiredTradeskillLevel` applies only to a server character (nw-core progression). In the editor's Play
  everything can be gathered.
- The trade skill level's `RollBonus` enters the loot roll: `AddToRoll` adds to the roll, `ClampMax` caps at the
  maximum. In the port the bonus is +10 per level (`NWProgressionComponent`, `TradeRollBonus`).
- Loot roll: roll 0..MaxRoll. An AND table gives every item with threshold ≤ roll. An OR table keeps the highest
  threshold reached.
- Loot tables with `Conditions` (tool tonics, season, PvP) and `[LBID]` buckets are left out for now.
- Example from the data: `Vein_Iron_c` → `OreVeinSmall`, pickaxe, 15 s, loot `OreVeinFinishSmall`, respawn
  504–720 s.

## Tools per trade skill

| Trade skill | Tool | Port montage (key in `NWGatherComponent`) |
|---|---|---|
| Mining | pickaxe | `AM_GatherMine` (`Mining`) |
| Logging | logging axe | `AM_GatherChop` (`Chopping`) |
| Harvesting | sickle | `AM_GatherCut` (`Cutting`) |
| Skinning | skinning knife | `AM_GatherSkin` (`Dressing`) |
| Fishing | fishing pole (`player_fishingpole.adb`, `Fishing_*`) | none yet |

Skinning time in the port: `SkinTime / max(0.1, knife efficiency + trade skill efficiency)`, minimum 1 s
(`NWGatherComponent`).

Loose stones need no tool. Some alchemy nodes are Harvesting, others Mining.

## Skinning

Skinning needs a dead creature. Creatures and their AI belong to the weapons area; ask in the report for what is
missing. The carcass is the game's `Boar`/`Wolf` gatherable. Without a knife the card asks for the knife.
Creature AI: [creatures-ai.md](creatures-ai.md).

## Game data sources

`datatables/` = `extract/data/sharedassets/springboardentitites/datatables/`. A file that is not extracted:
`python3 tools/heavy.py nwbt cat -g "$NW_GAME" <path>`.

| What | Where |
|---|---|
| Gatherables (tool, time, loot, variations, depletion VFX/SFX) | `datatables/javelindata_gatherables*.datasheet.json` |
| Trade skills (XP per level, speed, loot bonus) | `datatables/javelindata_tradeskill{mining,logging,harvesting,skinning,fishing}` |
| Loot | `javelindata_loottables*` (roll rules above) |
| Animations | fragment `Gather` in `player_pick.adb`, `player_axe.adb`, `player_1h_melee.adb`, `player_2h_melee.adb`, `player_offhand.adb`, `player.adb`; fishing in `player_fishingpole.adb` (`Fishing_*`) |
| State logic | gathering CAGE actionlists in `extract/serverpak/client/.../cage/` |
| Screens | `hud/unifiedinteractcard/*`, loot notice, compass (`CompassIcon`); [ui/original.md](ui/original.md) |

## Port pieces

| Piece | Role |
|---|---|
| `NWGatherComponent` (C++) | interaction, montages `AM_GatherMine`/`AM_GatherCut`/`AM_GatherChop`/`AM_GatherSkin` from the player set, loot, trade skill XP, skinning |
| `NWGatherField` (C++) | instances, respawn |
| `tools/make_gatherables.py` | capitals + variations + tables → `Data/World/guardaventos_gatherables.json`; also writes `extract/ue/gatherables/meshes.txt` |
| `tools/linux/convert_gatherables.sh` | converts the gatherable meshes (skill `nw-extract`) |
| `tools/ue/import_gatherables.py` | imports the meshes into Unreal |
| `tools/sets/player/gathering.json` | the gathering part of the player set |

## Porting a trade skill

1. Survey: what the game defines (sources above) against what the port does. Record it in this file, per trade
   skill, with the list of what is missing, before changing code.
2. Animations: new montages with prefix `AM_Gather*`, tools, fragments and `.adb` windows in
   `tools/sets/player/gathering.json`. Conversion:
   `python3 tools/heavy.py python3 tools/build_set.py tools/sets/sword_shield.json`.
3. Rules and loot in `NWGatherComponent` and `make_gatherables.py`, with the source of each number.
4. Sound and effects: Wwise sounds already extracted (`extract/audio`, `tools/extract_audio.py`), sound map
   (`tools/make_sound_map.py`), VFX from the variations.
5. HUD: interaction card, progress bar, loot notice, compass.
6. Build and import with the editor closed (import step 01, `02`; gatherables `03`/`13` as needed), open
   `L_SouthWindsward`, ask the maintainer for a screenshot.

## Survey

No per-trade-skill survey recorded yet.

## Pending polish

- The gathering card uses the game's pieces (`markers/unifiedinteract`). The multi-option card
  (`hud/unifiedinteractcard/interactscreen`) for NPCs and stations is missing.
- Loot tables with `Conditions` (tool tonics, season, PvP) and `[LBID]` buckets are left out.
- No depletion VFX/SFX (`DepletedVFX`/`DepletedSFX` from the variations) and no compass icon (`CompassIcon`).
  `make_gatherables.py` already exports `compass_icon`.
- Vegetation from the `<PLOT>` plots (1,169) and trees outside the gatherables layer: step G5
  ([world/windsward.md](world/windsward.md)).
- Fishing: not started.
