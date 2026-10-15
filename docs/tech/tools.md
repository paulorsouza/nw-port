# Tools

Everything the project uses: third-party programs (where they come from, version, license, how to get them) and
every script in the repository. The from-scratch procedure is in [../setup.md](../setup.md); how each asset leaves
the game is in [asset-pipeline.md](asset-pipeline.md).

## Third-party programs

Built binaries go to `bin/` (not tracked). Sources, when a rebuild is needed, go to `vendor/` (not tracked, very
large). `tools/linux/get_tools.sh` downloads or builds everything that goes into `bin/` on Linux.

| Tool | What for | Origin, version, license | Binary | How to get or rebuild |
|---|---|---|---|---|
| nwbt (nw-buddy) | opens the `.pak` files; tables, text, screens, models and animations → glTF; region scenery | fork of giniedp/nw-buddy at `81b2302`, branch `nw-port`. MIT | `bin/nwbt`, `bin/dbgwater` (Windows: `.exe`) | `tools/linux/build_nwbt.sh --test` or `tools/windows/build_nwbt.ps1 -Test`: fork base commit + `patches/nwbt.patch` ([../../patches/README.md](../../patches/README.md)) |
| ooz (Kraken) | Oodle decompression on Linux (the game does not ship the DLL for it) | powzix/ooz `0503806` + `patches/ooz.patch` (`tools/build_ooz.py`). GPL-3.0 | `bin/libooz.so` (Windows: `bin/ooz.dll`) | `tools/linux/build_ooz.sh` or `tools/windows/build_ooz.ps1` |
| texconv (DirectXTex) | DDS → PNG (textures) | microsoft/DirectXTex `may2026`. MIT | `bin/texconv.exe`; on Linux called through wine by `tools/linux/texconv` (`bin/texconv`) | `tools/linux/get_tools.sh` (Windows: download the `.exe` from the release) |
| wwiser | reads the Wwise banks and writes a `.txtp` per sound event | bnnm/wwiser `08e937b` (pure Python) | — | `git clone https://github.com/bnnm/wwiser vendor/wwiser && git -C vendor/wwiser checkout 08e937b` |
| vgmstream-cli | plays `.txtp`/`.wem` (decodes audio) | vgmstream/vgmstream `r2117` (official release) | `bin/vgmstream-cli` | `tools/linux/get_tools.sh`; building from source: [asset-pipeline.md](asset-pipeline.md), Audio |
| oggenc (vorbis-tools 1.4.3) | writes `.ogg` | apt `vorbis-tools` | `oggenc` on the PATH (or `bin/oggenc`) | — |
| Blender 5.2.2 | assembles character + animations and exports FBX (`tools/export_ue.py`, `build_set.py`) | blender.org | default `~/.local/opt/blender-5.2.2-linux-x64` (or `NW_BLENDER`) | — |
| wine | runs texconv | apt | — | — |
| Go 1.24+ | builds nwbt | go.dev | default `~/.local/go` | — |
| Unreal Engine 5.8.3 | the game | Epic binary (`NW_UE`, for example `/mnt/jogos/UnrealEngine/5.8`); the dedicated server uses the engine built from source (for example `/mnt/jogos/UnrealEngine/src-5.8.3`, [server.md](server.md)) | — | — |
| Rust, Docker, Postgres 17 | backend and server | [../../backend/README.md](../../backend/README.md), `deploy/` | — | — |
| Cryengine-Converter | fallback for formats nwbt does not cover (not used today) | Markemp/Cryengine-Converter `7b951db` | — | — |
| Lumberyard (reference) | read how the base engine interprets the data | shallow clone of aws/lumberyard in `reference/` (not in git) | — | — |
| Ghidra, ghidra-mcp, JDK 21, unluac | reverse engineering | [reverse-engineering.md](reverse-engineering.md) | — | — |

`tools/linux/env.sh` puts `bin/`, Go and Blender on the PATH and points nwbt to `libooz.so`: run
`source tools/linux/env.sh` before manual commands. The Python scripts find `bin/`, `libooz.so` and Blender by
themselves (`tools/nwplatform.py`).

nwbt records its patch version: `nwbt --version` shows `nwport-patch=<base>-<hash>`.
`python3 tools/build_nwbt.py --check` compares it with the checkout's `patches/nwbt.patch`; `--test-argfile`
converts a cube from the game through `@list` and fails if nothing is written. `check.sh` (Linux) and `doctor.py`
(Windows) run both checks, and `build_set.py` and `convert_npcs.py` refuse an nwbt from another patch.

