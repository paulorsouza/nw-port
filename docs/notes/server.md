# Server notes

Dated history for the server, network and backend. Rules in effect are in
[../tech/architecture.md](../tech/architecture.md), [../tech/server.md](../tech/server.md),
[../tech/network-test.md](../tech/network-test.md) and [../tech/backend-testing.md](../tech/backend-testing.md).

## 2026-10-11: platforms, languages, Windsward first

- Platforms decided: Windows is the main client platform, the game server is Linux, development stays on Linux. The
  Windows machine for packaging the client was being set up in the week of 2026-10-12.
- Languages decided: pt-BR and en-US only, because two clan players do not speak Portuguese well.
- F4 requested by the maintainer: only the Windsward territory, complete, before any other region.
- The maintainer asked not to run automated game tests (`tools/linux/net_test.sh`) unless asked with those words:
  they took hours, competed for memory and VRAM with New World and did not show what matters.

## 2026-10-11: state of the server work

- F0: engine 5.8.3 built from source (Linux dependencies only; two AutomationTool/IOS icons generated in place so UAT
  compiles). Server `NWPortServer` packaged by `tools/linux/package_server.sh` into `deploy/build/LinuxServer`
  (2.3 GB; cooked with the installed editor, same CompatibleChangelist). The game's own network version
  (`FNWPortModule`, `NetProtocol`) added, because without it the source-built server refused clients of the installed
  engine. Tested with `NW_SERVER=pacote tools/linux/net_test.sh 3`: 3 characters log in and save, intruder refused.
  **F0 completed** on Docker (`deploy/build_images.sh` + `docker compose up -d`; secrets in `deploy/.env`).
- F0 measurement with 10 bots (`NW_SERVER=docker tools/linux/net_test.sh 10`, `-nwbot` walks and attacks), all 10
  connected: CPU 26–31% of one core, RAM ~530 MB, outbound ~66 KB/s in total (~53 kbit/s per player). Conclusion: a
  VM with 2 vCPU / 2–4 GB is more than enough for the clan; bandwidth is not an issue.
- F1, backend: `backend/` (crate `nw-core`) ready, with 8 tests passing against Postgres.
- F1, game server: `UNWBackend` (HTTP) and `ANWGameMode` (ticket in `PreLoginAsync`, spawns at the saved position,
  saved every 30 s and on logout). Abilities and mastery kept by the server and replicated only to the owner. Tested
  with `tools/linux/net_test.sh` (server `-server` + 2 clients): login through the ticket, combat between the two,
  position/health/weapon/abilities saved and restored, connection without a ticket refused. Position saved only with
  the character on the ground.
- Client: entry screen `FNWLoginMenu` built and checked with `-nwuisteps` (login, wrong password, create account by
  invite, create and delete character, Play into the city). Real keyboard typing not yet tested by a person.
