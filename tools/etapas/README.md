# Area steps

Each area ([docs/workflow.md](../../docs/workflow.md)) adds its own steps here, without touching the numbered list of
the main scripts:

- `<area>.build.sh`: extraction and conversion, read at the end of `tools/linux/build_all.sh`.
- `<area>.ue.sh`: Unreal import, read at the end of `tools/linux/ue_import_all.sh`.

Inside the file, one line per step: `step <area>-NN "description" command [args]`. In `ue_import_all.sh`,
`py <log> <script> <args>` runs a script inside Unreal.

Run a single step: `tools/linux/ue_import_all.sh montarias-01`.

Helper functions carry the area prefix (`montarias_cavalo() { ... }`) so they do not collide with other areas'
functions.

The Windows orchestrator (`tools/build_all.py`) includes these steps and stops with an explicit error on an area step
it does not know ([docs/tech/windows.md](../../docs/tech/windows.md)).
