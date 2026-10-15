# Setup on Windows

Native Windows, no WSL in the pipeline. The Python pipeline in `tools/` is the same as on Linux; Windows-only
scripts are in `tools/windows/`. Use `python` instead of `python3`, PowerShell from the repository root.

Status: tools build and pass their tests on Windows; a full extraction and import with the real game has not been
accepted yet (`docs/windows-convert.md`). Report any failure with its log.

Machine: 32 GB RAM (64 recommended), GPU with 8 GB VRAM, about 150 GB free on the repository's disk. Keep the
repository at a short path (`C:\nw-port`): some generated paths are long.

## 1. Python, Git, Go, build tools

Check: `python tools\setup_status.py` shows `git` and `python` without MISSING.

```powershell
winget install Python.Python.3.13 Git.Git GoLang.Go
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
python -m pip install numpy Pillow
```

Python from python.org (winget) only: the Microsoft Store Python is refused (its child processes escape the job
that stops them on failure). Installers ask for admin approval: the person clicks it. Open a new terminal after
installing so the PATH is updated.

## 2. Unreal Engine 5.8.3

Check: `ok Unreal 5.8.3` in the status.

The person installs it from the Epic Games Launcher (Unreal Engine → Library → 5.8.3). Default folder
`C:\Program Files\Epic Games\UE_5.8` is found automatically; elsewhere, set `NW_UE` in `local.env`.

Stop here on the `import-zips` path and go to "Import zips".

## 3. Extraction tools

Check: status `tools` line without MISSING.

Build `ooz.dll`, `nwbt.exe` and `dbgwater.exe` in the **Developer PowerShell for VS (x64)**, and download
`texconv.exe`:

```powershell
git clone https://github.com/paulorsouza/nw-buddy.git vendor\nw-buddy
.\tools\windows\build_ooz.ps1
.\tools\windows\build_nwbt.ps1 -Test
Invoke-WebRequest https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe -OutFile bin\texconv.exe
git clone https://github.com/bnnm/wwiser vendor\wwiser
git -C vendor\wwiser checkout 08e937b
winget install BlenderFoundation.Blender   # 5.2.x; check the version it installs
```

Audio: download vgmstream (https://github.com/vgmstream/vgmstream/releases/tag/r2117, `vgmstream-win64.zip`) and oggenc2
(https://www.rarewares.org/ogg-oggenc.php) into `C:\tools\` keeping their DLLs next to the executables, and add to
`local.env`:

```
NW_VGMSTREAM_CLI=C:\tools\vgmstream\vgmstream-cli.exe
NW_OGGENC=C:\tools\oggenc\oggenc2.exe
NW_BLENDER=C:\Program Files\Blender Foundation\Blender 5.2\blender.exe
```

Full check, including a test conversion from the game's paks:

```powershell
python tools\windows\doctor.py --full
```

## 4. Extract and import

Check: status `extract done` and `import done`.

```powershell
python tools\build_all.py          # 2 to 4 h, resumable; "python tools\build_all.py 07" redoes step 07
python tools\ue_import_all.py      # editor closed, ~1 h 30, resumable
```

Run them in the background and report progress from the output. A step that fails leaves its log path in the
output; fix and run the same command again.

## 5. Open and check

```powershell
& "$env:NW_UE\Engine\Binaries\Win64\UnrealEditor.exe" unreal\NWPort\NWPort.uproject /Game/NW/Maps/L_SouthWindsward
```

(`NW_UE` from `local.env`, or the path the status printed.) The first open compiles shaders for several minutes. Ask
the person for a screenshot after Play: character in the city, ground and props with textures.

Not on Windows yet: `play_local`, `net_test`, `package_server`, `ui_shot` (Linux only).

## Import zips

For the `import-zips` path, after step 2. The zips hold the imported project made on another machine of the same
person.

```powershell
python tools\assets_import.py --check
```

- Exit 2 lists missing zips: ask the person to copy them into the folder, then run again.
- It refuses zips from another Unreal version, another game build, or a commit not in this checkout (`git pull`).

Show the summary, confirm with the person, then `python tools\assets_import.py --yes`, then step 5.

## WSL2 instead

To use the Linux scripts on Windows, run Claude Code inside WSL2 (Ubuntu) and follow `linux.md`. Repository in the
Linux filesystem (`~/nw-port`, not `/mnt/c`); game at `NW_GAME=/mnt/c/Program Files (x86)/Steam/steamapps/common/New
World`; WSL memory in `.wslconfig` (`memory=24GB`).