## Folders by system

- `tools/*.py`: the pipeline, one for Linux and Windows (`nwplatform.py` resolves paths and executables,
  `nwprocess.py` the queue, memory and child processes, `outputs.py` checks what nwbt and Blender wrote).
- `tools/linux/`: Linux system scripts (`.sh`, texconv wrapper, nwbt and ooz builds). The old `tools/<x>.sh` files
  are temporary shortcuts to them.
- `tools/windows/`: Windows-only scripts (`.ps1` builds, `doctor.py`, `check_migration.py`); see
  [windows.md](windows.md).
- `tools/steps/`: extraction and import steps of the areas, read by the orchestrators of both systems
  ([../../tools/steps/README.md](../../tools/steps/README.md)).
- `tools/ue/`: runs inside Unreal.
- `tools/re/`: reverse-engineering tools ([reverse-engineering.md](reverse-engineering.md)).
- `tools/tests/`: Python tests of the pipeline.

## Main scripts (day to day)

| Script | What for | Heavy? |
|---|---|---|
| `tools/linux/build_all.sh [NN]` (`python tools/build_all.py`) | extracts and converts everything from the game install, 14 resumable steps; `--dry-run` writes nothing | yes, 2 to 4 h |
| `tools/linux/ue_import_all.sh [NN]` (`python tools/ue_import_all.py`) | builds the Unreal project (editor closed), resumable steps | yes, ~1 h 30 |
| `tools/linux/make_data.sh` (= `python3 tools/make_data.py`) | regenerates `unreal/NWPort/Data/` (items, loot, gatherables, text, screens); `--dry-run` lists the steps | no |
| `tools/linux/build_nwbt.sh [--test]` | rebuilds `bin/nwbt` and `bin/dbgwater` from the fork + `patches/nwbt.patch` | yes, ~1 min |
| `tools/linux/build_ooz.sh` | rebuilds `bin/libooz.so` | no |
| `tools/linux/get_tools.sh [--force]` | puts the extraction tools in `bin/` (downloads and builds) | yes |
| `tools/linux/convert_city.sh <slice>` (`tools/convert_city.py`) | converts a town (`*_complete`) into a clean glb + terrain cut | yes |
| `tools/linux/convert_gatherables.sh [list] [output]` (`tools/convert_gatherables.py`) | converts gatherable meshes in batches (or another `.cgf` list, e.g. `tools/arena/meshes.txt`) | yes |
| `tools/linux/render_ambient.sh <territory>` (`tools/render_ambient.py`) | renders a territory's ambient sound | no |
| `tools/linux/play_local.sh [map]` | backend + dedicated server + client at the login screen (for one person to play) | yes |
| `tools/linux/package_server.sh [--skip-cook]` | packages the Linux dedicated server into `deploy/build/LinuxServer` | yes |
| `deploy/build_images.sh` | Docker images of the backend and the server | medium |
| `tools/linux/net_test.sh [n] [map]` | network test with automatic clients and a screenshot. Manual use by developers; do not automate with AI | yes |
| `tools/linux/network.sh` | clan network test on one PC: Postgres + nw-core + game server in Docker, nw-core published through Cloudflare Tunnel at `$NW_NETWORK_HOST` ([network-test.md](network-test.md)) | yes |
| `tools/linux/backend_dev.sh [status\|down]` | local test nw-core for the editor's Play; ensures the editor's account and character ([backend-testing.md](backend-testing.md)) | no |
| `tools/linux/ui_shot.sh <scenario.json> [capture]` | screenshot of a port screen without opening the game: windowless Unreal draws it to a PNG with the game code (`NWUIShotCommandlet`), from a scenario in `tools/ui_shots/` | yes |
| `tools/linux/check.sh [--cpp]` | check before merging a branch: forgotten conflicts, JSON, keys repeated across areas, Python, shell, nwbt at the current patch and the `@list` test (~20 s in the queue), game version; `--cpp` compiles (game target, works with the editor open, goes through the queue) | no |
| `tools/linux/heavy.sh <command>` (`tools/heavy.py`) | heavy-job queue: one at a time on the machine, same lock on both ([../workflow.md](../workflow.md)) | — |
| `tools/linux/game_version.sh [--exe]` (`tools/game_version.py`) | checks that the install is the team build (`tools/game.version`) | no |
| `tools/linux/env.sh`, `tools/linux/local_env.sh` | shell environment: `bin/`, Go, Blender, `NWBT_OODLE`, and the variables from `local.env` | no |
| `tools/linux/gallery.sh` (`tools/gallery.py`) | armor gallery (visual pick) | yes |
| `tools/setup_status.py [--json] [--exe]` | what the machine has and which setup path applies ([../setup.md](../setup.md)) | no |
| `tools/assets_export.py`, `tools/assets_import.py` | export the imported project to zips / import them on another machine ([asset-sync.md](asset-sync.md)); shared code in `assets_common.py` | yes |

