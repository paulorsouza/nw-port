# Project notes

Dated history of the project as a whole: state snapshots, decisions, discarded paths, rounds of work. Current state
is in [../status.md](../status.md); open work in [../backlog.md](../backlog.md).

## Decisions in effect (dates not recorded)

- Unreal is the engine. Lumberyard/O3DE was considered and discarded (the Lumberyard code stays only as a reference
  for how the game's data is read: [../tech/asset-pipeline.md](../tech/asset-pipeline.md)).
- The legal topic is closed.

## Early Unreal project (first combat build)

The first C++ project had Sword and Shield combat only (plan then in the combat spec,
[../game/combat.md](../game/combat.md)). Classes at that time:

| Class | Role |
|---|---|
| `ANWCharacter` | over-the-shoulder camera, strafe/sprint, input (created in code), sword and shield on sockets; without a controller it becomes a training dummy |
| `UNWCombatComponent` | health, stamina, actions, blade sweep, block, dodge, reactions, death; the server decides, montages by multicast |
| `UNWAnimInstance` | base of `ABP_NW` (Speed, Direction, TurnRate, bSprinting, bBlocking) |
| `ANWLost` | sword Lost (set `lost_sword`): chases at 15 m and alternates 4 of the game's attacks |
| `ANWHUD` | health/stamina bars and the health of the target ahead |
| `UNWAnimNotify_RandomSound` | sound with random variation, written into the animations by `import_nw.py` |
| `FNWDamageRow` / `FNWActionRow` | DataTable rows (`tools/make_datatables.py` writes the CSV) |

First-time order then: `make_datatables.py`, `make_sound_map.py`, open the project, `import_nw.py`, create `ABP_NW`
by hand in `/Game/NW/sword_shield/` (parent `NWAnimInstance`, blend spaces + slot DefaultSlot + block loop), repeat for
`tools/sets/lost_sword.json` and create `ABP_Lost` in `/Game/NW/lost_sword/` (BS_Walk + slot), then
`make_test_map.py` (`L_Arena` with floor, sky, 3 dummies: standing, with shield, facing away; Brightwood music).

Controls then: WASD, mouse, Shift sprints, left button attacks (hold = heavy), right blocks, Space dodges, Q/R/F
abilities. (Changed on 2026-10-11, see below.)

## 2026-10-11: first session in Unreal

Pending list written after the first session in Unreal. Open items moved to the backlog; what was done or found:

### Town

- South Windsward converted (`extract/cities/.../settlement_southwindsward_complete.glb`). The slice has only
  buildings; the street ground is the world terrain.
- Terrain: `tools/make_terrain.py` made 768×768 m at 1 m under the town, from `region.heightmap` (height =
  value/32) and `roads_splat` (dirt on the roads); `tools/ue/import_terrain.py` placed it in the map. Start at the
  central crossing. Outside the 768 m there was no terrain (only the safety floor). The region terrain later grew
  to 2 km (extraction step 08).
- The character fell because the start point sat over a plaza without terrain. Building collision checked: BlockAll +
  complex-as-simple on 1130 meshes.
- Green/red blocks were the game's NoDraw materials; `tools/glb_strip_nodraw.py` removes those primitives (clean
  glb in `extract/cities/limpa/`, same name as the original so UE replaces the assets).

### Jump and traversal

- `UNWTraversalComponent`: Space jumps; with an obstacle ahead it does step-up, vault or mantle, within the ranges
  of the game's `TraversalComponent` (`slices/player.dynamicslice`). Root motion adjusted to the real height and
  distance. Space held in the air grabs a ledge (mantle).
- Game keys (`libs/config/defaultprofile.xml`): Space jumps, Shift dodges. Sprint moved to left Alt (the game has
  no equivalent key with a weapon in hand).

### Heavy attack

- Game flow (`player_sword.adb`): press = `Attack_Start_Charge`; release < 0.333 s = light (continues the combo);
  0.333–0.667 s = `Attack_Heavy` (1H_Sword_Attack2, 1.2×); at 0.667 s `Attack_Heavy_Charged` fires by itself
  (1H_Sword_Attack2_Charged, 1.6×). Implemented in `UNWCombatComponent` (actions HeavyCharge/Heavy/HeavyCharged)
  with a charge bar on the HUD.
- The heavy's stamina cost came out 0 (CostID `Attack_1H_Sword_Heavy` in the table; the datasheet CostID did not
  match the table's). Later the same day: the game's attacks spend no stamina
  (`javelindata_staminacosts_player` = 0 for attacks); only dodge and block do.

### HUD (H1)

- Game screens converted: `tools/ui_canvas.py` (LyShine `.dynamicuicanvas` → layout) and `tools/ui_build_hud.py`
  (vitals2024, weaponquickslots, consumablequickslots, reticles → `Content/NW/UI/hud_layout.json`); textures by
  `tools/ue/import_ui.py`; Nimbus fonts read from the `.otf` in `Content/NW/UI/Fonts`.
- `FNWUILayout` draws with LyShine rules (anchors/offsets, 9-slice, linear/radial fill, text). `ANWHUD` sets the
  values the Lua used to set.
- Ability cooldowns from the game (`javelindata_cooldowns_player`). Icon on a background by category
  (AbilitiesCommon: attack = bg2 red, defense = bg3 blue).
- `-nwshot=file.png` in the game: uses Q and R, takes a screenshot at 6 s and quits (automatic check).
- Still to do then: H2 (menus) and H3 (Hatchet + swap 1/2; H3 was done the same day, below).

