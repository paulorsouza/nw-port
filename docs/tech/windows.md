# Native Windows port

Goal: extract, convert, import, build and play on Windows without WSL in the pipeline. Docker stays allowed for
Postgres and deploy. The Linux production server stays as it is.

The pipeline Python (`tools/*.py`) is the same on Linux and Windows. What is Windows-only lives in `tools/windows/`.
Delivery logs, validation runs and the change log are in [../notes/windows.md](../notes/windows.md). Setup from
scratch: [../setup.md](../setup.md) and the `nw-setup` skill (`.claude/skills/nw-setup/windows.md`).

## Plan

| Step | Scope | Status |
|---|---|---|
| 1. Native tools | Build `ooz.dll`, `nwbt.exe` and `dbgwater.exe`; fix DLL loading and JSON RTTI resolution; provide Windows `vgmstream-cli` and `oggenc` with recorded versions. | Built; not validated on real paks |
| 2. Shared configuration | Standardize `NW_GAME`, `NW_UE`, tool paths and subprocesses; remove Linux dependencies from the Python scripts; check paths with spaces and Unicode. | Done |
| 3. Orchestration | Port `build_all`, `convert_city`, `convert_gatherables`, `render_ambient`, `make_data` and `game_version` to Python, keeping resumable steps, logs, queues, memory/disk limits and child-process cleanup. Include the extra steps in `tools/steps/`. | Done; not run on real data |
| 4. Unreal | Port `ue_import_all` and `check` to `Build.bat`/Win64 and `UnrealEditor-Cmd.exe`, keeping validation through the commandlet log; check maps, materials, animations and audio. | `tools/ue_import_all.py` exists; not validated |
| 5. Local run | Port `play_local` and `net_test`, keeping Docker for the database; support native Rust backend, `UnrealEditor -server` and the client. A dedicated Win64 package comes later if needed; it is not required to keep the Linux deploy. | Pending |
| 6. Acceptance | Build from scratch, interrupt and resume steps, check the game version, import all assets, play with an account and persistence, and run a network test. Update [../setup.md](../setup.md) and [tools.md](tools.md). | Pending |

## What works

Native tools:

- Shared C++ shim for MSVC/GCC that exports `OodleLZ_Decompress` on Windows. Reproducible `ooz.dll` x64 build with
  MSVC.
- `nwbt.exe` and `dbgwater.exe` built natively with a pinned dependency and a reapplicable patch
  ([patches/README.md](../../patches/README.md)).
- `texconv.exe` is in `bin/` and is called directly on Windows, without wine.
- nwbt on Windows: `NWBT_OODLE` and `ooz.dll`; JSON RTTI without `exec.LookPath`/`PATHEXT`; `AppendToPATH` uses the
  system separator and keeps existing entries. `NWBT_OODLE` points to a DLL explicitly; on Windows a configured path
  that does not exist fails, with no silent fallback.

Shared configuration:

- `NW_GAME` wins over `GAME`, which is still accepted for compatibility.
- `build_set.py`, `make_sound_map.py` and `extract_audio.py` adapted. Audio works on Windows without symlink
  privileges.
- `nwplatform` finds Blender in `~/.local/opt` on Linux.

Orchestration:

- `tools/game_version.py`: Steam manifest and SHA256 hash without `awk`/`sha256sum`. Accepts `NW_GAME`, `--game`,
  `--exe` and `NW_IGNORE_VERSION`. Without a manifest it requires the executable hash. Uses `hashlib.file_digest`
  (Python 3.11 or newer).
- `tools/make_data.py`: the `Data/` generation sequence, including optional steps and screens (`make_ui_sounds`,
  `make_port_items`). Uses the same Python that started it, the repository directory, and arguments without a shell.
  `--dry-run` lists the steps without starting subprocesses or writing `Data/`. `make_data.sh` only calls the `.py`.
- `nwprocess.py`: available physical memory and disk space without external Linux tools.
- `heavy.py`: heavy-work lock shared between processes and inherited by children. Runs the command in the caller's
  directory and returns its exit code. `python`/`python3` in the command become the queue's Python.
- Windows Job Objects: the whole process tree ends on failure or cancel. A wrapper waits until the process is assigned
  to the Job before starting the command, so no child escapes supervision.
