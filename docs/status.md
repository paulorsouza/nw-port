# Project status

Starting point for anyone new to the project. Open work is in [backlog.md](backlog.md); history is in
[notes/project.md](notes/project.md).

## What the project is

A port of New World to Unreal Engine 5.8.3, so a clan can play on its own server. The game's assets come from each
person's own install; nothing from the game is in the repository. The main client is Windows, the server is Linux,
development happens on Linux. Languages: pt-BR and en-US only.

Current scope: the whole Windsward territory (region `r_+04_+01`) and the Everfall capital (the town only, for the
city systems). Work runs in parallel areas (weapons, gathering, mounts, systems, lighting, UI), described in
[workflow.md](workflow.md). The weapons area has two sub-areas: Ice Gauntlet and Greatsword.

## What works

| Area | Where |
|---|---|
| Combat for Sword and Shield, Hatchet, Bow and Rapier: light, charged heavy, block, dodge, aim and arrows, abilities Q/R/F, Weapon Mastery. Game rules: critical and backstab, reaction by PowerLevel, stagger, knockback with falloff, camera shake, stamina, status effects with ticks | `NWCombatComponent`, `NWCharacter`, `NWProjectile`; [game/combat.md](game/combat.md), [game/weapons/README.md](game/weapons/README.md), [tech/weapon-porting.md](tech/weapon-porting.md) (how to port another weapon) |
| Ice Gauntlet v1 (sub-area 1a): mana, light and heavy, 6 abilities, the game's effects in Niagara. Known bugs in the weapon spec | [game/weapons/ice-gauntlet.md](game/weapons/ice-gauntlet.md) |
| Creature AI from the game's behavior trees (wolf, boar), with sight, group aggro and navigation from the slice | `NWBehaviorTreeComponent`, `tools/make_ai.py`, [game/creatures-ai.md](game/creatures-ai.md) |
| Port AI for humans (duelist, archer) and the test arena `L_Arena` with game pieces and waves (F10 > Arena) | `NWBrainComponent`, `tools/ue/make_test_map.py` |
| Lost (enemy) with simple AI and loot | `NWLost`, `ANWGameMode::GrantLoot` |
| Dedicated server, Rust backend (invite-only accounts, characters, saved inventory), Docker | `backend/`, `deploy/`, [tech/server.md](tech/server.md) |
| Login screen, chat, group, death and respawn, game HUD, mastery screen | `NWLoginMenu`, `NWChat`, `NWHUD`, `NWMasteryMenu` |
| Inventory with the game's items, rarity, weight, equipping weapons and tools | `NWInventoryComponent`, `NWInventoryMenu` |
| Gathering: 100k gatherables on the landscape (ore, trees, plants, alchemy, stones), tools, loot from the game's tables, respawn | `NWGatherField`, `NWGatherComponent`, `tools/make_gatherables.py`, [game/gathering.md](game/gathering.md) |
| Windsward town, region terrain (2 km), pre-generated scenery (cliffs, ruins) | `tools/ue/import_*.py`, [game/world/windsward.md](game/world/windsward.md) |
| Everfall capital in `L_QuedaEterna` (town, 768 m terrain, start point) with 30 NPCs dressed and in the game's idle pose; weekly rewards and starting items per trade skill; refining stations with the game's screen (recipes in game order, chance of extra item, XP with the ingredient factor and the first craft, count); Storage shared between towns in nw-core (1000 kg per known town, 5 named tabs, market purchases capped by capacity); Merchant shop (T5 potion and reagents with weekly limit); coins in cents; level from tutorials; crafting weapon, shield and tool with the game's result screen; gems and runeglass; Gear Score refining (51%, small steps more likely) with the material chosen on screen; crafted armor and jewelry (no visuals); 500-slot Inventory with a loot bag on the ground and its screen; network test through Cloudflare Tunnel. Trading Post and housing backends ready, no screen yet | `NWNpc`, `backend/`, `tools/etapas/sistemas.*`; [game/city.md](game/city.md), [game/world/everfall.md](game/world/everfall.md), [tech/network-test.md](tech/network-test.md) |
| Windsward music and ambience | `NWAmbienceComponent` |
| Game screens: Esc menu (with Unstuck), settings (video, audio, game, keybindings), navigation bar, map (M) with POIs, compass with POIs, item tooltip with comparison, loot ticker, warning banner, hit indicator, encumbrance, XP bar, level-up banner, Attributes, Trade skills tab in the K window | `NWGameMenu`, `NWOptions`, `NWWorldMap`, `NWHUD`, `NWInventoryMenu`, `NWTradeskillMenu`; [game/ui/original.md](game/ui/original.md) |
| Level, XP and Attributes from the game's tables (character XP has no source yet; "Gain 1 level" in F10); gathering trade skills with XP, level, speed, loot bonus and minimum level of gatherables; all saved in nw-core | `NWProgressionComponent`, `tools/make_progression.py`, migration 0003 |

