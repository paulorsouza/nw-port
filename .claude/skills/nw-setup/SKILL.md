---
name: nw-setup
description: Sets up nw-port on a machine (Linux or Windows) from a fresh clone to the editor open with the game world. Detects whether the Unreal project is already imported, whether asset zips exist, or whether New World is installed, then installs what is missing, extracts, imports and opens the editor; explains to the person what they must do themselves (accounts, downloads, sudo). Use when someone says setup, install, build the project, first run, or the project has no assets.
---

# Setup

You run the setup for the person. They do only what needs their accounts, their password or their files. Run long
steps in the background and keep them informed of each step in one short line.

The game's files never go into the repository: everything is extracted from the person's own New World install, or
comes from asset zips they made on another machine of theirs (`tools/assets_export.py`).

## 1. Find where the machine is

From the repository root:

```
python3 tools/setup_status.py        # Windows: python tools\setup_status.py
```

No Python yet: install it first (Linux: `sudo apt install python3`; Windows: `windows.md`, step 1), then run it.

The last block says the path and what is still needed:

| Path | Meaning | Go to |
|---|---|---|
| `ready` | Unreal project already imported | step 5 of the platform file (open and check) |
| `import-zips` | asset zips found in `NW_ASSETS` (default `~/nw-port-assets`) | "Import zips" in the platform file |
| `build-from-game` | New World installed at the team build | the platform file from step 1 |
| `wrong-build` | New World installed, other build | "Wrong build" below |
| `no-source` | no zips and no game | "Nothing to build from" below |

Then follow `linux.md` or `windows.md` (same folder as this file). Each step there starts with how to check it is
already done; skip what is done. Run `tools/setup_status.py` again after each step that changes the machine.

Paths the status script could not find: ask the person where the folder is and write it to `local.env` at the
repository root (copy `local.env.example`; not tracked). Every script reads it.

## 2. What only the person can do

Ask for these at the moment they are needed, one at a time, with the exact link or command:

- Steam: owning and installing New World.
- Epic Games account and the Unreal Engine 5.8.3 download (Linux: zip from unrealengine.com/linux; Windows: Epic
  Games Launcher, or the same zip).
- Their password for `sudo` (Linux) or the admin prompt (Windows installers).
- Asset zips: copying their own zips from wherever they keep them into `NW_ASSETS`.
- Disk space: freeing it, or choosing another disk.

Never enter passwords, log in to accounts or accept license prompts for them.

## Nothing to build from (`no-source`)

Tell the person, in their language, close to this:

> To build the project this machine needs one of two things:
> 1. New World installed through Steam (build `<buildid from tools/jogo.versao>`). The setup extracts everything it
>    needs from your own install; nothing from the game is downloaded from anywhere else.
> 2. Asset zips you made on another machine of yours (`python3 tools/assets_export.py` there), copied to
>    `<NW_ASSETS folder>`.
>
> The repository has only code and tools; the game's files are not in it and cannot be shared.

If New World is installed in a place the script did not look, ask for the folder and set `NW_GAME` in `local.env`.
Stop there until they have one of the two.

## Wrong build (`wrong-build`)

Extracted data and reverse-engineering answers are valid for one build only (`tools/jogo.versao`). Tell the person
which build they have (`build_errors` in the status) and which one the project uses. Do not extract from another
build unless they say the whole team is moving to it; in that case `NW_IGNORA_VERSAO=1` lets the scripts run, and
`tools/jogo.versao` is updated in a commit of its own.

## 3. After the setup

- Tell them the editor command and the first things to try (F10 opens the dev menu).
- Suggest a backup of the imported project with `tools/assets_export.py` (skill `nw-asset-sync`): it is for
  their other machines or for after formatting the disk, so they do not repeat hours of extraction and import. The
  zips hold game content: keep them for themselves.

## Rules

- One heavy job at a time (extraction, import, compiling, Blender). Do not open New World or the Unreal editor while
  extraction or import runs.
- Never write to the game folder.
- A step that fails: read its log (the script prints where), fix the cause, run the same step again. Steps are
  resumable; finished ones are skipped.
