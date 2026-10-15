# Workflow

How work on the port is organized. Each area can be a session on the same machine (same checkout) or a person on
their own machine, on a branch (see "Several people"). Reverse engineering of `NewWorld.exe` is not an area: any
area asks a specific question through the `nw-re` agent (see "Reverse engineering on demand").

## Areas

| Area | Spec | Tests | Test map |
|---|---|---|---|
| 1. Weapons | [game/combat.md](game/combat.md), [game/weapons/README.md](game/weapons/README.md), [tech/weapon-porting.md](tech/weapon-porting.md) | [tests/weapons.md](tests/weapons.md) | `L_Arena` |
| 1a. Ice Gauntlet | [game/weapons/ice-gauntlet.md](game/weapons/ice-gauntlet.md) | [tests/ice-gauntlet.md](tests/ice-gauntlet.md) | `L_Arena` |
| 1b. Greatsword | [game/weapons/README.md](game/weapons/README.md) | [tests/weapons.md](tests/weapons.md) | `L_Arena` |
| 2. Gathering | [game/gathering.md](game/gathering.md) | [tests/gathering.md](tests/gathering.md) | `L_SouthWindsward` |
| 3. Mounts | [game/mounts.md](game/mounts.md) | [tests/mounts.md](tests/mounts.md) | `L_SouthWindsward` |
| 4. Systems and Everfall | [game/city.md](game/city.md), [game/items.md](game/items.md), [game/crafting.md](game/crafting.md), [game/economy.md](game/economy.md), [game/world/everfall.md](game/world/everfall.md) | [tests/systems.md](tests/systems.md) | `L_QuedaEterna` |
| 5. Lighting | [game/lighting.md](game/lighting.md) | [tests/world.md](tests/world.md) | `L_QuedaEterna`, `L_SouthWindsward` |
| UI (game screens) | [game/ui/README.md](game/ui/README.md) | [tests/systems.md](tests/systems.md) | `L_QuedaEterna` |

The UI files belong to the systems area (owners table below); any area that builds or redoes a game screen follows
the UI work cycle in [game/ui/README.md](game/ui/README.md).

Every area also follows the rules below and, when a source is missing, asks the `nw-re` agent.

## Platform

The pipeline Python in `tools/*.py` is the same on Linux and Windows (`python3` on Linux, `python` on Windows).
System-dependent actions are named by what they do, for example "import step NN". Linux commands are in
`tools/linux/` (skill `nw-linux`), Windows ones in `tools/windows/` (skill `nw-windows`).

| Action | Linux | Windows |
|---|---|---|
| Heavy-job queue | `python3 tools/heavy.py <command>` | `python tools/heavy.py <command>` |
| Extraction step NN | `tools/linux/build_all.sh NN` | `python tools/build_all.py NN` |
| Import step NN (01 = compile) | `tools/linux/ue_import_all.sh NN` | `python tools/ue_import_all.py NN` |
| Regenerate `Data/` | `python3 tools/make_data.py` | `python tools/make_data.py` |
| Check before merging | `tools/linux/check.sh --cpp` | `python tools/windows/doctor.py --full` |

## Common rules

### Machine

- Heavy jobs go through the queue: `python3 tools/heavy.py <command>` (on Linux, `tools/linux/heavy.sh` uses the
  same lock). Heavy = nwbt, Blender, `build_set.py`, `convert_*.sh`, `UnrealEditor-Cmd`, compiling, Ghidra in
  batch. `build_all.sh` and `ue_import_all.sh` enter the queue by themselves. Reason: `systemd-oomd` kills programs
  when two heavy jobs add up memory pressure ([status.md](status.md), pitfalls).
- The original New World stays closed during heavy jobs.

### Editor and tests

- The editor belongs to the maintainer. Compiling and importing need the editor closed: ask for it to be closed, do
  everything that needs it closed in one go, and at the end open the editor on your area's map.
- Never run the game, the client or automatic tests on your own. Open the editor and ask for a screenshot.
- Exception: a performance test (load test, [tech/architecture.md](tech/architecture.md)) may be run by a session,
  without a window (server `-server -nullrhi`, bots `-nullrhi`), through the heavy-job queue, with the editor and
  the original New World closed.
- Every test uses the local nw-core ([tech/backend-testing.md](tech/backend-testing.md)): run
  `tools/linux/backend_dev.sh` before opening the editor. Play logs in with account `editor` / character `Exilado`
  and saves Inventory, position and levels between sessions.
- Each area has its own test list in `docs/tests/<area>.md`, numbered only within it ([tests/README.md](tests/README.md)).

### Shared code

- Other sessions edit the same checkout at the same time. Files owned by another area (table below): do not touch
  them. If you need something there, make a minimal change and describe it in your report.
- Shared file: reread it before editing, change only the needed part, do not reformat.
- The C++ must compile at the end of each of your changes. A compile error in another area's file: do not fix it;
  tell the maintainer and wait.