- F2 completed. Prediction measurement: with 150 ms simulated latency, 0 refusals and 1 correction in ~30 hits.
- F3 completed. Delivered: catalog of 48 items from the game tables (`tools/make_items.py`: swords, hatchets and
  shields T1–T5, health potions, materials; pt/en names); items and coins in nw-core (migration 0002, whole inventory
  swapped in one transaction, `first_join` for the starter kit); `UNWInventoryComponent` (bag 40, equipment
  sword/hatchet/shield, stacks, potion with the game's scaled heal and 25 s cooldown, damage × weapon gear score); loot
  from the Lost (killer + group within 60 m, roll per player, notice on the right); potion on hotkey 3; inventory
  screen (Tab) with the game's window, grid, tooltip and clicks (left picks up/drops, right uses/equips). Save on
  logout before the character is destroyed (`PawnLeavingGame`). Left for polish: the game's equipment doll, mesh of
  the equipped weapon (appearance was fixed), drag and drop.
- Risk note: building the engine needs ~150 GB; at the time there were 267 GB free on the games SSD.

## 2026-10-13: clan network test setup

Maintainer decision: the servers run on the maintainer's PC; nw-core goes out through Cloudflare Tunnel; the Windows
client is packaged by the maintainer and given to two testers who own the game. It is an exception to the rule that
each player extracts from their own install; the package stays private. Procedure:
[../tech/network-test.md](../tech/network-test.md).

## 2026-10-14: server architecture draft

Draft of the server rules written for the maintainer to approve before they apply to all areas
([../tech/architecture.md](../tech/architecture.md)). The rule violations were surveyed the same day.

Fixed after the survey:

- `ANWSpawnField::Step` walked all characters to find players. Now uses `UNWPlayerGrid`.
- AI target search in the behavior tree: `UNWBehaviorTreeComponent::LookForTarget` and `ANWCreature::HasTarget` use
  `UNWPlayerGrid` (row `UNWBrainComponent::FindTarget` in the survey).
- Periodic save rewrote the whole bag every 5 s even without changes (500 players = ~100 large writes/s to the
  database). Now only what changed is sent (`inventory_patch`).

Database snapshot at the time: the periodic save rewrote the whole bag (up to 500 stacks) of each player every 5 s,
which in the 5,000-player scenario is ~1,000 writes/s, each deleting and inserting hundreds of rows, plus the log
diff. It would not hold.

The maintainer allowed performance tests to be run without a window (server `-server -nullrhi`, bots `-nullrhi`),
through the heavy-work queue, with the editor and New World closed.

## 2026-10-14: recordings

Maintainer requirement: nothing a player saw happen is undone, and everything that touches an item goes to a log.

- Stage 1 (order and log) done: migration 0011. Periodic save changed from 30 s to 5 s. In the test that day,
  decimal numbers in `props` differed in the last digit between before and after, and the log showed a false removal
  and addition on every write; fixed with `serde_json` `float_roundtrip`. Left open by stage 1: the client saw the
  result before the write, and the ground was not saved.
- Stage 2 (operations, ground in nw-core) done: migration 0012.
- Stage 3 (`inventory_patch`, remaining grants as operations) done, no migration.
- Server security reviewed: F10 restricted to admin accounts. Open for the weapons area: `ServerSetMastery`,
  `ServerSetWeaponAbilities`, general RPC rate limit.

## 2026-10-14: load measurements

How to run: [../tech/architecture.md](../tech/architecture.md#how-to-run).

### Editor binary as server

Editor binary as server (`-server`; the packaged server should be faster), Everfall, simulated characters walking
all the time (worst case), no creatures, no connections. Times in ms per tick.

| Simulated | Work/tick | Movement | Tick | Animation | Physics | Note |
|---|---|---|---|---|---|---|
| 0 | ~1 ms | | | | | |
| 50 | 9.3 ms | 3.4 | 1.1 | 4.0 | 0.6 | as it was |
| 50 | 6.7 ms | 3.4 | 1.0 | 0.8 | 0.6 | `-nwexp=anim` |
| 100 | 12.8 ms | 6.7 | 2.0 | 1.4 | 1.0 | `anim` |
| 200 | 24.7 ms | 13.2 | 4.2 | 2.7 | 2.0 | `anim` |
| 50 (L_Arena, flat floor) | 5.9 ms | 2.9 | 1.1 | 0.9 | | `anim`; 60 characters |

Reading:

- Linear growth: ~120 µs per walking character (with `anim`), twice the 60 µs budget. Ceiling of this binary: ~260
  characters walking at the same time.
- Animation on the server was the largest part (80 µs per character). Montage-only animation on the server (hit,
  dodge) cut 80%.
- Movement (`CharacterMovement`): ~65 µs per walking character in the city, ~49 µs on the arena's flat floor. The city
  uses per-triangle collision (`complex as simple`, `import_city.py`/`fix_city.py`), which costs more.
- Character tick: ~20 µs, including the camera (`UpdateCamera`, `UpdateCameraClipHide`), which is useless on the
  server.
- `meshcol` (mesh without collision) and `floor` (fewer floor checks): no measured gain.

After items 1 and 2 (server animates only with montages; camera only on the local player; mesh without collision on
the server when it is not a hit target), 200 simulated in the editor binary: 24.1 → 18.1 ms (movement 12.2, animation
2.3, tick 2.0, physics 0.5).

### Packaged server, Everfall

Packaged server (`tools/linux/package_server.sh`, Development,
`deploy/build/LinuxServer/NWPortServer.sh <mapa> -nwperf -nwsim=<n> -nwcsv=300`), Everfall, simulated characters
walking all the time:

| Simulated | Work/tick | Movement | Animation | Tick | Physics | Memory |
|---|---|---|---|---|---|---|
| 50 | 3.5 ms | 2.4 | 0.5 | 0.3 | 0.2 | 1.0 GB |
| 200 | 13.3 ms | 8.8 | 1.5 | 1.3 | 0.4 | 1.0 GB |
| 500 | 32.3 ms (p95 35) | 22.3 | 3.7 | 3.4 | 0.8 | 1.1 GB |

- ~65 µs per walking character; 500 walking at the same time touch the 33 ms limit. Movement is 70% of the cost
  (~44 µs per walking character).
- Not measured yet at that point: creatures (Windsward), real connections (replication and bandwidth), writes to
  nw-core, and the real mix (a player standing, gathering or in a menu costs less than one walking all the time).

### Packaged server, Windsward

Windsward (`L_SouthWindsward`), packaged server, 200 simulated within 900 m, ~170 creatures spawned around them
(profile 70 s later, `-nwcsvdelay=70`):

| Situation | Work/tick | Movement | Tick | Ticks/s |
|---|---|---|---|---|
| Before | 37 ms (p95 51) | 13.2 | 13.4 | ~25 |
| Player grid + sight every 0.25 s (`UNWPlayerGrid`) | 31–35 ms | 12.1 | 13.0 | ~27 |
| + gathering focus only on the local player (`UNWGatherComponent`) | **26 ms** (p95 29) | 13.0 | 6.9 | 30 |

- The expensive tick was the [E] gathering focus: on the server, creatures and simulated characters count as
  "locally controlled" and searched for gatherables every tick (~7 ms with 370 characters). Same mistake as the
  camera. This produced the rule "locally controlled on the server includes all AI".
- The creature behavior tree costs ~21 µs per creature per tick (`Behavior` in the trace).

### One-hour soak test

Packaged server with the code of commit b884219, Windsward, 200 simulated within 900 m, ~160 creatures around them,
no connections and no nw-core; `soak.log` in `unreal/NWPort/Saved/Perf/`.

| Minutes | Work/tick avg | p95 | Max | Creatures | Memory |
|---|---|---|---|---|---|
| 0–5 | 24.9 ms | 27.4 | 36 | 162 | 2.76 → 2.79 GB |
| 15–20 | 28.0 | 30.5 | 42 | 168 | 2.75 GB |
| 30–35 | 27.5 | 30.1 | 45 | 156 | 2.76 GB |
| 55–60 | 28.1 | 30.6 | 40 | 155 | 2.76 GB |

- No leak: memory flat at ~2.76 GB for 60 min (+12 MB between minutes 15 and 60).
- No degradation: after the first 15 min the tick stays at 27–29 ms; 30 ticks/s the whole time, no report below 29.
- The rise from ~25 to ~28 ms in the first 15 min follows the simulated characters stuck on walls (`is stuck`, 26
  thousand lines; some stuck the whole hour): the simulated character walks straight and does not steer. Test defect
  (`nw.sim`), not a game defect.
- One tick of ~45 ms every ~10 min (20.18.26, 20.28.37, 20.38.48...). Cause not found; nothing in the port has a
  600 s period.
- Log: 78 thousand warnings `GetSocketInfoByName ... No SkeletalMesh` (97% from `NWCreature`): a creature spawned on
  the server without a mesh looks up a socket. Only log noise.
