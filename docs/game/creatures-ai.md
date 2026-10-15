# Creature AI

Creatures run the game's own behavior tree (from `Server.pak`), interpreted by the port. The meaning of the nodes
comes from reverse engineering (ask the `nw-re` agent). What the game runs only on the server and was cut from the
client (target sensor, movement) is done by the port, with the numbers from the creature's slice.

Spawning and the creature list of Windsward: [world/windsward.md](world/windsward.md), step G3.
Tests: [tests/world.md](../tests/world.md), section "Creature AI from the game's trees".

## Pieces

| Piece | Where |
|---|---|
| Export tree + profile | `tools/make_ai.py` → `unreal/NWPort/Data/AI/<set>.json` (runs in `tools/linux/make_data.sh`) |
| Check without Unreal | `python3 tools/ai_sim.py <set> Target_Dist=5 --tag Alerted --times 50 --trace` |
| Interpreter | `UNWBehaviorTreeComponent` (C++), attached by `ANWCreature` when `Data/AI/<set>.json` exists |
| No tree | `UNWBrainComponent` (the port's own styles), as before |

Sources: trees in `extract/serverpak/json/sharedassets/ai/behaviortrees/` (extraction in
[reverse-engineering.md](../tech/reverse-engineering.md)); slice in
`extract/serverpak/client/slices/characters/<x>.dynamicslice.json`.

## What comes from the game

- The whole tree (`<x>_Base.behaviortree` with its embedded subtrees): leash, alert, combat by distance, attack
  choice by utility with a random draw, named cooldowns, timers, repositioning.
- The slice profile: ambient and combat vision (angle, distance), group aggro (radius, how many), target profile
  (`passive` = fights only when hit or when the group fights), replanning (frequency and watched keys), initial
  blackboard (leash, speeds), navigation per type (speed, turn, arrival, repath and default destination).

| Creature | Vision | Group aggro | Run speed | Other |
|---|---|---|---|---|
| Grey wolf | 12 m/75° idle, 30 m/180° in combat | 7.5 m (up to 5) | 9.2 m/s | combat leash 70 m |
| Boar | 12 m/85° | 20 m | 7 m/s | passive |

## What the port does instead of the server

| Part | How |
|---|---|
| Target | nearest player inside ambient vision with line of sight; whoever hits becomes the target; group aggro; target lost after 15 s outside combat vision |
| Sensor keys | `Has_Target`, `Target_Pos`, `Target_Dist` (m), `Target_Angle` (degrees, + to the left), `Orientation_Delta` (hypothesis: target forward − my forward), `Target_Visible`, `Target_Vitals_State`, `Full_Body_Reaction`, `Last_Damage_Pos`, `Spawn_Pos`, `Waypoint_Pos` |
| Reachable | `Target_Reachable` and `Target_Directly_Reachable` always 1 (no navmesh) |
| Movement | straight line to the chosen destination, at the speed and turn of the navigation type; gives up (fails) if it takes too long |
| CAGE tasks | table `TASKS` in `make_ai.py`: attack → creature action; wait; turn to target; dodge step; tag set at the end (Alert → `Alerted`) |
| `Difficulty_Tier` | 2 (no source for the per-mob value) |
| Variant tags | `VARIANT_TAGS` (wolf = `Grey`); the slice's tags come from the TagComponent |

## Adding a creature

1. Animation set (`tools/sets/<x>.json`, `make_datatables.py`), like the boar and the wolf.
2. `CREATURES` in `make_ai.py`: set → slice.
3. `TASKS`: each CAGE task of the tree (`python3 tools/ai_sim.py <x> --trace` and `tools/bt_dump.py` show which) →
   action, wait, turn or step. A task without an entry becomes a short wait.
4. Variant tags, if the tree tests any (a `MannequinTagCondition` that never passes in the simulator).
5. Run `python3 tools/make_ai.py` and check in the simulator per distance band.

## Known differences

- No navmesh: creatures walk straight; the tree assumes everything is reachable.
- The wolf's dodge is a step without animation (the set does not have the animation yet).
- Stunned: the plan pauses. In the game the tree has an escape branch (`BreakoutWindow`); the port does not.
- Timber wolves use the grey wolf's tree.
