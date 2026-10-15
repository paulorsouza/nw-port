---
name: nw-asset-sync
description: Backs up the imported Unreal project (Content and Data) as zips with tools/assets_export.py, or restores it on another machine of the same person with tools/assets_import.py. For someone with several machines or about to reinstall the disk, so hours of extraction and import are not repeated. Use for "back up the assets", "export the assets", "import the zips", "restore the project".
---

# Asset backup between machines

The zips hold game content: they are for the person's own machines, never for sharing. Details: `docs/tech/asset-sync.md`.

Folder: `NW_ASSETS` in `local.env` (default `~/nw-port-assets`): `manifest.json` + `bundles/*.zip`. The person
moves files between machines with whatever they use (a synced folder, an external disk). Commands are the same on
Windows with `python`.

## Export (machine that imported)

Before: editor closed; code committed and pushed; the newest `manifest.json` from the shared folder copied into
`NW_ASSETS`.

```bash
python3 tools/assets_export.py
```

Tell the person the "upload" and "can delete" lists from the output: they copy the new zips, then `manifest.json`
last, and delete the listed old zips. Uncommitted changes in `Source/`, `Config/` or `tools/`: ask before using
`--allow-dirty`.

## Import (other machine)

Before: editor closed; `git pull`; the newest `manifest.json` in `NW_ASSETS`.

```bash
python3 tools/assets_import.py --check
```

Exit 2 lists missing zips: ask the person to copy them, run again. Show the summary, confirm, then:

```bash
python3 tools/assets_import.py --yes
```

Never `--force` unless the person asks. After the import, open the editor for them: Unreal compiles shaders for
what changed.