- Player set: one file per area (`tools/sets/player/weapons.json`, `gathering.json`, `mount.json`), merged with
  `tools/sets/sword_shield.json` (character, jump, fall, death) by `tools/setspec.py`. The same montage in two parts
  is an error. Whoever runs `build_set.py` also takes what the other areas changed.

### Data and sources

- Every number, window, animation and sound comes from the game's data, with its source (file and ID) in the area
  spec or in a short code comment. No source in the data: ask the `nw-re` agent. No answer: put it in the spec's
  "No source" section.
- A fact of `NewWorld.exe` that the data does not give: ask the `nw-re` agent, even if it was asked before (answers
  are not kept in the repository). A fact with grade C or R outweighs an older port doc.
- A value from reverse engineering goes into the code with the value itself (a constant or
  `FNWRules::Num("id", value)`). No executable address in code or docs.
- Damage, critical, grit and mitigation are server-side and not in the client; asking for them is useless.

### Screens

- A new or redone game screen follows the cycle in [game/ui/README.md](game/ui/README.md): screen spec, capture
  without the game (`tools/linux/ui_shot.sh`), comparison with the original's screenshot. The screenshot request to
  the maintainer comes last.
- `FNWUILayout` is shared: the LyShine-exact mode (`bExactText`) is turned on per screen, without changing the others.

### Delivery

- Commit and push only when the maintainer asks, and only the area's files (`git add <paths>`, never `git add -A`).
  Message prefixed with the area: `armas:`, `coleta:`, `montarias:`, `sistemas:`.
- Test credentials stay in `dev.env`; never in chat or in a doc. No official servers.

## File owners

| Area | Files |
|---|---|
| Weapons (1, 1a, 1b) | `NWCharacterWeapons.cpp`, `tools/sets/player/weapons.json`, `tools/steps/weapons.*`, `tools/strings/weapons.json`; `NWCombatComponent`, `NWProjectile`, `NWFxActor`, `NWBrainComponent`; `tools/make_datatables.py`, `make_abilities.py`, `weapon_report.py`, `tools/ue/make_test_map.py`, `make_fx.py`; `docs/game/weapons/`, `docs/game/combat.md`, `docs/tech/weapon-porting.md`. Sub-areas: new weapon code in its own `NWArma<Name>.cpp`, its set part in `tools/sets/player/armas_<weapon>.json` (listed in `parts`), the weapon's spec and tests; the rest of area 1 is shared by the three |
| Gathering | `tools/sets/player/gathering.json`, `tools/steps/gathering.*`, `tools/strings/gathering.json`; `NWGatherComponent`, `NWGatherField`; `tools/make_gatherables.py`, `convert_gatherables.sh`, `tools/ue/import_gatherables.py`; `docs/game/gathering.md` |
| Mounts | `NWMountComponent` (created, empty), `tools/sets/player/mount.json`, `tools/steps/mounts.*`, `tools/strings/mounts.json`; `NWMount*`; `tools/sets/mount_*.json`; `docs/game/mounts.md` |
| Systems (and UI) | `NWCharacterSave.cpp`, `tools/strings/systems.json`, `tools/steps/systems.*`; `NWHUD`, `NWInventory*`, `NWProgression*`, `NWItems`, `NWGameMenu`, `NWOptions`, `NWWorldMap`, `NWTradeskillMenu`, `NWAttributesMenu`; `backend/`; `tools/make_items.py`, `make_progression.py`, `ui_build_*.py`, `convert_city.sh`, `tools/ue/import_city.py`, `fix_city.py`; `NWRefiningMenu`, `NWStorageMenu`, `NWLootBag`, `NWShop*`, `tools/shops.json`, `NWCrafting`, `NWInteractable`, `NWNpc*`, `NWEvents`, `NWUIShotCommandlet`; `tools/ui_screens/`, `ui_shots/`, `ui_shot.sh`, `ui_styles.py`, `ui_compare.py`, `make_crafting.py`, `make_npcs.py`; `docs/game/ui/`, `extract/ui/survey.md`, `docs/game/city.md`, `docs/game/world/everfall.md` |
| Lighting | `tools/steps/lighting.*`, `tools/ue/city_exposure.py` (comes from area 4), light scripts in `tools/ue/`, `docs/game/lighting.md`; actors in the maps' `Iluminacao/` folder. The `sliceLight` of the nwbt patch (`vendor/nw-buddy`, `patches/nwbt-nwport.patch`) |
| Server and network (for now with area 4) | `NWLoadTest`, `NWPlayerGrid`, `tools/perf_csv.py`, `tools/item_log.py`, `docs/tech/architecture.md` (performance rules for every area; draft until the maintainer approves) |
| Reverse engineering (`nw-re` agent) | `tools/re/`, `docs/tech/reverse-engineering.md` |
| Shared | `NWCharacter.h`, `NWCharacter.cpp`, `NWPlayerController`, `SNWDevMenu`, `NWCombatTypes.h`, `NWMasteryMenu`, `NWRules`, `NWInputRegistry`, `NWDevRegistry`, `NWUILayout`, `tools/ui_canvas.py`, `NWAnimInstance`, `ABP_NW` (single owner, below); `tools/sets/sword_shield.json`, `tools/strings/general.json`, `build_all.sh`, `ue_import_all.sh`, `make_data.sh`; `docs/status.md`, `docs/backlog.md` |

