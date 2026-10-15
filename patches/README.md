# nwbt patches

The nwbt code lives in the nw-port fork of nw-buddy (`FORK` in `tools/build_nwbt.py`), branch `nw-port`. Never push
to the upstream `giniedp/nw-buddy`.

| File | What it is |
|---|---|
| `nwbt-nwport.patch` | Reference: what the fork has over upstream `81b2302` (already in the base commit; not applied) |
| `nwbt.patch` | The nw-port patch over the fork's base commit (`BASE` in `tools/build_nwbt.py`), applied in both builds |

## Build

Linux and Windows use the same script. It exports the base commit to `.nwbt/build/src` (LF line endings), applies
`nwbt.patch` and writes the version into the binary. The fork checkout (`vendor/nw-buddy` or `NW_BUDDY`) is not
changed.

```bash
tools/linux/heavy.sh python3 tools/build_nwbt.py --test
```

```powershell
./tools/windows/build_nwbt.ps1 -Test
```

`tools/linux/heavy.sh` and `tools/linux/check.sh` are shortcuts to `tools/linux/heavy.sh` and `tools/linux/check.sh`.

`nwbt --version` shows `nwport-patch=<base>-<hash do patch>`. `tools/linux/check.sh` and `tools/windows/doctor.py` flag a
binary built with another patch (or without a version) and test conversion through `@list`.

## Change nwbt

1. `python3 tools/build_nwbt.py --prepare` sets up `.nwbt/build/src` (base + patch, its own git repository).
   Edit in `.nwbt/build/src/tools/nwbt`.
2. `python3 tools/build_nwbt.py --make-patch` writes `patches/nwbt.patch` (the header is kept). The build refuses
   to delete `.nwbt/build/src` while it has edits that have not gone into the patch.
3. Rebuild (`tools/linux/heavy.sh python3 tools/build_nwbt.py --test`) and commit `patches/nwbt.patch` together with
   `bin/nwbt`. On Windows, each person rebuilds `nwbt.exe`.

`vendor/nw-buddy` is not used to build (only as the source of the base commit). A change made there (CRLF sources)
goes into the patch by copying the files to `.nwbt/build/src` before step 2.

To change the base commit (changes moved into the fork), update `BASE` in `tools/build_nwbt.py` and redo the patch
on top of it.

## Patch parts and owners

| Part | Owner |
|---|---|
| `sliceLight` (slice lights) | lighting area |
| Everything else (Windows, `@list`, error when no file is found, version) | tools |

Areas: [docs/workflow.md](../docs/workflow.md). Windows build details: [docs/tech/windows.md](../docs/tech/windows.md).
