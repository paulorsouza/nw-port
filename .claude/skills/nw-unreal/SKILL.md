---
name: nw-unreal
description: How Claude works with the Unreal project — import steps (ue_import_all, commandlets), compiling C++ with or without the editor open, opening the editor for the person, Python scripts inside Unreal (tools/ue), and capturing screens without the game (ui_shot + ui_compare). Use for "import", "redo import step NN", "compile", "open the editor", "capture the screen".
---

# Unreal

Engine 5.8.3 at `NW_UE` (`local.env`; `python3 tools/setup_status.py` prints it). Project
`unreal/NWPort/NWPort.uproject`. Linux commands below; Windows equivalents in the last column.

| Task | Linux | Windows |
|---|---|---|
| Import everything missing (editor closed) | `tools/linux/ue_import_all.sh` | `python tools/ue_import_all.py` |
| One import step again (e.g. 08, materials) | `tools/linux/ue_import_all.sh 08` | `python tools/ue_import_all.py 08` |
| Compile C++ with the editor closed | `tools/linux/ue_import_all.sh 01` | `python tools/ue_import_all.py 01` |
| Compile the game target with the editor open | `tools/linux/check.sh --cpp` | — |
| Capture a screen without the game | `tools/linux/ui_shot.sh tools/ui_shots/<screen>.json [capture]` | — |
| Original above, port below | `python3 tools/ui_compare.py <game shot> <port capture> <out.png> [--crop x0,y0,x1,y1]` | same with `python` |

## Rules

- Import steps and commandlets write the same assets as the editor: ask the person to close the editor and wait for
  the confirmation.
- Each import step checks "Python script executed successfully" in its log (`unreal/NWPort/Saved/ImportLogs/`).
  Commandlets return an error code for glTF material warnings; that is expected.
- Never run the game, PIE or automated game tests on your own. Open the editor for the person and ask for a
  screenshot of what must be checked. Screen captures with `ui_shot` (no window) are fine.
- Grey materials: import step 08. Known Unreal pitfalls: `docs/status.md`.

## Open the editor

```bash
"$NW_UE/Engine/Binaries/Linux/UnrealEditor" unreal/NWPort/NWPort.uproject /Game/NW/Maps/L_SouthWindsward &
```

Windows: `& "$env:NW_UE\Engine\Binaries\Win64\UnrealEditor.exe" unreal\NWPort\NWPort.uproject /Game/NW/Maps/L_SouthWindsward`.
`source tools/linux/local_env.sh` first if `NW_UE` is not in the shell. Maps: `L_SouthWindsward` (Windsward),
`L_QuedaEterna` (Everfall), `L_Arena` (training).

## Python inside Unreal

Scripts in `tools/ue/` run headless through the editor:

```bash
"$NW_UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PWD/unreal/NWPort/NWPort.uproject" -run=pythonscript -script="$PWD/tools/ue/<script>.py <args>" -unattended -nosplash -nullrhi
```

Use absolute paths in `-script`. The list of scripts and what each does: `docs/tech/tools.md`.

## Screen captures

`ui_shot` renders a screen with the game's code (`NWUIShotCommandlet`) from a scenario in `tools/ui_shots/<screen>.json`
(bag, levels, station, recipe, window size) into `extract/ui/shots/<screen>_<capture>.png`. It needs the module
compiled (import step 01) and runs in the heavy-job queue. Compare with a screenshot of the original game before
asking the person for one. UI work cycle: `docs/game/ui/README.md`.
