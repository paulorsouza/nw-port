---
name: nw-linux
description: Linux platform commands for nw-port — machine paths (local.env), the heavy-job queue, PATH for manual tool calls, building nwbt/ooz and fetching tools into bin/, the pre-merge check, and Linux pitfalls. Use when working on Linux and a command, path or tool is needed.
---

# Linux

System scripts in `tools/linux/`; the Python pipeline in `tools/` is shared with Windows. Run from the repository
root. First-time setup: skill `nw-setup`.

| Task | Command |
|---|---|
| What this machine has | `python3 tools/setup_status.py` |
| Machine paths (`NW_GAME`, `NW_UE`, `NW_BLENDER`, `NW_ASSETS`, `NW_REDE_HOST`) | `local.env` (model in `local.env.example`) |
| Export them to a shell | `source tools/linux/local_env.sh` |
| `bin/`, Go and Blender on the PATH for manual calls | `source tools/linux/env.sh` |
| Heavy-job queue (one at a time) | `python3 tools/pesado.py <command>` or `tools/linux/pesado.sh <command>` |
| Fetch or rebuild everything in `bin/` | `tools/linux/get_tools.sh [--force]` |
| Rebuild nwbt and dbgwater only | `tools/linux/build_nwbt.sh --test` |
| Rebuild libooz.so only | `tools/linux/build_ooz.sh` |
| Pre-merge check | `tools/linux/check.sh [--cpp]` |

Extraction and data: skill `nw-extract`. Unreal: skill `nw-unreal`. Running the game: skill `nw-run`.

## Tools in bin/

Not tracked. `nwbt` and `dbgwater` come from `paulorsouza/nw-buddy` (branch `nw-port`) plus `patches/nwbt.patch`
(`tools/build_nwbt.py`, same on Windows); `libooz.so` from `powzix/ooz` plus `patches/ooz.patch`
(`tools/build_ooz.py`); `texconv.exe` (through wine, `bin/texconv` → `tools/linux/texconv`) and `vgmstream-cli`
from their releases. `oggenc` from the `vorbis-tools` package. After pulling a new `patches/nwbt.patch`, rebuild:
`check.sh`, `build_set.py` and `convert_npcs.py` refuse an nwbt built from another patch. Changing nwbt:
`patches/README.md`.

## Pitfalls

- One heavy job at a time: Ubuntu's `systemd-oomd` kills programs under memory pressure even with RAM shown as free.
- Never put the nwbt cache in `/tmp` (it is RAM).
- `pgrep -f` / `pkill -f` match the shell's own command line.
