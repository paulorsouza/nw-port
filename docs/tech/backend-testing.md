# Testing with nw-core

Every test in the editor uses the local nw-core. Play always logs in with the same account and character, and the
bag, equipment, position, levels, storage and NPC rewards are saved between one Play and the next.

## Use

1. Start nw-core (once per boot; running it again does nothing if it is already up):

   ```bash
   tools/linux/backend_dev.sh
   ```

   It starts Postgres (`nw-core-db`, `backend/compose.yaml`), builds nw-core if the source changed (through the
   heavy-work queue), starts the API on `127.0.0.1:8090`, and creates the account `editor` with the character
   `Exilado` if they do not exist.
2. Open the editor and press Play (single player, "Standalone"/"Play in Viewport"). The log shows
   `Play com backend: Exilado (editor)`.
3. Stopping Play saves everything (it also saves every 5 s). Item history of the character:
   `tools/item_log.py --personagem Exilado` (filters `--item`, `--serie`, `--desde`).

Other commands: `tools/linux/backend_dev.sh status | log | down`. `down` stops only the API; the database keeps its
data.

There is no Windows equivalent of `backend_dev.sh` yet ([windows.md](windows.md)).

## How it works

- `backend/dev.env` (test values, in the repository): `NW_BACKEND`, `NW_SERVER_SECRET`, `NW_PIE_ACCOUNT`,
  `NW_PIE_CHARACTER` and the password `NW_TEST_PASSWORD`.
- In single-player Play (PIE standalone), `ANWGameMode::BeginPlay` calls `UNWBackend::EnablePie`, which reads
  `dev.env`. The local player enters through the same path as a client on the dedicated server: login, `join`
  (ticket) and redeem (`/internal/tickets/redeem`). The saved character is applied (position if it is the same map;
  bag, equipment, progress). It saves through `PUT /internal/characters/{id}/state` on the timer and when the world
  closes.
- The account session lives only in Play's memory (it does not write `[NW.Session]` to GameUserSettings).
- With nw-core down, Play runs without a backend (starter kit, nothing saved) and shows "Jogando sem backend"
  ("Playing without backend") on screen. To play without a backend on purpose: `tools/linux/backend_dev.sh down`.
- With a backend, trade skill levels count (without one they were free). To test something that needs a level or
  material, use the F10 buttons.

## Reset the character

Deleting and recreating goes back to the start (starter kit):

```bash
docker exec nw-core-db psql -U nw -d nw -c "delete from characters where lower(name) = 'exilado'"
```

```bash
tools/linux/backend_dev.sh
```

## Other tests

- `tools/linux/play_local.sh`: dedicated server + client with the entry screen (uses the same nw-core; needs the
  editor closed).
- `tools/linux/net_test.sh`: several clients without a window (accounts `NW_TEST_ACCOUNTS`).
- nw-core tests: [backend/README.md](../../backend/README.md) (each test creates a temporary database).

Running the game is not something contributors do on their own to test. The maintainer opens the editor or the game
and sends a screenshot; see [workflow.md](../workflow.md). Performance tests without a window are the exception
([architecture.md](architecture.md#how-to-run)).

### Local play (`play_local.sh`)

Only when the maintainer asks to open the game.

- Before: Unreal editor closed (it uses the same binary), New World closed (8 GB of VRAM), nothing heavy running.
- `tools/linux/play_local.sh` opens map `L_SouthWindsward`.
- Closing the client window ends everything. Logs in `unreal/NWPort/Saved/Play/`.
- The player logs in with their own account. Dev menu on F10.

### Automated network test (`net_test.sh`)

Only when the maintainer asks for this test by name. Automated game tests took hours, competed with New World for
memory and VRAM, and did not show what matters. By default, open the editor and ask for a screenshot.

```bash
NW_BOT= NW_TEST_SECONDS=60 tools/linux/net_test.sh 1          # 1 client, no bot
NW_SERVER=docker tools/linux/net_test.sh 10                    # 10 bots against Docker
```

Client test hooks (in `NW_CLIENT_EXTRA`): `-nwtestdev=GoToOre|GiveTools`, `-nwtestgather=N`, `-nwtestinv=...`,
`-nwtestchat=...`, `-nwtestdie=N`, `-nwlang=en`. Logs and screenshots in `unreal/NWPort/Saved/NetTest/`.