## Extraction and data (`tools/*.py`)

| Script | What for |
|---|---|
| `extract_audio.py` | Wwise audio → `.ogg` (`extract/audio`, index in `extract/audio/INDEX.md`) |
| `item_log.py` | nw-core item log (`--character`, `--item`, `--serial`, `--since`): what came in and out, from where, why |
| `make_items.py` | item catalog (weapons, tools, potions, loot resources) and the game's loot tables |
| `make_port_items.py` | port item rules → `unreal/NWPort/Data/Items/regras_itens.json` ([../game/items.md](../game/items.md), [../game/crafting.md](../game/crafting.md)) |
| `make_crafting.py` | Everfall refining stations and the game's refining recipes → `unreal/NWPort/Data/Crafting/refining.json` |
| `make_npcs.py` | Everfall capital NPCs: position in `L_QuedaEterna`, appearance, idle animation and name |
| `convert_npcs.py` | converts the appearances of the Everfall NPCs (`.cdf` from `Data/World/queda_eterna_npcs.json`) to FBX |
| `export_npcs.py` | Blender part of `convert_npcs.py`: each NPC glb → `extract/ue/npcs/<SK_name>/<SK_name>.fbx` + `materials.json` |
| `make_gatherables.py` | Windsward gatherables: capitals + `region.distribution` → `Data/World/guardaventos_gatherables.json` |
| `make_spawns.py` | Windsward creatures (G3): spawners of the region's `_ai` layers → `Data/World/guardaventos_spawns.json` |
| `make_terrain.py` | terrain mesh (cut around the town or the whole `--region`) from the heightmap |
| `make_water.py` | water mesh (rivers, lakes, waterfalls) of a region, in the town map's frame |
| `catalog_meshes.py` | geometry hash of each mesh in the scenery glb files → `extract/ue/catalogo_malhas.json` (for `tools/ue/catalog.py`) |
| `make_abilities.py` | abilities per weapon for the HUD and Weapon Mastery |
| `ice_gauntlet_actions.py` | Ice Gauntlet actions for `make_datatables.py` ([../game/weapons/ice-gauntlet.md](../game/weapons/ice-gauntlet.md)) |
| `make_worldmap.py` | world map (M): town frame, region limits and the game's tiles (levels 2 and 3) around Windsward → `Data/World/worldmap.json` and `extract/ui/worldmap_textures.txt` |
| `make_progression.py` | character level (total XP per level, attribute points) and the effect of each attribute value → `Data/Progression/progression.json` |
| `make_datatables.py` | CSV for the damage, actions (per weapon) and camera shake DataTables |
| `make_particles.py` | a game particle effect (CryEngine library `libs/particles/*.xml`) → JSON for `UNWNiagaraTools::BuildEffect` ([particles.md](particles.md)) |
| `make_ui_sounds.py` | crafting station interface sounds → `unreal/NWPort/Data/Audio/ui_sounds.json` |
| `make_ai.py` | creature AI: tree from Server.pak + slice profile → `Data/AI/<set>.json` ([../game/creatures-ai.md](../game/creatures-ai.md)) |
| `ai_sim.py <set> Key=value...` | simulates the tree's planning for a blackboard (check without Unreal) |
| `bt_dump.py <file.behaviortree.json> [depth]` | prints a behavior tree compactly ([reverse-engineering.md](reverse-engineering.md)) |
| `sim_economy.py` | week-by-week simulator of the towns' economy ([../game/economy.md](../game/economy.md)) |
| `perf_csv.py` | summary of an Unreal CSV profile (csvprofile; `NWLoadTest -nwcsv=`): mean per cycle of each column, largest first |
| `build_nwbt.py` | builds nwbt (Linux and Windows) and checks the binary (`--check`, `--test-argfile`); `--prepare` / `--make-patch` to change the patch |
| `build_ooz.py` | ooz sources: powzix/ooz at the BASE commit + `patches/ooz.patch` |
| `outputs.py` | checks nwbt and Blender outputs (Blender with `--python-exit-code 1`; expected files rewritten) |
| `nwassets.py` | light structural check so truncated GLBs are not reused |
| `nwplatform.py`, `nwprocess.py` | shared paths and tools (loads `local.env`); supervised execution, queue and resumable steps |
| `heavy.py <command>` | heavy-job queue (see above) |
| `game_version.py` | game build check (see above) |
| `setspec.py` | merges the parts of a set (`tools/sets/player/<area>.json` for the player) for `build_set.py` and `make_datatables.py` |
| `make_rules.py` | optional: local reverse-engineering cache (`re-local/valores/`) → `Data/regras.json`, which overrides the `FNWRules` defaults |
| `re_check.py` | optional, with the local reverse-engineering cache (`re-local/`): what of it is already in the port: values applied and missing, `Data/regras.json` up to date, damagetable columns the game client reads and the port does not export |
| `weapon_report.py <weapon>` | report of what the game defines about a weapon's combat (fragments, windows, damage, stamina, cooldowns, abilities, status effects) → `extract/research/` |
| `make_sound_map.py` | links sounds to animations (footsteps, hits, voices) |
| `build_strings.py` | pt-BR/en-US text table for the C++ (`Data/UI/nw_strings.json`), from `tools/strings/<area>.json` (our text + game keys) |
| `build_set.py` | set of character + weapons + animations → FBX + manifest (`tools/sets/*.json`) |
| `export_ue.py` | Blender part of `build_set.py` (FBX in cm, animation layers, weapons on sockets) |
| `nwchar.py` | Blender character/animation utilities used by the others |
| `assemble_character.py` | assembles character + animations into a `.blend` (for checking) |
| `adb_resolve.py`, `adb_windows.py` | read the Mannequin recipes (`.adb`): clips by tags and combat windows |
| `glb_strip_nodraw.py` | removes from a glb the primitives the game does not draw (collision, NoDraw, effects) |
| `luac_dump.py <file> [filter]` | disassembles a New World `.luac` (Lua 5.1, 2-byte prefix before `\x1bLua`) |
| `ui_canvas.py` | a game LyShine screen (`.dynamicuicanvas` or a `.dynamicslice`, e.g. banners) → flat layout (base of the `ui_build_*.py`) |
| `ui_build_hud.py`, `ui_build_login.py`, `ui_build_mastery.py`, `ui_build_respawn.py`, `ui_build_group.py`, `ui_build_inventory.py`, `ui_build_tradeskills.py`, `ui_build_attributes.py` | layouts of the game's screens for the C++ (`Data/UI/*_layout*.json`) and texture lists; the HUD includes the XP bar (`hud2024/xp`); trade skills and attributes are tabs of the K window |
| `ui_build_screen.py` | a game screen from its canvas and a state file (refining, Storage, ...), scenarios in `tools/ui_screens/` |
| `ui_textures_extra.py` | texture lists of the screens drawn directly by the C++ (compass, encumbrance, cards, damage numbers, loot ticker, tooltip, level-up banner, trade skill icons, hit indicator, warning banner, navigation bar) |
| `ui_styles.py` | the game's text styles and colors (`_common/uistyle.lua`) → `extract/ui/uistyle.json` |
| `ui_compare.py` | original on top, port below, at the same size (for tuning screens with `tools/linux/ui_shot.sh`) |
| `ui_survey.py` | survey of the game's LyShine screens: structure, text and textures of each canvas (local output: `python3 tools/ui_survey.py > extract/ui/survey.md`; it holds text from the game's files, so it stays out of the repository) |
| `gallery.py` | armor gallery |
| `render_preview.py`, `preview_fbx.py` | headless previews in Blender |