- Steps: completed steps are skipped. A forced step that fails loses its marker and stays pending. `--dry-run` creates
  no locks, logs, caches or markers.
- `build_all.py`: 14 main steps and three Everfall steps (including `sistemas-01/02/03`), with logs per step, game
  version, RTTI, open-process validation, extraction and conversion. Unknown future Bash area steps raise an explicit
  error so they do not silently disappear from the Windows flow.
- `convert_city.py`, `convert_gatherables.py` and `render_ambient.py` replace the matching Bash helpers.
  `convert_npcs.py` uses native tools and supervision.
- The nwbt CSV manifest keeps names with spaces. Truncated GLBs are not reused. Clean outputs, terrain and audio are
  promoted only after success.
- Failures that used to be tolerated now leave the step pending: ambiguous capital, missing heightmap, layer
  conversion with a process error, missing capitals GLB, meshes with the same output name.
- `build_set.py` and `convert_npcs.py` refuse an outdated nwbt, call Blender with `--python-exit-code 1`, and check
  that the expected glb/fbx files were rewritten (`tools/outputs.py`).
- `tools/windows/doctor.py` and `tools/linux/check.sh` flag an nwbt built with another patch. `doctor.py --full` and
  `check.sh` convert a cube from the game through `@list` and fail if nothing is written.
- `tools/windows/check_migration.py` checks caches and references of a checkout copied from WSL. `--apply` fixes paths
  and audio banks with a backup and invalidates only the affected markers. The check is structural; it does not
  replace acceptance with real data and Unreal.

## Pending

- Real extraction of JSON, textures, audio, meshes and animations on Windows.
- Unreal import on Windows: `tools/ue_import_all.py` exists but has not been run on real assets. The original also
  lists the import orchestrator and the Windows launcher as pending (unclear in the original which parts are missing).
- `tools/ue_import_all.py` on Linux: steps `sistemas-04/05` (`tools/steps/systems.ue.sh`), the heavy-work queue,
  and the open-editor check on Linux.
- No Windows equivalent yet: `backend_dev`, `ui_shot`, `check` (use `doctor.py` and
  `python -m unittest discover -s tools/tests`), `rede`, `net_test`, `play_local`. Packaging client and servers:
  `tools/windows/build_demo.ps1` (Demo build, below).

## Acceptance status

Not accepted. No real extraction or Unreal import has run on Windows.

What the current tests prove: script behavior (queue and lock, resume and step markers, memory/disk monitoring,
child cleanup, Unreal commands and the local launcher with test processes, synthetic fixtures). They do not prove
asset fidelity:

- The audio pipeline was tested with synthetic decoder/encoder programs, including failure. That does not validate
  real codecs.
- The Kraken block test validates loading, ABI and result copy with an uncompressed block. It does not validate the
  compressed blocks of real paks.

Acceptance that needs a machine with the game and Unreal: unpack the paks of the game build, compare
JSON/textures/meshes/animations/audio, assemble the project in Unreal, check rendering and collision, play with
persistence and network, and measure memory and time on the real data volume.

### CI

`.github/workflows/native-tools.yml` runs the tests with Python 3.13 on Windows and Linux, without the game, Unreal
or access to the private fork. On Windows it also builds `ooz.dll` with MSVC and tests the ABI, an uncompressed block,
an invalid header and buffer limits. On Linux the DLL test is skipped. The workflow runs on pushes to
`windows-convert` and on PRs that change the tools.

## Limits

The conservative limits of the original flow stay:

- 10 GiB during heavy work.
- City conversion waits for 12 GiB and stops below 8 GiB.
- Gatherables run in batches of eight with two workers, require 16000 MiB before a batch and stop below 10000 MiB.
- Converters check for 30 GiB free on the disks involved.
- Windows uses below-normal CPU priority; Linux uses nice 19.

## Commands

Use Python from python.org (the Microsoft Store Python is refused) and `python` instead of `python3`.

