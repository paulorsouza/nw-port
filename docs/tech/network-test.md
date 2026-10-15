# Clan network test

Network test of Windsward with a Development build. The servers run on the maintainer's PC, nw-core is published
through Cloudflare Tunnel, and the Windows client is packaged by the maintainer and given to the testers, who own the
game.

The packaged client is an exception to the rule that each player extracts from their own install
([server.md](server.md#cloud)). The package stays private, only for the testers. History of this decision:
[../notes/server.md](../notes/server.md).

## Pieces

| Piece | Where | Address |
|---|---|---|
| Postgres + nw-core | Docker on the server PC, project `nw-rede` (`deploy/compose.yaml` + `deploy/compose.rede.yaml`) | `127.0.0.1:8091` → `https://<NW_REDE_HOST>` (Cloudflare Tunnel) |
| Game server (Unreal dedicated Linux, Windsward) | Docker on the server PC, same project | UDP 7777 on `<IP of the PC>`, reached through Cloudflare WARP |
| Client | Each tester's Windows | `Jogar.bat` with `-nwbackend` and `-nwconnect` |

`NW_REDE_HOST` is set in `local.env` (see `local.env.example`). The development nw-core
(`tools/linux/backend_dev.sh`, port 8090) stays separate, with another database. Network secrets are in
`deploy/.env` (not in git).

## Why WARP

The internet provider uses CGNAT, so opening the port on the router does not bring the game in from outside.
nw-core goes out through the tunnel (HTTP). The game (UDP) comes in through the private network of the same tunnel:
each tester turns on the WARP app of the maintainer's Zero Trust organization, and the client connects to
`<IP of the PC>:7777` as if it were on the local network.

Reserve that IP for the PC in the router's DHCP so it does not change.

## One-time setup

1. In a terminal: `cloudflared tunnel login` (opens the browser; choose `<your domain>`). Then
   `tools/linux/rede.sh tunnel` (creates the tunnel `nw-rede`, the `<NW_REDE_HOST>` and the private route
   `<IP of the PC>/32`).
2. Zero Trust dashboard (one.dash.cloudflare.com), free organization:
   - Settings > Custom pages / Team name: choose the team name (testers type it into WARP).
   - Settings > Authentication: method "One-time PIN" (code by email).
   - Settings > WARP Client > Device enrollment permissions: a rule with the emails of the maintainer and the
     testers.
   - Settings > WARP Client > Default profile > Split Tunnels: switch to "Include IPs and domains" and include only
     `<IP of the PC>/32` (the rest of the testers' internet does not go through Cloudflare).
   - Settings > Network > Proxy: on, with TCP and UDP.
3. Each tester: install Cloudflare WARP (1.1.1.1). In Preferences > Account > "Login with Cloudflare Zero Trust",
   type the team name, sign in with the code from the email, and keep it on while playing.

## Server (on the server PC)

```bash
tools/linux/rede.sh build
```

```bash
tools/linux/rede.sh up
```

| Command | What it does |
|---|---|
| `tools/linux/rede.sh build` | Builds the server with the source engine, cooks Windsward and rebuilds the images. Heavy; editor closed. |
| `tools/linux/rede.sh up` | Starts Postgres, nw-core, the game server and the tunnel (`<NW_REDE_HOST>` + private network). |
| `tools/linux/rede.sh invite` | Generates account invites. |
| `tools/linux/rede.sh admin <conta>` | Enables F10 for that account only. Without admin the server refuses and the menu does not open. |
| `tools/linux/rede.sh down` | Stops everything. The database is kept. |
| `tools/linux/rede.sh logs` | Shows the logs. |

## Windows client (on the maintainer's Windows machine)

With the project extracted and compiling on Windows ([../setup.md](../setup.md), [windows.md](windows.md)), from the
repository root:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\unreal\NWPort\NWPort.uproject" -platform=Win64 -clientconfig=Development -map=/Game/NW/Maps/L_SouthWindsward -build -cook -stage -pak -archive -archivedirectory="%CD%\deploy\build" -unattended -nop4 -utf8output
```

In `deploy\build\Windows`, create `Jogar.bat`:

```bat
start "" "%~dp0NWPort.exe" -nwbackend=https://<NW_REDE_HOST> -nwconnect=<IP of the PC>:7777
```

Zip the `Windows` folder and send it only to the testers (with WARP on). Each one logs in on the entry screen with an
invite (`tools/linux/rede.sh invite`).

## Version

Client and server need the same C++ and the same data (`Data/`). After changing code, rebuild both.
