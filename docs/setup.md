# Setup

The full procedure for Linux and Windows is the setup skill in
[`.claude/skills/nw-setup/`](../.claude/skills/nw-setup/SKILL.md). With Claude Code, ask for "setup" and it runs
it. Without it, read `SKILL.md` and then `linux.md` or `windows.md` in the same folder. Every step is a command.

## Where the machine is

Start from the repository root:

```bash
python3 tools/setup_status.py           # Windows: python tools\setup_status.py
python3 tools/setup_status.py --json    # same, for scripts
python3 tools/setup_status.py --exe     # also hashes NewWorld.exe (slower, exact build check)
```

It reads only. It reports what the machine has (Unreal, New World install and build, asset zips, tools, extraction
and import steps done) and which path applies:

| Path | Meaning |
|---|---|
| `ready` | the Unreal project is already imported: open the editor |
| `import-zips` | asset zips found in `NW_ASSETS` (`tools/assets_import.py`): import them |
| `build-from-game` | New World installed at the team build: extract and import |
| `wrong-build` | New World installed, but not the build in `tools/jogo.versao` |
| `no-source` | neither zips nor the game: nothing to build the project from |

The game's files are not in the repository. They come from your own New World install (build in
`tools/jogo.versao`), or from asset zips you made on another machine of yours (`tools/assets_export.py`,
[tech/asset-sync.md](tech/asset-sync.md)).

## local.env

Machine paths go in `local.env` at the repository root (not tracked). Copy `local.env.example`. Environment
variables win over the file. Every script reads it.

| Variable | What | Default |
|---|---|---|
| `NW_GAME` | New World install (the folder with `assets/` and `Bin64/`) | found in your Steam libraries |
| `NW_UE` | Unreal Engine 5.8.3 (the folder with `Engine/`) | common install folders |
| `NW_BLENDER` | Blender 5.2 executable | PATH or `~/.local/opt/blender-*/blender` |
| `NW_ASSETS` | folder with the asset zips | `~/nw-port-assets` |
| `NW_REDE_HOST` | public host of nw-core for the network test through Cloudflare Tunnel (`tools/linux/rede.sh`) | — |
| `NW_VGMSTREAM_CLI`, `NW_OGGENC` | audio tools on Windows | — |

## Extraction steps

`tools/linux/build_all.sh [NN]` or `python tools/build_all.py [NN]`. Each step writes `extract/.etapas/<NN>.ok`.
Finished steps are skipped. Total 2 to 4 h.

| Step | What | Time |
|---|---|---|
| 00 | checks tools and the game folder | — |
| 01 | file list of the paks (`extract/manifest/paths.txt`) | 5 min |
| 02 | game tables (JSON) and pt-BR/en-US text | 10 min |
| 03 | interface screens, fonts, Lua | 10 min |
| 04 | animation recipes (.adb) | 2 min |
| 05 | audio: music, hits, footsteps, ambience | 40 min |
| 06 | Windsward region data (heightmap, capitals, distribution, water) | 5 min |
| 07 | Windsward town | 20 min |
| 08 | region terrain (2 km) and water | 1 min |
| 09 | gatherables: 100k points and 435 meshes | 30 min |
| 10 | pre-generated region scenery (cliffs, ruins, props) | 25 min |
| 11 | characters, weapons, tools, animations | 15 min |
| 12 | `unreal/NWPort/Data/` and interface textures | 5 min |
| 13 | arena pieces | — |
| sistemas-01..03 | Everfall: town, entities and lots, NPC timelines | — |

## Import steps

`tools/linux/ue_import_all.sh [NN]` or `python tools/ue_import_all.py [NN]`, editor closed. Each step writes
`extract/.etapas/ue_<NN>.ok`. Total about 1 h 30.

| Step | What | Time |
|---|---|---|
| 01 | compiles the C++ (NWPort module) | 10 min |
| 02 | player and Lost: meshes, animations, montages, blend spaces | 5 min |
| 03 | training map `L_Arena` | 1 min |
| 04 | town (`L_SouthWindsward`) | 5 min |
| 05 | region terrain | 5 min |
| 06 | 435 gatherable meshes | 25 min |
| 07 | region scenery (35k pieces) | 5 min |
| 08 | Nanite-ready materials and instances | 2 min |
| 09 | interface textures | 2 min |
| 10 | ambient sounds | 1 min |
| 11 | map optimization (distances, shadows, lights) | — |
| 12 | creatures (boar, wolf) and their animation blueprint | — |
| sistemas-01..03 | Everfall: town, terrain and materials, NPCs | — |

Areas add their own steps in `tools/etapas/<area>.build.sh` / `.ue.sh` ([tools/etapas/README.md](../tools/etapas/README.md)).

`ABP_NW` and `ABP_Lost` (animation blueprints) come from the repository: they were built by hand in the editor.

## After the setup

- Changed a table or a data script: `tools/linux/make_data.sh` (`python3 tools/make_data.py`).
- Changed C++: compile with import step 01.
- Back up the imported project with `tools/assets_export.py` for your other machines
  ([tech/asset-sync.md](tech/asset-sync.md)). The zips hold game content: keep them for yourself.
- Tools and scripts: [tech/tools.md](tech/tools.md). How each asset leaves the game:
  [tech/asset-pipeline.md](tech/asset-pipeline.md).
