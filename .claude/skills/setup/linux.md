# Setup on Linux

Tested on Ubuntu 26.04 (x86-64). Commands run from the repository root. System scripts are in `tools/linux/`; the
Python pipeline in `tools/` is the same on Windows.

Machine: 32 GB RAM (64 recommended), GPU with 8 GB VRAM, about 150 GB free on the repository's disk (60 GB for
`extract/`, 25 GB for the project, the engine apart). First full build: 4 to 6 hours, almost all unattended.

## 1. System packages

Check: `setup_status.py` shows `git`, `python`, `numpy`, `Pillow`, `wine` without MISSING.

```bash
sudo apt install git build-essential cmake python3 python3-numpy python3-pil wine curl
```

Ask the person to run it (needs their password) or to allow you to.

## 2. Unreal Engine 5.8.3

Check: `ok Unreal 5.8.3` in the status.

The person downloads the Linux zip from https://www.unrealengine.com/linux (Epic account, accept the license there).
Then:

```bash
mkdir -p ~/UnrealEngine/5.8 && unzip -q <downloaded zip> -d ~/UnrealEngine/5.8
```

If the zip has a top folder, `NW_UE` points to the folder that contains `Engine/`. Engine elsewhere: set `NW_UE` in
`local.env`. Only the dedicated server needs the engine built from source (`docs/plano-servidor.md`); playing and
developing use the binary.

Stop here on the `import-zips` path and go to "Import zips".

## 3. Extraction tools

Check: status `tools` line without MISSING.

Already in `bin/` (Linux x86-64): `nwbt`, `dbgwater`, `libooz.so`, `texconv.exe` (runs through wine), `vgmstream-cli`,
`oggenc`. To fetch:

```bash
# Go (1.24 or newer) (tools/make_terrain.py runs "go run" for the heightmap)
curl -L https://go.dev/dl/go1.27.2.linux-amd64.tar.gz | tar -xz -C ~/.local
# Blender 5.2.2 (glb → fbx conversions)
mkdir -p ~/.local/opt && curl -L https://download.blender.org/release/Blender5.2/blender-5.2.2-linux-x64.tar.xz | tar -xJ -C ~/.local/opt
# wwiser (reads the Wwise sound banks)
git clone https://github.com/bnnm/wwiser vendor/wwiser && git -C vendor/wwiser checkout 08e937b
# nw-buddy: the extraction reads nwbt's type tables (RTTI) from it
git clone https://github.com/paulorsouza/nw-buddy.git vendor/nw-buddy
```

`tools/linux/env.sh` puts `bin/`, Go and Blender on the PATH for manual commands; the scripts find them without it.

`nwbt_patch` MISSING: `bin/nwbt` is not from this checkout's `patches/nwbt.patch`. Rebuild it:
`tools/linux/build_nwbt.sh --test` (`patches/README.md`). `libooz.so` missing: `tools/linux/build_ooz.sh`.

Full check, including a test conversion from the game's paks (~20 s):

```bash
tools/linux/check.sh
```

## 4. Extract and import

Check: status `extract done` and `import done`.

Extraction (2 to 4 h, resumable; `tools/linux/build_all.sh 07` redoes only step 07). Run it in the background and
report progress from its output:

```bash
tools/linux/build_all.sh
```

Import into Unreal, editor closed (~1 h 30, resumable the same way):

```bash
tools/linux/ue_import_all.sh
```

Memory: Ubuntu's `systemd-oomd` kills programs under memory pressure even with RAM shown as free. A step stopped for
memory: close other programs and run the same command again.

## 5. Open and check

```bash
"$NW_UE/Engine/Binaries/Linux/UnrealEditor" unreal/NWPort/NWPort.uproject /Game/NW/Maps/L_SouthWindsward
```

(`source tools/linux/local_env.sh` first if `NW_UE` is not set in the shell.) The first open compiles shaders for
several minutes. Ask the person for a screenshot after Play: character in the city, ground and props with textures.
Grey materials: `tools/linux/ue_import_all.sh 08`.

Playing with the server and accounts (only when the person asks): `tools/linux/play_local.sh`, `docs/plano-servidor.md`.

## Import zips

For the `import-zips` path, after step 2. The zips hold the imported project (`unreal/NWPort/Content` and `Data`)
made on another machine of the same person.

```bash
python3 tools/assets_import.py --check
```

- Exit 2 lists missing zips: ask the person to copy them into the folder, then run again.
- It refuses zips from another Unreal version, another game build, or a commit not in this checkout (`git pull`).

Show the summary, confirm with the person, then apply:

```bash
python3 tools/assets_import.py --yes
```

Then step 5. Converting new things later (new regions, weapons) needs steps 3 and 4 and the game installed.