Server phases F0–F3 are done. F4 (Windsward complete) is in progress: [game/world/windsward.md](game/world/windsward.md).

Tests in the editor use the local nw-core with a fixed account: [tech/backend-testing.md](tech/backend-testing.md).

## Known problems

The full list is in [backlog.md](backlog.md). The main ones:

- Lighting: only sun, sky and fog. None of the game's lights (candles, fireplaces, lamp posts); interiors are dark.
  Lighting area, [game/lighting.md](game/lighting.md).
- Ground with a single texture. The game's per-biome material is missing (`region.tractmap.tif` plus terrain
  materials).
- Character "flying" at some spots and a floating boulder: no diagnosis yet. One cause was fixed: the safety floor
  sat above the valleys (`tools/ue/import_terrain.py` now puts it below the lowest terrain).
- Town tree canopies show colored planes (masked foliage).
- Creatures at the POIs (boar, wolf, skeletons...), town NPCs, chests: steps G3–G5 of
  [game/world/windsward.md](game/world/windsward.md).
- New screen textures (tooltip, map, XP bar, banner, loot ticker) are not imported into Unreal yet. With the editor
  closed: `tools/linux/ue_import_all.sh 09`. Until then those screens show without images.
- Screens built without the editor open compile, but nobody has seen them on screen yet.
- Waterfall looks wrong; boar and wolf only partly fixed.
- Editor + Play + Lumen exceed 8 GB of VRAM. After a crash from VRAM, the Nvidia 615 driver can lock the GPU
  (GSP `NV_ERR_STATE_IN_USE`) until a reboot.

## Pitfalls (each one cost hours)

### Memory and machine

- Ubuntu kills programs through `systemd-oomd` when memory *pressure* stays above 50% for 20 s, even with RAM shown
  as free in htop (disk cache counts). Cause: several heavy jobs together (build + import + nwbt + the original New
  World). Rule: one heavy job at a time ([workflow.md](workflow.md), heavy-job queue). Do not use
  `systemd-run -p MemoryMax` for heavy jobs: the cap makes the job itself create pressure and oomd kills another
  program. (Ghidra is the exception, see [tech/reverse-engineering.md](tech/reverse-engineering.md).)
- `nwbt` loads ~6 GB of pak index on every call. Mesh conversions run in batches of 8 with `-w 2`.
- Importing a very large glb into Unreal runs out of memory. The region's prop scenery in a single file (2.4 GB,
  150k pieces) took the session down. Export and import one capitals layer at a time.
- A memory watcher running inside the Claude app dies with the app when oomd kills it; the heavy job keeps running
  without a watcher.
- 8 GB of VRAM: the original New World and the Unreal client do not fit together. Do not take high-resolution
  screenshots through a remote session with the editor in Play.