| Action | Command |
|---|---|
| Diagnostics (tools, nwbt of the current patch; `--full`: audio, Blender, Unreal and the `@list` test) | `python tools/windows/doctor.py --full` |
| Heavy-work queue | `python tools/heavy.py <comando>` |
| Whole extraction or step NN | `python tools/build_all.py [NN]` (`--dry-run` lists) |
| Build and import into Unreal, step NN (editor closed) | `python tools/ue_import_all.py [NN]` |
| `Data/` | `python tools/make_data.py` |
| City, gatherables, ambient sound | `python tools/convert_city.py`, `convert_gatherables.py`, `render_ambient.py` |
| Game version | `python tools/game_version.py --exe` |
| Checkout copied from WSL | `python tools/windows/check_migration.py [--apply]` |
| Python tests | `python -m unittest discover -s tools/tests -v` |

Examples:

```powershell
python tools/build_all.py --dry-run
python tools/build_all.py 07 --dry-run
python tools/convert_city.py settlement_southwindsward_complete --dry-run
python tools/convert_gatherables.py tools/arena/meshes.txt extract/ue/arena --dry-run
python tools/heavy.py python -c "print('fila nativa')"
python tools/make_data.py --dry-run
```

After configuring the dependencies and `NW_GAME`:

```powershell
python tools/game_version.py --exe
python tools/build_all.py
python tools/build_all.py 07           # redo one step, even if it already has a marker
python tools/build_all.py sistemas-02
python tools/make_data.py
```

Variables: `NW_GAME` (game), `NW_UE` (engine), `NW_BLENDER` (`blender.exe`), `NW_BUDDY` (RTTI tables checkout, if
not in `vendor/nw-buddy`), `NW_PYTHON` (Python used by the build scripts).

## Demo build

