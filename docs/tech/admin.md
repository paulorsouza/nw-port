# Admin page

A web page served by the game server for demos and tests. From the browser you can:

- see who is on the server;
- create bots and drive them or your own character;
- run the F10 commands on any player;
- give coins and levels;
- summon creatures and steer their AI.

Code: `NWAdmin.h/.cpp`. Page: `unreal/NWPort/Data/Admin/index.html`. The server reads the page on every request, so
after editing it, reload the browser. Development builds only (not in Shipping).

## Turn it on

| Where | How | Page |
|---|---|---|
| Dedicated server | `-nwadmin` (port 8780 + game port − 7777) or `-nwadmin=<port>` | Windsward http://127.0.0.1:8780, Everfall :8781, Arena :8782 |
| Play in the editor | console `nw.admin` (or `nw.admin <port>`) | http://127.0.0.1:8780 |
| `tools/linux/play_local.sh`, `tools/linux/demo_local.sh`, Windows demo `Servidores.bat` | already passes `-nwadmin` | as above |

- It listens on 127.0.0.1 only. A request with an `Origin` or `Host` that is not `127.0.0.1` or `localhost` is
  refused (403). Another site in the browser cannot drive the server.
- In the header you can switch to another map's server. Each server is a separate process.

## Page

- **List**: players, bots, creatures, NPCs (dummies, the Lost, arena fighters) and load-test players (hidden by
  default). Rows are sorted by distance to the selected character.
  - Click selects a character.
  - Right click picks a **target**, used by Follow, Fight, Attack target, Teleport and summons.
- **Map** (from above, north up):
  - click on the ground: the selected character walks there;
  - shift+click: teleport;
  - right click: pick target;
  - wheel: zoom;
  - middle drag: pan.
  - Dashed lines show who attacks or follows whom.
- **Inspector**, by kind:
  - Player or bot, Drive card:
    - Follow, Fight, Go to target, Wander, Stop;
    - one action per click: light, heavy, Q/R/F, dodge, jump, block, unblock, swap weapon.
  - Player: coins (cents in the API, units on the page), level, unlimited bag, every F10 command.
  - Bot: weapon (any weapon set of the character).
  - Creature with a behavior tree:
    - Freeze, Passive, Attack target, Drop target, Replan, vision ×;
    - running path of the tree, tags, cooldowns, blackboard.
    - A blackboard value edited on the page stays fixed over what the sensor writes (orange). × releases it.
  - Arena fighter (brain): style, block and dodge chance, reaction, hold distance, aggro distance.
  - Everyone: heal, god mode, kill, max HP, damage, speed. Not for players: pause (time stopped for that character),
    remove.
  - Summon: creature of the catalog (★ = has the game's tree), count, distance, aggro on the target; bots with a
    starting mode.
- Header: time dilation of the whole server; Remove summoned (bots and everything summoned by the page or by F10).

## How it acts

- **Players**: the page sends the order to the player's own client (`ClientAdminDrive`, `ClientAdminAct`).
  - The client moves and attacks with the same prediction as the keys. Driving a player who is not you also works.
  - The F10 commands run on the server without the admin account check (`RunDev`). They save like F10, with the
    reason `dev:<command>`.
- **Coins and level**: saved to nw-core with the reason `admin:<op>`, like F10. The coin cap still applies; nw-core
  also enforces it.
- **Unlimited bag, god mode, stats, pause, AI**: session only.
- **Bots**: server characters without a connection, marked as players (`bSimPlayer`). Creatures hunt them and the
  arena brain fights them. They do not log in, have no nw-core record and are not saved. They disappear with the
  server or with Remove summoned.
- **Creatures**: same spawn as the F10 arena waves (catalog of `guardaventos_spawns.json`, tag `NWDev`).

## API

The page uses only these routes; scripts can use them too.

| Route | Answer |
|---|---|
| `GET /api/state` | map, time dilation, coin cap, `actors[]` (id, kind, name, position, health, combat state, drive, AI, coins, level...) |
| `GET /api/catalog` | F10 commands (`dev`, `dev_named`), creatures, brain styles, weapons |
| `POST /api/cmd` | JSON `{"op": ..., "id": <actor>, "target": <actor>, ...}`; `{"error": ...}` with 400 when it fails |

`id` and `target` are the `id` values from `/api/state`. They are valid while the actor exists.

| op | Fields |
|---|---|
| `dev` | `command`: name from the catalog (enum or registered id) |
| `coins` | `amount` in cents (negative takes away) |
| `level` | `level` |
| `bag` | `on` |
| `heal`, `kill` | — |
| `god`, `pause` | `on` |
| `stats` | `max_hp`, `damage`, `speed` (×), `aggro_range` (m) |
| `teleport` | `target` or `to: [x, y]` / `[x, y, z]` (cm; without z it lands on the ground) |
| `drive` | `mode`: `move`, `follow`, `fight`, `wander`, `stop`; `target` or `to` |
| `act` | `action`: `light`, `heavy`, `ability1`..`3`, `dodge`, `jump`, `block`, `unblock`, `swap` |
| `weapon` | `weapon` (bots) |
| `remove`, `remove_all` | — |
| `bot_spawn` | `count`, `mode`, `name`; near `id` (or the player start) |
| `creature_spawn` | `creature`, `count`, `distance` (m), `aggro`; near `id` |
| `slowmo` | `value` (0.05–4) |
| `ai` | answer `ai`: set, frozen, passive, vision, target, `plan[]`, `blackboard`, `overrides`, `tags`, `cooldowns` |
| `ai_target` | `target` (0 drops) |
| `ai_freeze`, `ai_passive` | `on` |
| `ai_replan` | — |
| `ai_vision` | `value` (×) |
| `ai_bb` | `key`, `value` (without `value`, releases) |
| `brain` | `style`, `block_chance`, `dodge_chance`, `reaction`, `hold_distance` |

```bash
curl -s 127.0.0.1:8780/api/state | jq '.actors[] | select(.kind=="player") | {id, name, coins}'
curl -s 127.0.0.1:8780/api/cmd -d '{"op":"coins","id":123,"amount":100000}'
```
