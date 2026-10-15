# Setup

The full procedure, for Linux and Windows, is in [`.claude/skills/setup/`](../.claude/skills/setup/SKILL.md). With
Claude Code, ask for "setup" and it runs it; without it, read `SKILL.md` and then `linux.md` or `windows.md`, every
step is a command.

Start with:

```bash
python3 tools/setup_status.py
```

It says what the machine has (Unreal, New World install and build, asset zips, tools, extraction and import steps
done) and which path applies.

The game's files are not in the repository. They come from your own New World install (build in
`tools/jogo.versao`), or from asset zips you made on another machine of yours (`tools/assets_export.py`).
Machine paths go in `local.env` (model in `local.env.example`).

## Extraction steps

`tools/linux/build_all.sh [NN]` or `python tools/build_all.py [NN]`. Each step writes `extract/.etapas/<NN>.ok`;
finished steps are skipped.

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
`extract/.etapas/ue_<NN>.ok`.

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

`ABP_NW` and `ABP_Lost` (animation blueprints) come from the repository: they were built by hand in the editor.
