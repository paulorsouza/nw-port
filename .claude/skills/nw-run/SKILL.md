---
name: nw-run
description: Runs the game for the person (nw-core + dedicated server + client), the automated network test, local nw-core for Play in the editor, and packaging the dedicated server and Docker images. Linux only. Use ONLY when the person explicitly asks to open the game, run the network test, start the backend, or rebuild the server.
---

# Run, test, package (Linux)

Only when the person asks for it in so many words. Never run the game or automated tests to check your own work:
open the editor and ask for a screenshot instead (skill `nw-unreal`).

Before any of these: editor closed (same binary), New World closed (VRAM), no heavy job running.

| Task | Command |
|---|---|
| Open the game for the person (login screen) | `tools/linux/play_local.sh` |
| nw-core for Play in the editor | `tools/linux/backend_dev.sh` (`docs/tech/backend-testing.md`) |
| Network test with automated clients | `tools/linux/net_test.sh <clients>` |
| Clan network test through Cloudflare | `tools/linux/network.sh up` (`docs/tech/network-test.md`) |
| Package the dedicated server | `tools/linux/package_server.sh [--skip-cook]` |
| Docker images | `deploy/build_images.sh` |

## play_local

Map `L_SouthWindsward`. Closing the client window stops everything. Logs in `unreal/NWPort/Saved/Play/`. The person
logs in with their own account; F10 opens the dev menu.

## net_test

```bash
NW_BOT= NW_TEST_SECONDS=60 tools/linux/net_test.sh 1     # 1 client, no bot
NW_SERVER=docker tools/linux/net_test.sh 10               # 10 bots against Docker
```

Client test hooks in `NW_CLIENT_EXTRA`: `-nwtestdev=GoToOre|GiveTools`, `-nwtestgather=N`, `-nwtestinv=...`,
`-nwtestchat=...`, `-nwtestdie=N`, `-nwlang=en`. Logs and screenshots in `unreal/NWPort/Saved/NetTest/`.

## Dedicated server

Heavy: compiles and cooks. Needs the engine built from source (`docs/tech/server.md`).

1. Build the server target in the source engine:
   `Engine/Build/BatchFiles/Linux/Build.sh NWPortServer Linux Development -Project=<repo>/unreal/NWPort/NWPort.uproject -WaitMutex -MaxParallelActions=6`
   (run from the source engine folder).
2. `tools/linux/package_server.sh` (cook and package), or `--skip-cook` when only C++ changed.
3. `deploy/build_images.sh` (uses `--network=host`).
4. Start: `docker compose -f deploy/compose.yaml up -d` (stop a local nw-core on port 8090 first).

Deploying to production or the cloud only on the person's explicit request.