- `/tmp` is RAM (tmpfs): never put the nwbt cache there (`.nwbt/` stays at the project root).
- `pgrep -f` / `pkill -f` match the shell's own command line.
- Ghidra's full auto-analysis of the executable (180 MB) used up the RAM (38 GB) and froze the machine. Always run
  Ghidra with a cap ([tech/reverse-engineering.md](tech/reverse-engineering.md)).

### Unreal

- glTF through Interchange: use the project's glTF pipelines (DefaultGLTFAssetsPipeline + DefaultGLTFPipeline),
  which make instances of `M_Default` with the textures. The generic pipeline alone leaves materials without
  textures.
- One folder per imported mesh: materials of different meshes share names ("stone").
- Materials need "Used with Nanite" and "Used with Instanced Static Meshes"; otherwise the game outside the editor
  uses the default grey material. The Interchange parents live in the engine plugin: `fix_nanite_materials.py`
  copies them to `/Game/NW/Materials/Interchange` and repoints (the installed engine is not changed).
- Nanite draws only opaque and masked: a mesh with a translucent material has no Nanite.
- Materials made by script must compile in SM6: no color texture into normal; Single Layer Water needs its output
  node.
- The server build is a unity build: `static` symbols with the same name in two .cpp files collide.
- Unreal 5.8 JSON: `FJsonObject::Values` keys are `TSharedString` (use `*Key`).
- Python commandlets return an error for glTF material warnings. Check for "Python script executed successfully"
  in the log.
- Unreal Python: positional `unreal.Rotator(a, b, c)` is (roll, pitch, yaw). Use named arguments.
- UI layouts, fonts and data live in `unreal/NWPort/Data/UI`, outside `Content`, which the editor watches and would
  try to import.
- The font importer opens a window and takes the commandlet down; Nimbus fonts are read from the `.otf` in
  `Content/NW/UI/Fonts`.
- Automating "run the game and take a screenshot" took hours without showing what matters. To check something
  visual, open the editor and look, or ask whoever has the editor open for a screenshot.

### Game data

- Coordinates: nwbt glTF = (−x, z, y) of CryEngine; UE = (−x, y, z) × 100. The map uses the town's frame (−94°
  rotation around the capital `Settlement_SouthWindsward_Complete`).
- Heightmap height = value / 32.
- Loot: roll 0..MaxRoll. An AND table gives every item with threshold ≤ roll; OR keeps the highest threshold
  reached.
- Green and red blocks in converted scenery are the game's NoDraw materials (`NoCollide`, `COL`, `Obstruct`).
  nwbt keeps them at opacity 0.1 and UE draws them opaque. `tools/glb_strip_nodraw.py` removes those primitives.
- nw-core migrations (for example 0003, column `progression`) need the new nw-core image in the test compose:
  `deploy/build_images.sh`.

## Repository map

| Folder | Content |
|---|---|
| `unreal/NWPort/Source` | game C++ (client and server) |
| `unreal/NWPort/Config` | configuration (Nanite, Lumen, VRAM, cook) |
| `tools/` | extraction and conversion. One Python pipeline for Linux and Windows in `tools/*.py`; system scripts in `tools/linux/` (`.sh`, nwbt and ooz builds) and `tools/windows/` (`.ps1`, `doctor.py`); `tools/ue/` runs inside Unreal. nwbt from a single patch (`patches/nwbt.patch`, version stamped in the binary, checked by `check.sh` / `doctor.py`). See [tech/tools.md](tech/tools.md) |
| `backend/` | nw-core (Rust, Postgres): accounts, characters, inventory |
| `deploy/` | Docker for the server and the backend |
| `docs/` | specs, setup, status, backlog, notes |
| `inspiracao/` (images not in git) | screenshots of the original game for the screens; index in `inspiracao/README.md` |
| `patches/` | `nwbt.patch`, applied on the nw-buddy fork (branch `nw-port`) by both builds; [patches/README.md](../patches/README.md) explains how to change it |
| `extract/` (not in git) | everything taken out of the game |
| `unreal/NWPort/Content`, `Data` (not in git) | generated by the scripts |
