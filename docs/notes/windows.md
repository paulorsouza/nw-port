# Windows notes

Dated history of the native Windows port and of moving data between machines. Current state and procedures:
[../tech/windows.md](../tech/windows.md) and [../tech/asset-sync.md](../tech/asset-sync.md).

## Start of the native Windows work (date not recorded)

- Branch `windows-convert`. nw-port base: `eaa1a340277ab44b0785bcde3b45865e7af61422`.
- Initial estimate: 6–11 working days, to be reviewed after extracting real assets on Windows.

## First delivery (date not recorded)

- [x] Shared C++ shim for MSVC/GCC and export of `OodleLZ_Decompress` for Windows.
- [x] Reproducible script to build `ooz.dll` x64 with MSVC.
- [x] nwbt patch: `NWBT_OODLE` and `ooz.dll` on Windows; JSON RTTI without `exec.LookPath`/`PATHEXT`.
- [x] Build of `nwbt.exe`/`dbgwater.exe` with a pinned dependency and a reapplicable patch.
- [x] Shared Python configuration; `build_set`, `make_sound_map` and `extract_audio` adapted.
- [x] Audio on Windows without symlink privileges.
- [x] Diagnostics of the Windows tools.
- [ ] Real extraction of JSON, texture, audio, mesh and animation on Windows.
- [x] Extraction orchestrator ported (validated with fixtures and `--dry-run`).
- [ ] Unreal import on Windows.

The Bash scripts stayed available for the Linux flow. `build_all.py` covered the extraction steps, including
`sistemas-01/02/03`. `ue_import_all.sh` still needed a native Windows entry point.

Validation of the first delivery with MSVC x64: `ooz.dll`, `nwbt.exe` and `dbgwater.exe` built natively. The build
script ran on a clean checkout and again with the patch already applied; both passed. `texconv.exe` ran directly on
Windows. The changed Python scripts passed compilation and `--help`; configuration was checked with `NW_GAME`, `GAME`,
a custom DLL and the Linux/Windows defaults. Native Go tests passed for JSON resolution with spaces, an explicitly
configured DLL, an uncompressed Kraken block and rejection of an invalid header. The diagnostics on that machine found
the four main binaries but not the game at the default path, nor the full audio/Blender/Unreal dependencies. No real
extraction was run.

## Second delivery: work without the assets (date not recorded)

- [x] `tools/game_version.py`: Steam manifest and SHA256 hash without `awk`/`sha256sum`; accepts `NW_GAME`, `--game`,
  `--exe` and `NW_IGNORE_VERSION`. Without a manifest it requires the executable hash.
- [x] `tools/make_data.py`: `Data/` generation sequence, including optional steps and screens; uses the Python that
  started the command, the repository directory and arguments without a shell.
- [x] `--dry-run` lists the steps without starting subprocesses or writing `Data/`.
- [x] Tests with synthetic files: wrong build/hash blocked, hash required without a manifest, exit code and explicit
  override; paths with spaces/accents, correct cwd and stop at the first generator failure.

The four tests passed on Windows with Python 3.13; the real repository sequence was also checked with `--dry-run`. The
full generator was not run without the input files.

## Third delivery: queue, supervision and extraction (date not recorded)

- [x] `nwprocess.py`: available physical memory and disk space without external Linux tools.
- [x] `heavy.py`: heavy-work lock shared between processes and inherited by children.
- [x] Windows Job Objects for the whole process tree.
- [x] Steps: completed steps skipped; a failed forced step loses its marker; `--dry-run` writes nothing.
- [x] `build_all.py`: 14 main steps and three Everfall steps.
- [x] `convert_city.py`, `convert_gatherables.py` and `render_ambient.py` replace the Bash helpers; `convert_npcs.py`
  uses native tools and supervision.
- [x] nwbt CSV manifest keeps names with spaces; truncated GLBs not reused; outputs promoted after success.
- [x] Incremental patch `nwbt-platform-path.patch` (later merged into `patches/nwbt.patch`) fixed `AppendToPATH`. The
  build applied both patches and accepted the earlier patch already installed, so a checkout from the first delivery
  could be updated.

Validation on that machine: 18 Python tests passed, including real concurrency between processes, timeout, low
memory, ending a grandchild without affecting an independent process, resume, dry-run mode without writes, terrain
regions, CSV manifest and real cleanup of a synthetic GLB. The audio pipeline was tested with synthetic
decoder/encoder programs, including failure. Four Go tests passed and the binaries were rebuilt with both patches.
Real asset extraction and Unreal import were still pending.

## 2026-10-12: copy to an external disk

A Linux → Windows migration copy was made to an external 240 GB disk (NTFS partition), folder `nwport/`:
`nwport-content.zip`, `nwport-extract.zip`, `nwport-local.zip`, `nw-core-db.dump`, `deploy-db.dump`, `Server.pak`,
`SHA256SUMS.txt` and the migration guide. The game was not copied; it is downloaded through Steam. Procedure:
[../tech/windows.md](../tech/windows.md#migrating-a-checkout-from-linux-or-wsl).

## 2026-10-15: single nwbt patch and per-system folders

Reason: `build_set.py` started calling `nwbt models files ... @list` on both platforms, but `@list` only existed in
the Windows patch. The Linux `bin/nwbt` treated `@...` as a pattern, found nothing and exited 0; Blender also exited 0
with the script failing. Conversion reported "ok" with old glb files.

- One patch: `patches/nwbt.patch` merges the former nwbt-windows, nwbt-platform-path, nwbt-pak-root-slash and
  nwbt-files-argfile patches (removed), the lighting area's `sliceLight`, `models files` exiting with an error when no
  file is found, and the patch version in the binary. Linux and Windows build through the same `tools/build_nwbt.py`.
  `build_nwbt.ps1` now calls it.
- `tools/windows/doctor.py` and `tools/linux/check.sh` flag an nwbt with another patch; `doctor.py --full` and
  `check.sh` convert a cube from the game through `@list` and fail if nothing is written. `build_set.py` and
  `convert_npcs.py` refuse an outdated nwbt, call Blender with `--python-exit-code 1` and check that the expected
  glb/fbx files were rewritten (`tools/outputs.py`).
- Linux system scripts moved to `tools/linux/` (the old `tools/<x>.sh` are temporary shortcuts); `tools/windows/`
  stays for Windows; the pipeline Python stays single in `tools/`.
- `heavy.py` runs the command in the caller's directory and returns its exit code; `python`/`python3` in the command
  become the queue's Python. `make_data.py` gained `make_ui_sounds` and `make_port_items` (they were missing compared to
  the `.sh`, which now only calls the `.py`). `nwplatform` finds Blender in `~/.local/opt` on Linux.
- Skills: `nw-windows` (with the WSL2 alternative, replacing `nw-wsl`) and `nw-linux`; area skills use commands for
  both systems and point to the platform skill for the rest.
- Pending in `ue_import_all.py` to also work on Linux: steps `sistemas-04/05` (`tools/steps/systems.ue.sh`), the
  heavy-work queue and the open-editor check on Linux.

## 2026-10-15: asset sync size

Measured: 514 zips from ~18 GB of source.