### Menus

- K = ability menu (`SNWSkillMenu`, Slate): 6 Sword actives with pt-BR names/descriptions from the game
  (`tools/make_abilities.py` → `Data/UI/abilities.json`); choice saved in `GameUserSettings.ini` `[NW.Loadout]`.
  Removed later the same day when K became the game's mastery screen.
- F10 = dev menu (`SNWDevMenu`): no cooldown, immortal, infinite stamina, heal, spawn Lost/dummies, kill/remove,
  slow motion, travel arena/town, debug.
- New effects: Shield Bash stuns 2 s (loop `AM_StunLoop`); Defiant Stance −30% damage for 8 s (montage
  `shield_spc_attack_3_start`); Leaping Strike uses the run attack clip; Reverse Stab now uses
  1H_Sword_Attack_Stab (180%).
- UI layout, fonts and data moved to `unreal/NWPort/Data/UI` (outside `Content`, which the editor watches and would
  try to import).

### Hatchet

- Second weapon in the same set (`sword_shield`): mesh `wep_axe1h_throwingaxea_t5`, blend spaces `BS_Hatchet_*`
  (one-hand base + sword arm, no shield), montages `AM_Hx_*` (windows from `player_axe.adb`, tags 1H_Melee+Axe),
  table `DT_HatchetActions`.
- Swap on 1/2 (`ANWCharacter::WeaponSets`, `ActiveWeapon` replicated): plays the draw; inactive weapon on the hip
  (`Stowed` on Hips), shield on the back (Spine2, tilted 29° to make up for the posture). HUD shows the active
  weapon below and the other above (the game's inactive background). Ability menu with tabs per weapon; loadout
  saved per weapon.
- Abilities: Berserk (+20% damage 5 s), Feral Rush, Raging Torrent, Rending Throw / Social Distancing / Infected
  Throw (projectile `ANWProjectile`). Specials 4/5 = Infected/Berserk by deduction.
- `ABP_NW` needed the Locomotion pin so locomotion switches with the weapon.
- Hatchet's own locomotion deferred (additive layers; see backlog).

### Mastery screen

- K opens the original MasteryTreeWindow (`tools/ui_build_mastery.py` → `Data/UI/mastery_layout.json`), drawn by
  `FNWUILayout` (now with masks, additive blend, offset and click). Logic in `FNWMasteryMenu`: nodes by
  TreeRow/TreeColumn of the datasheet, art by type (active/passive/info), upgrade arrows, Q/R/F slots, points (19),
  ultimate with 10 points in the tree, tooltip with rich text, tabs 1/2 per weapon. Default frame style
  (FrameMultiBg) and @key texts applied at build time.
- Passives and ultimate only count points. The old Slate ability menu was removed.

### Reverse engineering setup

- Headless MCP server over the Ghidra project verified with the command in
  [../tech/reverse-engineering.md](../tech/reverse-engineering.md) (~1.7 GB RSS).

## 2026-10-12: game screens without the editor

Screens built on the PC alone (no editor, no game run): they compile, but were not checked on screen. Items to
import and check are in the backlog. Facts from that round:

- Trade skills: `RequiredTradeskillLevel` applies only to a server character (nw-core progression); in the editor's
  Play everything is gatherable. The level's `RollBonus` goes into the loot roll (AddToRoll adds, ClampMax caps at
  the maximum).
- Level, XP, Attributes and trade skills are saved in nw-core (migration 0003, column `progression`); the test
  compose needs the new nw-core image (`deploy/build_images.sh`) to apply the migration.
- unluac `unluac_2025_12_23.jar` downloaded for the Lua interface scripts.

## 2026-10-12: test with the user

- Character "flying": fixed for one cause (the safety floor sat above the valleys; `import_terrain.py` now puts it
  below the lowest terrain). The 2026-10-13 snapshot still lists "flying" at some spots and a floating boulder as
  undiagnosed.
- Script-made materials did not compile in SM6 (normal with a color texture; Single Layer Water without the output
  node): fixed in the editor and in the scripts. Player, Lost, boar, wolf and water were affected.
- Still missing then: the waterfall ("very strange") and lighting (screenshots pending); boar/wolf "improved a
  bit".
- Editor + Play + Lumen exceeded the 8 GB of VRAM; after a VRAM crash the Nvidia 615 driver can lock the GPU (GSP
  `NV_ERR_STATE_IN_USE`) until a reboot.

## 2026-10-13: state snapshot

Status doc updated. Scope: the whole Windsward territory (region `r_+04_+01`) and the Everfall capital (town only,
for the systems). Work ran in five parallel areas (weapons, gathering, mounts, systems, lighting), with weapons split
into the Ice Gauntlet and Greatsword sub-areas. Server phases F0–F3 done, F4 (Windsward complete) in progress.
Screens made on 2026-10-11/12 without the editor open compiled but had not been seen on screen.

## 2026-10-14: performance tests by sessions

The maintainer allowed sessions to run performance tests (load test) without a window (server `-server -nullrhi`,
bots `-nullrhi`), through the heavy-job queue, with the editor and the original New World closed. All other runs of
the game, client or automatic tests stay with the maintainer.

## 2026-10-15: tools reorganized

- The Linux system scripts moved from `tools/<x>.sh` to `tools/linux/`; the old `tools/<x>.sh` files are temporary
  shortcuts for sessions that were already open.
- nwbt is built from a single patch (`patches/nwbt.patch`) with the version stamped in the binary and checked by
  `check.sh` / `doctor.py`.