## Extension points

Use these instead of editing shared files.

| I need | Use |
|---|---|
| A new key (shows in Keybindings) | `NW_REGISTER_INPUT` in your `.cpp` (`NWInputRegistry.h`) |
| A new F10 button | `NW_REGISTER_DEV` in your `.cpp` (`NWDevRegistry.h`); runs on the server |
| Change the character's locomotion (mounted, gathering) | `ANWCharacter::SetLocomotionOverride` / `ClearLocomotionOverride` |
| A new player montage | your part in `tools/sets/player/<area>.json` |
| New text | `tools/strings/<area>.json` (`texts` or `game_keys`) |
| An extraction or import step | `tools/steps/<area>.build.sh` / `.ue.sh` ([tools/steps/README.md](../tools/steps/README.md)) |
| A reverse-engineering fact | in the code, with the value (ask the `nw-re` agent) |

`ABP_NW` (the player's animation graph) is binary and does not merge: it has a single owner (the maintainer). The
rest uses montages in the `DefaultSlot` / `UpperBody` slots and the locomotion override. A change in the graph: ask
the maintainer, who makes it in a commit with only the ABP.

## Day to day

- Build from scratch: [setup.md](setup.md) (`tools/linux/build_all.sh` + `tools/linux/ue_import_all.sh`).
- Changed a table or a data script: `tools/linux/make_data.sh`. Changed C++: compile (import step 01).
- Game screen: capture without opening the game (`tools/linux/ui_shot.sh`) and compare with the original's
  screenshot; cycle in [game/ui/README.md](game/ui/README.md).
- Check something visual: open the editor and look, or ask whoever has the editor for a screenshot. Do not automate
  "run the game and take a screenshot".

## Several people

- Each person needs their own New World install at the build in `tools/game.version` (`tools/linux/game_version.sh`
  checks it; `build_all.sh` stops on another build) and the full pipeline ([setup.md](setup.md)). Extraction,
  `Content` and `Data/` do not go into git: each person generates their own.
- One branch per area from `main` (`armas`, `coleta`, `montarias`, `sistemas`). Merge into `main` at each closed
  item, at most every few days. Before that, bring `main` into the branch and run `tools/linux/check.sh --cpp`
  (it includes nwbt at the current patch and the `@list` test).
- A change in a shared file: a small separate PR, merged soon, so the others can build on it.
- Who merges into `main`: the maintainer (or whoever the maintainer names).
- Reverse engineering runs only on the machine with Ghidra set up
  ([tech/reverse-engineering.md](tech/reverse-engineering.md)). Others pass their question to whoever has Ghidra to
  run the agent.

## Reverse engineering on demand

The repository keeps the method, not the answers. Nobody depends on reverse engineering already done: if you need
a fact from the executable, ask again. Method and tools: [tech/reverse-engineering.md](tech/reverse-engineering.md).

1. Run the `nw-re` agent in the background with the question in the prompt, one question per agent, and keep
   working on something else. Do not run the reverse engineering yourself. Prompt shape:

   ```
   <area>-<short subject>
   - Question: what the game's code does, in one or two sentences.
   - Purpose: where the port will use it (file, function) and what it needs (value, formula, unit, sign).
   - Leads: strings, class names, columns, cvars you already saw.
   ```

2. The agent answers in its final report. Its cache stays only on its machine (`re-local/`, not in git) and is
   never committed or cited as a source.
3. Apply the answer:
   - Number: a constant in the code with the answer's value, or `FNWRules::Num(TEXT("<area>.<name>"), value)` when
     it must be tunable without recompiling. The default of `Num` is the value itself: the port must work without
     `re-local/`.
   - Rule or formula: implement it where it applies. Grade H (hypothesis): use it only if there is no alternative,
     and say so in the test.
   - No executable address in code, docs or specs.
4. Add the test item in `docs/tests/<area>.md` and update the area spec (remove the item from "No source").

Damage, critical, grit and mitigation are server-side and not in the client; do not ask for them.

What reverse engineering already settled for combat, actions, mounts and screens is folded into the area specs
(for combat: [game/combat.md](game/combat.md)).

## Delivery checklist

When you close an item:

1. Area spec updated, including its "No source" section.
2. Test item in `docs/tests/<area>.md`.
3. The area's line in [status.md](status.md) (and [backlog.md](backlog.md)) updated.
4. Editor open on the area's map and a screenshot requested from the maintainer.
5. Before merging a branch: the check from the platform table (includes nwbt at the current patch and the `@list`
   test).