`tools/windows/build_demo.ps1` builds a Development demo for the clan: client with `L_SouthWindsward`,
`L_QuedaEterna` and `L_Arena` (F10 for admin accounts), one native server per map and Postgres + nw-core in Docker
Desktop. Per-map servers and travel between them: [server.md](server.md#one-server-per-map).

Needs: Unreal 5.8.3 built from source (`NW_UE_SOURCE` in `local.env`), the installed 5.8.3 (`NW_UE`, cooks), Docker
Desktop running, Python, the project imported, editor and game closed. The imported project (`Content`, not in git) comes
from the import on this machine or from the asset zips of a machine that already imported everything
(`tools/assets_export.py` there, `tools/assets_import.py` here); the script checks it has the looks, the Windsward NPCs
and the new icons, and stops saying which import step is missing.

```powershell
powershell -ExecutionPolicy Bypass -File tools\windows\build_demo.ps1 -HostAddress 192.168.0.10 -Admin paulo -OpenFirewall
```

Run as administrator only for `-OpenFirewall`; once the rules exist, a normal PowerShell is enough.

| Parameter | Use |
|---|---|
| `-HostAddress` | Address the players use to reach this PC. Default: the IPv4 of the network with the default gateway. |
| `-BackendUrl` | nw-core URL for the clients (e.g. a Cloudflare Tunnel). Default: `http://<HostAddress>:8090`. |
| `-Admin a,b` | Accounts with F10. An account that does not exist yet gets an invite; run again after it is created. |
| `-Invites N` | Prints N account invites (7 days). |
| `-CookWith Source` | Cooks with the source engine instead of the installed editor. |
| `-Import` | Runs the missing extraction and import steps first (`tools/build_all.py`, `tools/ue_import_all.py`). |
| `-OpenFirewall` | Windows firewall rules: UDP 7777-7779 (servers) and TCP 8090 (nw-core). Needs administrator. |
| `-SkipData`, `-SkipCompile`, `-SkipCook`, `-SkipPackage`, `-SkipDocker`, `-SkipZip` | Skip a step. |

What it does, in order (running it again is safe; compiling and cooking are incremental):

1. Checks the engines, Docker Desktop and that the editor, client and servers are closed; with `-OpenFirewall`, the
   firewall rules.
2. With `-Import`, the missing extraction and import steps; then `python tools/make_data.py` and the check of the
   imported project.
3. Compiles `NWPort` and `NWPortServer` Win64 Development with the source engine (heavy-job queue).
4. Cooks Windows + WindowsServer with the installed editor and packages with the source engine's RunUAT into
   `deploy\build\Windows` and `deploy\build\WindowsServer`.
5. `deploy\.env`: creates `NW_DB_PASSWORD` and `NW_SERVER_SECRET` when missing (never printed) and writes
   `NW_MAP_SERVERS`. Builds the `nw-core:local` image and starts `deploy/compose.windows.yaml` (project `nw-demo`,
   port 8090).
6. Flags the `-Admin` accounts and prints the invites.
7. Writes `deploy\build\Servidores.bat`, `deploy\build\Jogar.bat`, `deploy\build\Windows\Jogar.bat` and
   `deploy\build\NWExiles-demo-client.zip`.

Play:

| Command | What it does |
|---|---|
| `deploy\build\Servidores.bat` | Three server windows: Windsward 7777, Everfall 7778, Arena 7779. `Servidores.bat arena` starts only one. Logs in `deploy\build\logs`. |
| `deploy\build\Jogar.bat` | Client with `-nwbackend`; no `-nwconnect` (nw-core returns the server address). |
| `docker compose -p nw-demo -f deploy\compose.windows.yaml exec core nw-core admin <account>` | F10 for one more account. |
| `docker compose -p nw-demo -f deploy\compose.windows.yaml exec core nw-core invite` | One more invite. |
| `docker compose -p nw-demo -f deploy\compose.windows.yaml down` | Stops nw-core; the database stays in the `nw-demo_db` volume. |

- Firewall: allow `NWPortServer.exe` on the first start (private network). Friends from outside the network need UDP
  7777-7779 and TCP 8090 forwarded to this PC, or the WARP setup of [network-test.md](network-test.md).
- The zip carries game content extracted from your install: only for testers who own the game.
- Client and servers come from the same build. After changing C++ or `Data/`, run the script again and send the new zip.
- Tests: [../tests/build.md](../tests/build.md).

## Building the tools on Windows

Prerequisites: Python, Git, Go and Visual Studio with C++ and the Windows SDK. In the **x64** Developer PowerShell:

```powershell
./tools/windows/build_ooz.ps1
./tools/windows/build_nwbt.ps1 -Test
$env:NW_GAME = 'D:\SteamLibrary\steamapps\common\New World'
$env:NW_UE = 'D:\UnrealEngine\5.8'
$env:NW_BLENDER = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
python tools/windows/doctor.py --full
```

- `build_nwbt.ps1` calls `tools/build_nwbt.py`, the same script as on Linux. It clones the private nw-buddy fork into
  `vendor/nw-buddy` if needed, exports commit `5e2c7e195f05975b73e4dbc3399d60a815133ed8` to `.nwbt/build/src` with LF
  line endings, applies `patches/nwbt.patch` and writes the version into the binary (`nwbt --version` shows
  `nwport-patch=<base>-<hash do patch>`). The fork checkout is not changed.
- `-Source` and `-GoExe` still work. `NW_PYTHON` picks the Python.
- After pulling a new `patches/nwbt.patch`, rebuild: `doctor.py` and `build_set.py` refuse an `nwbt.exe` built with
  another patch.
- Changing nwbt: [patches/README.md](../../patches/README.md).
- The Windows binaries are not in git and can be rebuilt with the scripts.

## Alternative: WSL2

To use the Linux scripts (`tools/linux/*.sh`) on Windows, work inside WSL2 (Ubuntu) and follow the Linux flow.

- Repository on the Linux filesystem (`~/nw-port`, not `/mnt/c`).
- Game at `NW_GAME="/mnt/c/Program Files (x86)/Steam/steamapps/common/New World"`.
- WSL memory in `.wslconfig` (`memory=24GB`).
- Windows Unreal reads `extract/` through `\\wsl$\Ubuntu\home\<user>\nw-port\extract`.

## Migrating a checkout from Linux or WSL

For moving the port from a Linux install (or WSL) to Windows, on the same machine or another. The code is on
GitHub. A disk carries what does not go to git: what Unreal imported and what was extracted from the game. With it,
Windows does not need to convert anything again.

To keep machines in sync after the first copy, without a disk: [asset-sync.md](asset-sync.md). It also works for the
first copy of `Content`; the disk is still useful for `extract/` and the game.

### What goes on the disk

| File | Content | Size | Needed? |
|---|---|---|---|
| `nwport-content.zip` | `unreal/NWPort/Content` and `unreal/NWPort/Data` (everything Unreal imported and the generated data) | ~15 GB | yes; without it, redo hours of import |
| `nwport-extract.zip` | `extract/` (converted assets, audio, tables, Ghidra project) | ~39 GB | only to convert something new or do reverse engineering |
| `nwport-local.zip` | `deploy/.env` (server passwords; not in git) | KB | yes, to run the server |
| `nw-core-db.dump`, `deploy-db.dump` | test accounts and characters (nw-core) | MB | optional |
| `Server.pak` | server package from an old game version (2023-12), source of the mob AI trees ([../game/creatures-ai.md](../game/creatures-ai.md)) | 9.6 MB | yes, to redo the mob AI; the current Steam build does not have it |
| `New World/` + `appmanifest_1063730.acf` | game install | ~72 GB | optional; avoids downloading again |

Left out (Windows rebuilds or does not use them): `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `.nwbt/`,
`vendor/`, `backend/target`, `deploy/build`, `reference/`.

The disk must be **NTFS** or **exFAT**. FAT32 does not accept files over 4 GB. Space: ~60 GB without the game,
~135 GB with it.

### Part 1: on Linux

#### 1. Before

1. Close Unreal and New World.
2. Everything on GitHub: `git status` in the repository must show "Your branch is up to date with 'origin/main'" and
   nothing modified. Work in progress in any area: commit and push it first.
3. Connect the disk and check the format: `lsblk -f` (FSTYPE column: `ntfs` or `exfat`). If it is `vfat` or another,
   format it as NTFS with the Disks app. **Formatting erases the whole disk.**
4. Find where it is mounted (`ls /media/$USER`) and create the folder:

```bash
HD=/media/$USER/DISK_NAME/nwport
mkdir -p "$HD"
```

#### 2. Zip

Each `zip` prints a dot per GB processed. The `extract/` one takes 30 to 60 minutes. From the repository root:

```bash
zip -r -1 -q -dg -ds 1g "$HD/nwport-content.zip" unreal/NWPort/Content unreal/NWPort/Data
zip -r -1 -q -dg -ds 1g "$HD/nwport-extract.zip" extract
zip -q "$HD/nwport-local.zip" deploy/.env
cp docs/tech/windows.md "$HD/"
cp -L <Steam folder>/ubuntu12_32/steamapps/content/app_1063730/depot_1063731/assets/Server.pak "$HD/"
```

`Server.pak` comes from Steam depot 1063731, manifest `3129454692625577733` (version from 2023-12). It is also inside
`nwport-extract.zip` (`zip` follows the link at `extract/serverpak/game/assets/Server.pak`); the loose copy is the
safeguard.

#### 3. Test database (optional)

```bash
docker start nw-core-db
docker exec nw-core-db sh -c 'pg_dump -U "$POSTGRES_USER" -Fc "$POSTGRES_DB"' > "$HD/nw-core-db.dump"
docker exec deploy-db-1 sh -c 'pg_dump -U "$POSTGRES_USER" -Fc "$POSTGRES_DB"' > "$HD/deploy-db.dump"
```

#### 4. Game (optional)

```bash
cp -r "<Steam library>/steamapps/common/New World" "$HD/"
cp <Steam library>/steamapps/appmanifest_1063730.acf "$HD/"
```

#### 5. Check and eject

```bash
cd "$HD"
unzip -tq nwport-content.zip
unzip -tq nwport-extract.zip
sha256sum $(ls | grep -E '\.(zip|dump|pak)$') > SHA256SUMS.txt
cd ~ && sync
```

Both `unzip -tq` must say "No errors detected". Then eject the disk from the file manager.

### Part 2: on Windows

#### 1. Install

- Git for Windows.
- Epic Games Launcher → Unreal Engine **5.8.3**.
- Visual Studio 2022 Community with the workloads "Desktop development with C++" and "Game development with C++"
  (brings the Windows SDK). Use the version Epic lists for 5.8.
- 7-Zip.
- Docker Desktop: only to run the server and nw-core.
- WSL2 with Ubuntu (`wsl --install -d Ubuntu`): only to convert something new through the Linux scripts (see
  [Alternative: WSL2](#alternative-wsl2); the pipeline also runs natively on Windows).

#### 2. Clone

In PowerShell. `autocrlf false` keeps the scripts with Linux line endings (WSL breaks with CRLF):

```powershell
git config --global core.autocrlf false
git clone <repository URL> C:\nw-port
```

The short folder is on purpose: the longest path in `Content` has 170 characters and Windows limits paths to 260.

#### 3. Copy from the disk and verify

Replace `E:` with the disk's drive letter.

```powershell
New-Item -ItemType Directory C:\nw-migracao
Copy-Item E:\nwport\*.zip, E:\nwport\*.dump, E:\nwport\*.pak, E:\nwport\SHA256SUMS.txt C:\nw-migracao\
Get-Content C:\nw-migracao\SHA256SUMS.txt | ForEach-Object {
    $h, $f = $_ -split '\s+', 2
    "{0}  {1}" -f ((Get-FileHash -Algorithm SHA256 "C:\nw-migracao\$f").Hash -eq $h), $f
}
```

Every line must start with `True`.

#### 4. Write the converted data into the project

```powershell
& "C:\Program Files\7-Zip\7z.exe" x C:\nw-migracao\nwport-content.zip -oC:\nw-port
& "C:\Program Files\7-Zip\7z.exe" x C:\nw-migracao\nwport-local.zip -oC:\nw-port
```

Check that you have `C:\nw-port\unreal\NWPort\Content\NW\Maps` (and not an extra folder in between).

Optional: excluding `C:\nw-port` from Windows antivirus (Windows Security → Virus & threat protection → Exclusions)
makes compiling and the first open much faster.

#### 5. First open

1. Double-click `C:\nw-port\unreal\NWPort\NWPort.uproject`. Unreal says the NWPort modules are missing and asks
   whether to build them: **Yes** (compiles the C++ for Windows; a few minutes).
2. If the build fails: right-click the `.uproject` → "Generate Visual Studio project files", open `NWPort.sln` in
   Visual Studio and build "Development Editor | Win64" to see the error.
3. On first open Unreal compiles the Windows shaders (can take over an hour). Only once.
4. Open `/Game/NW/Maps/L_Arena` and `L_SouthWindsward` and press Play.

#### 6. Game (optional)

With Steam closed, copy `E:\nwport\New World` to `<Steam library>\steamapps\common\` and `appmanifest_1063730.acf` to
`<Steam library>\steamapps\`. Open Steam: it verifies the files instead of downloading.

The build must stay the one in `tools/game.version`. If Steam updates the game, the extracted data and the
reverse-engineering addresses stop being valid until the project updates them together.

#### 7. `extract/` in WSL (only to convert something new)

The Linux pipeline runs in WSL Ubuntu, with the repository inside Linux (on `C:` it is far too slow):

```bash
git clone <repository URL> ~/nw-port
cd ~/nw-port && unzip -q /mnt/c/nw-migracao/nwport-extract.zip
```

Check `ls -la extract/serverpak/game/assets/Server.pak` (9.6 MB). If it is missing:
`mkdir -p extract/serverpak/game/assets && cp /mnt/c/nw-migracao/Server.pak extract/serverpak/game/assets/`.

To download the old version again (75 GB in total; only `Server.pak` matters): Steam console
(`steam://open/console` in the browser) → `download_depot 1063730 1063731 3129454692625577733`.

Pipeline tools: [../setup.md](../setup.md) and [tools.md](tools.md). Import from WSL into Windows Unreal has no script
yet (natively on Windows: `python tools/ue_import_all.py`).

#### 8. Test database (optional)

```powershell
cd C:\nw-port\backend
docker compose up -d
docker cp C:\nw-migracao\nw-core-db.dump nw-core-db:/tmp/db.dump
docker exec nw-core-db sh -c 'pg_restore -U "$POSTGRES_USER" -d "$POSTGRES_DB" --clean --if-exists /tmp/db.dump'
```

`deploy-db.dump` is restored the same way into the container `deploy-db-1`, after `docker compose up -d` in
`C:\nw-port\deploy`.

### Outside this procedure

This covers only the port. Before formatting the Linux install, also save what exists only on that machine: other
projects, SSH keys, tool settings and Docker databases of other projects.
