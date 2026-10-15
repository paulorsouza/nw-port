# Architecture

How the repository is laid out, how the client, the dedicated server and the nw-core backend fit together, and the
server rules every area must follow. The cloud plan, phases and server security are in
[server.md](server.md). Measurements and dated decisions are in [../notes/server.md](../notes/server.md).

Status: draft. The rules below wait for maintainer approval before they apply to all areas (see
[Open questions](#open-questions)).

## Repository map

| Path | Role |
|---|---|
| `unreal/NWPort` | Unreal project. Client and dedicated server build from the same C++ in `Source/`; configuration in `Config/`. `Content/` and `Data/` are generated from each person's New World install and are not in git. |
| `tools/` | Extraction and conversion pipeline. The Python scripts (`tools/*.py`) are the same on Linux and Windows. |
| `tools/linux/` | Linux system scripts (extraction, Unreal import, local backend, server packaging, network test). |
| `tools/windows/` | Windows-only scripts (diagnostics, `ooz.dll` and `nwbt.exe` builds). See [windows.md](windows.md). |
| `tools/ue/` | Scripts that run inside Unreal. |
| `tools/steps/` | Extra pipeline steps per area ([tools/steps/README.md](../../tools/steps/README.md)). |
| `tools/sets/` | Animation and mesh sets per character; the player set is split per area (`tools/sets/player/`). |
| `backend/` | nw-core: Rust (axum + sqlx) and Postgres ([backend/README.md](../../backend/README.md)). |
| `deploy/` | Docker for the dedicated server and the backend (`compose.yaml`, `compose.rede.yaml`, `Dockerfile.gameserver`, `build_images.sh`). Packaged server output goes to `deploy/build/`. |
| `patches/` | The nw-port patch for nwbt ([patches/README.md](../../patches/README.md)). |
| `bin/` | Prebuilt tool binaries (nwbt, libooz, vgmstream-cli, oggenc, texconv). |
| `docs/` | Documentation ([docs/README.md](../README.md)). |
| `extract/`, `vendor/` | Not in git: extracted game data and third-party checkouts. |

Tool details: [tools.md](tools.md). How each asset type leaves the game: [asset-pipeline.md](asset-pipeline.md).

## Client, dedicated server and nw-core

| Part | Runs | Owns |
|---|---|---|
| Client | Unreal, on each player's PC, built from that player's own New World install | Rendering, input, prediction. No game rules. |
| Dedicated server | Unreal dedicated server (Linux, no rendering), one process per map | Real time: movement, collision, combat, traversal, AI, projectiles, replication. |
| nw-core | Rust service + Postgres, one for all servers | What persists: accounts, characters, items, coins, storage, market, houses, cities, item log. Economy rules. |

The full diagram, the entry and economy flows, and why the server is not rewritten in Rust are in
[server.md](server.md#architecture). The nw-core API is in [backend/README.md](../../backend/README.md).

## Target and budget

- Target: one channel (one server process per map) with up to 500 players spread across the world. Not 500 in the
  same place: the city gathers dozens, the rest are spread out.
- More players means more channels of the same map. Character, storage and market already live in nw-core and are
  shared by all channels.
- The server runs at 30 ticks/s (33 ms per tick), almost all logic on one core. With 500 players that leaves about
  60 µs per player for movement, combat, nearby AI and replication.
- So the cost of each system must grow with what is near the player, never with the size of the world or the total
  number of players.

## Rules for all areas

1. **Do not walk the world** (`TActorIterator`, `TObjectIterator`, `GetAllActorsOfClass`) in `Tick`, AI, combat or
   RPC. Nearby searches go through the spatial grid (`FNWSpatialGrid`, to do; `UNWPlayerGrid` already indexes
   players). Searches for a known actor go through a registry (for example `ANWNpc` by variant, `ANWStation` by city).
   Walk the world only in `BeginPlay` or in tools.
2. **Tick only when needed.** An actor or component with nothing to do has tick disabled. What can be a timer (1 s,
   0.25 s) is not a tick. Expensive per-player logic runs on an interval, not every tick.
3. **AI only near players.** A creature with no player within N m does not think, move or look for targets
   (`UNWBehaviorTreeComponent`, `UNWBrainComponent`). `ANWSpawnField` already spawns only near players.
4. **Replication.** Iris enabled in the project. Properties use the push model (`MARK_PROPERTY_DIRTY`). Large lists
   (bag, storage, loot bag items) use an incremental list (`FFastArraySerializer`). Idle actors are dormant
   (`NetDormancy`) and wake only when they change (stations, NPCs, loot bags, doors). Replication distance and rate per
   type (`NetCullDistanceSquared`, `NetUpdateFrequency`): distant creatures and players get less, gatherables and NPCs
   only nearby.
5. **Only the owner receives what is theirs** (bag, storage, progression, F10: `COND_OwnerOnly`). This is already the
   case.
6. **Client RPCs carry intent, never results.** The server checks distance, owner, quantity, level and cooldown. Rate
   limit per player (to do; today only chat has one).
7. **Persistence.** nw-core is the source of truth. The server writes small operations (stage 2 of the
   [recordings](server.md#recordings)), and the periodic save writes only characters that changed and only what
   changed.
8. **Measure.** Each expensive system has a timing counter (`SCOPE_CYCLE_COUNTER` /
   `TRACE_CPUPROFILER_EVENT_SCOPE`) and appears in the load test report.
9. **"Locally controlled" on the server includes all AI.** Local-player logic (camera, [E] focus, HUD, camera shake)
   checks `IsLocallyControlled() && IsPlayerControlled()`.

## Open items: code that breaks the rules

| Where | What | Cost with many players | Area |
|---|---|---|---|
| `UNWBrainComponent::FindTarget` | Each creature walks all characters (players and creatures) to find a target. The behavior tree path is fixed: `UNWBehaviorTreeComponent::LookForTarget` and `ANWCreature::HasTarget` use `UNWPlayerGrid`. Whether `FindTarget` itself still walks all characters is unclear in the original. | creatures × characters per search | not in the area table (AI); `NWBrainComponent` belongs to the weapons area |
| `UNWBehaviorTreeComponent::Aggro` | Walks all creatures to call the group | same | not in the area table (AI) |
| `ANWCharacter::Tick` (attack homing) | Walks all characters every tick during homing | attacking players × characters | shared |
| `ANWCharacter::TickDummy` | Walks all characters | dummies × characters | weapons |
| `UNWInteractorComponent` (NPC, shop, storage shed) | Walks NPCs and stations on every RPC | low; becomes a registry | systems |
| `ANWLootBag::GroundWeight` | Walks all loot bags on every drop | low; becomes a registry per owner | systems |
| `UNWGatherComponent::ServerGather` | Walks all gather components to see if the node is taken | gathering players × players | gathering |
| Project | No Iris, no push model, no dormancy, no replication distance per type | replication grows with the total | server/network |
| Inventory and storage | Replicated lists are plain `TArray` (bag of 500 slots) | each change sends more than needed | systems |

Items already fixed are listed in [../notes/server.md](../notes/server.md).

Log noise to fix together with these items: creatures spawned on the server without a mesh look up sockets
(`GetSocketInfoByName ... No SkeletalMesh`, 97% from `NWCreature`).

## Database layout

One Postgres for all channels. Sizing scenario: 10 full channels = 5,000 players on the same nw-core and the same
Postgres.

- Rewriting the whole bag (up to 500 stacks) of every player every 5 s would be about 1,000 writes/s, each deleting and
  inserting hundreds of rows, plus the log diff. That does not hold.
- With small operations and a periodic save of only what changed: about 0.2–0.5 item operations per player per second
  (gathering, loot, crafting, storage) = 1,000–2,500 small transactions/s (5–10 rows each), plus position and health
  every 30–60 s. A Postgres with 8–16 cores and NVMe handles this with room to spare. The real number comes from the
  nw-core load test.

Rules:

- Never rewrite a whole collection. A write is what changed.
- Position, health and XP do not need to reach the database every 5 s (every 30–60 s and on logout is enough). Items
  and coins are written immediately.
- `item_log` is partitioned by day with retention (30–90 days). Gathering and loot are aggregated (one row per item
  per minute, not one per hit). Without this the scenario produces about 200 million rows per day.
- nw-core keeps no state in memory and runs as several copies, with PgBouncer in front of Postgres.
- Market transactions stay short (orders for popular items compete for the same rows).

If load grows past this, in order: the log in a separate database, a read replica for market lists, and last,
characters split across databases by id (the market in its own database).

## Load test plan

Three parts, to measure without 500 PCs:

1. **Simulated players on the server.** The server command `nw.sim <n> [region]` creates n characters controlled by
   AI (no connection), spread like real players (some in the city, some gathering, some fighting creatures). Measures
   CPU of movement, combat, AI, gathering and persistence (the simulated characters save to a test nw-core).
2. **Real bots** (`tools/linux/net_test.sh <n>`, `-nwbot`, no rendering): a few dozen, to measure actual replication
   and bandwidth.
3. **nw-core load** (script): N characters writing at the rate measured in parts 1 and 2 (operations and periodic
   save) against a test Postgres. Measures transactions per second, latency (p50/p95/p99) and log size.

Report every 10 s in the server log: tick time (average, p95, max), time per system, players, active creatures,
replicated actors, bytes sent per player, writes per second to nw-core.

Rounds: 50, 100, 200, 500 simulated + 20 bots, in Everfall (`L_QuedaEterna`) and in Windsward (`L_SouthWindsward`).

### How to run

Headless server, no nw-core, through the heavy-work queue, with the editor and New World closed:

```bash
tools/linux/heavy.sh timeout -s INT 90 /mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor \
  "$PWD/unreal/NWPort/NWPort.uproject" /Game/NW/Maps/L_QuedaEterna -server -nullrhi -unattended -nosplash \
  -nwperf -nwsim=100 -nwsimraio=300 -nwcsv=300 -abslog="$PWD/unreal/NWPort/Saved/Perf/sim.log"
python3 tools/perf_csv.py --filter Exclusive/GameThread
```

The engine path is an example; use your own. Packaged server (`tools/linux/package_server.sh`, Development):

```bash
deploy/build/LinuxServer/NWPortServer.sh <map> -nwperf -nwsim=<n> -nwcsv=300
```

Other flags used in the measurements: `-nwcsvdelay=<s>` (start the profile later, for example after creatures
spawn), `-nwexp=anim` (experiment: montage-only animation on the server). Long runs write `soak.log` to
`unreal/NWPort/Saved/Perf/`.

Performance tests are the one kind of game run that contributors may start without being asked: no window (server
`-server -nullrhi`, bots `-nullrhi`), through the heavy-work queue, with the editor and New World closed. Other game
runs follow [workflow.md](../workflow.md).

Known defect of the test itself: `nw.sim` characters walk straight and do not steer, so some get stuck on walls
(`is stuck` in the log). This is a test defect, not a game defect.

## Open optimization paths

Measured and recorded (numbers in [../notes/server.md](../notes/server.md)). Come back when a channel fills up.

1. **Movement**: about 44 µs per walking character, the largest remaining cost. Options: creatures far from players
   with less frequent movement and `NavWalking`; simpler city collision (the city uses per-triangle collision,
   `complex as simple` from `import_city.py`/`fix_city.py`, about 25% of movement). `MoveComponent` and the floor
   check are most of it.
2. **Client movement send rate** at most 30/s (15/s when idle) and speed-hack detection enabled (`DefaultGame.ini`,
   `GameNetworkManager`). Waiting for the network test with the maintainer (walk, jump, dodge without rubber-banding).
3. **Montage-only animation on the server and no mesh collision on the server** (`ANWCharacter`). Waiting for the
   maintainer's combat test (hit, combo, dodge, homing, bow) in the editor and over the network.
4. **Real network**: connected bots (`tools/linux/net_test.sh`) to measure replication and bandwidth; then Iris,
   replication distance per type, dormancy.
5. **Database**: nw-core load test (script in `tools/`). Saving only what changed is done (`inventory_patch`).
6. The other entries in [Open items](#open-items-code-that-breaks-the-rules) (attack homing, dummies, `ServerGather`,
   loot bags on the ground, NPCs and stations through a registry).

## Work order

1. Counters per system + report + `nw.sim` (no behavior change). Measure the current code.
2. Fix in order of cost (likely: AI target search → spatial grid; AI asleep far from players; Iris + distance per
   type + dormancy; incremental inventory; saving only what changed).
3. Measure again after each item.
4. Channels (server registry in nw-core, channel choice at login, group kept together, chat across channels) when a
   channel fills up.

## Open questions

- These rules are a draft waiting for maintainer approval before they apply to all areas.
- A tick of about 45 ms appears every ~10 min on long runs. Cause not found; nothing in the port has a 600 s period.
- `UNWBrainComponent::FindTarget`: see the first row of [Open items](#open-items-code-that-breaks-the-rules).