Hand-written data: `tools/strings/<area>.json` (our pt/en text and the game keys used by the C++);
`tools/sets/*.json` (set recipes; the player's in parts: `tools/sets/player/<area>.json`); `tools/ui_shots/*.json`
and `tools/ui_screens/*.json` (screen capture scenarios); `tools/shops.json`, `tools/itens.json`,
`tools/economia.json`, `tools/governanca.json`, `tools/npc_rewards.json`, `tools/perks_decisoes.json`,
`tools/precos_cidade.json`, `tools/precos_referencia.json`, `tools/tutorials.json` (systems data, described in the
matching specs under [../game/](../game/)); `tools/game.version` (the team's game build); `tools/arena/meshes.txt`
(arena pieces).

## Inside Unreal (`tools/ue/*.py`)

They run with `UnrealEditor-Cmd ... -run=pythonscript`, except `remote.py` and `spawn_trace.py`, which need the
editor open.

| Script | What for |
|---|---|
| `import_nw.py <manifest>` | character, weapons, animations with windows and sounds, blend spaces, montages, DataTables |
| `import_tables.py` | imports the CSV from `tools/make_datatables.py` (`extract/ue/<set>/data`) as DataTables in `/Game/NW/Data` |
| `make_abp.py` | creates empty `ABP_NW` / `ABP_Lost` (the graph is made by hand; the finished ones are in the repository) |
| `make_test_map.py` | test arena `L_Arena` (game pieces in `/Game/NW/Arena`, shooting zones, dummies, duel, traversal) |
| `import_city.py <glb> <Name>` | a town in its own map |
| `fix_city.py <Name>` | collision, movable light, safety floor |
| `merge_city.py` | merges loose town pieces (old import, no folder) into instanced components and removes empty actors |
| `nanite_on.py <folders>` | turns Nanite on for the meshes |
| `import_terrain.py <glb> <Name> <grass> <dirt>` | terrain in the town map |
| `import_water.py` | puts the region's water (glb from `tools/make_water.py`) in `L_SouthWindsward`, with a water material |
| `fix_spawn.py <Name>` | PlayerStart at a free spot |
| `spawn_trace.py` | puts the open map's PlayerStart on a free street spot near the center (capsule sweep, editor open) |
| `import_gatherables.py <folder> [filter]` | gatherable meshes (one folder per mesh, glTF pipeline) |
| `import_region_scene.py <glb> <Name>` | region scenery (world → map frame) |
| `merge_instances.py` | merges the actors of each `Regiao/<layer>` folder of `L_SouthWindsward` into instanced components |
| `optimize_region.py` | makes `L_SouthWindsward` lighter without moving anything |
| `catalog.py [--dry-run]` | single catalog: textures, materials and meshes repeated across layers, towns and gatherables become one original in `/Game/NW/Catalogo`; run after each scenery import |
| `fix_nanite_materials.py <folders>` | Nanite flags and instances; copies the Interchange parents into the project |
| `cap_textures.py <folder> <max\|0>` | caps (or releases, with 0) texture size |
| `import_ui.py` | screen textures and fonts |
| `import_ui_sounds.py` | crafting station interface sounds (`extract/audio/ui/crafting`) as SoundWave |
| `import_audio.py` | looping music and ambience |
| `import_npcs.py` | NPC appearances (`tools/convert_npcs.py`) and the NPC field in the town map |
| `import_fx.py` | a game particle effect → Niagara system ([particles.md](particles.md)) |
| `fx_notifies.py` | Ice Gauntlet hand and body effects on the `AM_Ig_*` montages ("Timed Niagara Effect" notify state on the procclip's bone) |
| `make_fx.py` | materials and simple combat effect tweaks |
| `city_exposure.py` | auto exposure in a town map: unbounded post-process volume, only in that map |
| `remote.py` | runs Python in the open Unreal editor (remote execution of the Python Editor Script plugin) |

`tools/ue/grip_offsets.md` holds weapon grip offsets (weapons area).

## Reverse engineering (`tools/re/`)

`xref.py`, `dis.py`, `dec.sh`, `fn.py`, `RangeDecompile.java`, `find_vcalls.py`, `cage_inventory.py`,
`cage_classes.py`, `columns.py`, `luac_batch.sh`, `lua_summary.py`: see
[reverse-engineering.md](reverse-engineering.md).

## Windows (`tools/windows/`)

| Script | What for |
|---|---|
| `build_nwbt.ps1 [-Test]` | builds `nwbt.exe` and `dbgwater.exe` (Developer PowerShell for VS x64) |
| `build_ooz.ps1` | builds `ooz.dll` |
| `doctor.py [--full]` | initial diagnosis without extracting assets or starting Unreal; `--full` includes a test conversion |
| `check_migration.py [--apply]` | checks WSL caches on Windows; `--apply` fixes references with a backup |

## Tests (`tools/tests/`)

`test_extraction.py`, `test_migration.py`, `test_nwprocess.py`, `test_ooz_dll.py` (smoke test of the built DLL),
`test_windows_pipeline.py` (contract of the native commands with synthetic fixtures, no game assets).

## Claude Code skills

`.claude/skills/` has skills for the main scripts: when to use them, preconditions and what to check at the end.
They are for people using Claude Code on the project; the scripts work without AI.
