---
name: nw-extract
description: Extracts and converts New World assets from the local install and regenerates unreal/NWPort/Data — full extraction (build_all, resumable steps), one step again, a town or settlement, gatherable meshes, a region's ambient sound, or Data/ after a generator changed. Use for "extract", "redo step NN", "convert this town", "regenerate Data".
---

# Extract, convert, generate Data/

Commands for Linux (`tools/linux/`); on Windows use the Python equivalent in the last column (`python`, not
`python3`). Run from the repository root. What each step produces: `docs/setup.md`; how each asset type is
converted: `docs/tech/asset-pipeline.md`.

| Task | Linux | Windows |
|---|---|---|
| Everything still missing | `tools/linux/build_all.sh` | `python tools/build_all.py` |
| One step again (e.g. 10) | `tools/linux/build_all.sh 10` | `python tools/build_all.py 10` |
| A town (`*_complete` dynamicslice) | `tools/linux/convert_city.sh <slice>` | `python tools/convert_city.py <slice>` |
| Gatherable meshes | `python3 tools/make_gatherables.py` then `tools/linux/convert_gatherables.sh` | `python tools/convert_gatherables.py` |
| Ambient sound of a territory | `tools/linux/render_ambient.sh <territory>` | `python tools/render_ambient.py <territory>` |
| `unreal/NWPort/Data/` | `tools/linux/make_data.sh` | `python tools/make_data.py` |
| Check the game build | `tools/linux/game_version.sh --exe` | `python tools/game_version.py --exe` |

## Before a heavy job

- New World and the Unreal editor closed; no other heavy job running. Extraction, conversions and nwbt go through the
  heavy-job queue (`tools/heavy.py`, automatic in the scripts).
- Run long jobs in the background and report progress from their output.
- A step stopped for memory (Linux `systemd-oomd`, or the script's own memory guard): run the same command again;
  finished steps are skipped (`extract/.etapas/<NN>.ok`).

## Outputs

- Steps: `extract/.etapas/`; logs in the step output.
- Town: `extract/cities/limpa/<slice>.glb`, `extract/cities/<short>_terrain.glb`, log `extract/cities/logs/<slice>.log`.
  The convert script needs 12 GB free to start and stops below 8 GB.
- Gatherables: `extract/ue/gatherables/limpa/` (batches of 8 meshes, 2 workers; stops below 10 GB free).
- Ambient: `extract/audio/ambient/<territory>/` (needs the `.txtp` files from step 05).
- Data/: seconds, nothing to close. A new screen whose textures are not PNG yet: `build_all.sh 12`, then import
  step 09 (skill `nw-unreal`).

Next: import into Unreal (skill `nw-unreal`).
