---
name: nw-windows
description: Native Windows commands for nw-port — machine paths (local.env), diagnostics, the heavy-job queue, extraction, import, Data/, building nwbt.exe/ooz.dll, migrating a checkout from Linux or WSL, and the WSL2 alternative. Use when Claude Code runs on Windows and a command, path or tool is needed.
---

# Windows

No WSL in the pipeline: `tools/*.py` is shared with Linux; Windows-only scripts are in `tools/windows/`. Use the
python.org Python (the Microsoft Store one is refused) and `python` instead of `python3`. State of the port:
`docs/tech/windows.md`. First-time setup: skill `nw-setup`.

| Task | Command |
|---|---|
| What this machine has | `python tools\setup_status.py` |
| Diagnostics (`--full`: audio, Blender, Unreal, nwbt `@list` test) | `python tools\windows\doctor.py --full` |
| Heavy-job queue | `python tools\pesado.py <command>` |
| Extraction, all or step NN | `python tools\build_all.py [NN]` (`--dry-run` lists) |
| Import into Unreal, all or step NN (editor closed) | `python tools\ue_import_all.py [NN]` |
| Data/ | `python tools\make_data.py` |
| Town, gatherables, ambient sound | `python tools\convert_city.py`, `convert_gatherables.py`, `render_ambient.py` |
| Game build | `python tools\jogo_versao.py --exe` |
| Checkout copied from WSL | `python tools\windows\check_migration.py [--apply]` |
| Script tests | `python -m unittest discover -s tools/tests` |

Machine paths in `local.env`: `NW_GAME`, `NW_UE`, `NW_BLENDER`, `NW_VGMSTREAM_CLI`, `NW_OGGENC`, `NW_ASSETS`;
`NW_BUDDY` if the nw-buddy checkout is not in `vendor\nw-buddy`.

## nwbt and ooz

In the Developer PowerShell for VS (x64), with Go, Git and the C++ workload:

```powershell
.\tools\windows\build_ooz.ps1
.\tools\windows\build_nwbt.ps1 -Test
```

Same `tools/build_nwbt.py` and `tools/build_ooz.py` as Linux. After pulling a new `patches/nwbt.patch`, rebuild:
`doctor.py` and `build_set.py` refuse an `nwbt.exe` built from another patch.

Not on Windows yet: `backend_dev`, `ui_shot`, `check` (use `doctor.py` and the unit tests), `package_server`,
`rede`, `net_test`, `play_local`.

## WSL2 instead

Run Claude Code inside WSL2 (Ubuntu) and use skill `nw-linux`. Repository in the Linux filesystem (`~/nw-port`, not
`/mnt/c`); `NW_GAME=/mnt/c/Program Files (x86)/Steam/steamapps/common/New World`; WSL memory in `.wslconfig`
(`memory=24GB`). Windows Unreal reads `extract/` through `\\wsl$\Ubuntu\home\<user>\nw-port\extract`.
