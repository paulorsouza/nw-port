# Mounts

Goal: mounts equal to the game: conversion, mount and rider animations, summon and dismount, movement, camera,
sound, HUD and network. Map: `L_SouthWindsward`. Tests: [tests/mounts.md](../tests/mounts.md).

## Order

1. Horse.
2. Wolf.
3. Cat (lion).
4. The other types (bear...).
5. Appearances and dyes.

## Data

`datatables/` = `extract/data/sharedassets/springboardentitites/datatables/`; `.adb` files in
`extract/models/animations/mannequin/adb/`. A file that is not extracted:
`python3 tools/heavy.py nwbt cat -g "$NW_GAME" <path>`.

| What | Where |
|---|---|
| Mounts, types, movement | `datatables/mounts/javelindata_mounts`, `_mounttypes`, `_mount_movement` |
| Appearances and dyes | `datatables/javelindata_itemdefinitions_mount{itemappearances,dye}`, `..._weaponappearances_mountattachments` |
| Riding level | `javelindata_tradeskillriding` |
| Mount animations | `animations/mannequin/adb/mounts/mount_<type>_anims.adb` (horse, wolf, cat, bear, grunt, turkey) |
| Rider animations | `animations/mannequin/adb/player/player_mount.adb` (`Mount*`, `MountDisMount_<type>`, `MountFall*`...); mount tags in the player `.adb` files |
| Mesh and materials | columns `Mesh`/`Material` of `javelindata_mounts`; the mount slices |
| Logic (summon, dismount, gallop) | mount CAGE actionlists in `extract/serverpak/client/.../cage/`; the player slice |
| Screens | icons from the table's `IconPath`; the game's mount HUD ([ui/original.md](ui/original.md)) |

## Parts

| Part | Done when |
|---|---|
| Conversion | mesh, skeleton and animations of the mount (set `tools/sets/mount_<type>.json`) |
| Rider | animations of the mounted player (mount tags in the player `.adb` files) |
| Summon and dismount | summon time, interruption by damage, dismount when hit or when attacking, cooldown |
| Movement | speeds, acceleration, turning, jump, gallop and its resource (`mount_movement`) |
| Camera and sound | mounted camera; footsteps, voice, summon |
| HUD | icon, cooldown, gallop resource |
| Network | another player sees the same mount and rider |

## Rules known from reverse engineering

Source: reverse engineering (ask the `nw-re` agent). Evidence grades were not recorded for these rules.

- Speed depends only on the riding level, in 5 bands (0, 50, 100, 150, 200+, `javelindata_tradeskillriding`), and on
  the surface. The per-species rows are not used.
- Gallop multiplies speed by 1.12.
- Gallop needs mount stamina ≥ 15, spends 20/s and recovers 10/s.
- Mounted turning: the rate is interpolated by speed (horse 0.11 → 0.09 between 0 and 15 m/s), with the
  exponential formula of the player's turn (`1 − (1 − r)^(30·dt)`, [combat.md](combat.md)).

Open question: mounted acceleration.

## Porting a mount

1. Survey, recorded in this file: table data, fragments and windows, actionlist rules. Show the plan to the
   maintainer before changing code.
2. New set `tools/sets/mount_<type>.json` (models: `tools/sets/wolf.json` and `boar.json`). Rider montages with
   prefix `AM_Mt_` in `tools/sets/player/mount.json` (part of the player set). Conversion through the heavy-work
   queue (`python3 tools/heavy.py python3 tools/build_set.py ...`).
3. C++: `UNWMountComponent` already exists on the character (`Mount`, empty). The mount itself is `ANWMount`
   (new). Mounted locomotion through `SetLocomotionOverride`, keys through `NW_REGISTER_INPUT`, test buttons through
   `NW_REGISTER_DEV`. Do not change `ANWCharacter` or `ABP_NW`.
4. Movement with the numbers from `mount_movement`; gallop and its resource; summon and dismount with the times and
   rules from the actionlists. No source: ask reverse engineering.
5. Sound (`tools/extract_audio.py`, a new group for the mount), camera, HUD, network (replication of the mounted
   state).
6. Build and import with the editor closed, open `L_SouthWindsward`, ask the maintainer for a screenshot.

## Survey

No survey recorded yet. Every part in the table above is open.
