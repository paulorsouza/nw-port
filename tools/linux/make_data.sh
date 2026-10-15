#!/usr/bin/env bash
# Regenerates unreal/NWPort/Data/ (catalog, loot, gatherables, texts, screens) from what was already extracted to
# extract/ (docs/setup.md, step 2). Light: Python only, no Unreal. Safe to run again.
# The list of steps lives only in tools/make_data.py (the same on Windows); a new step goes there.
#
#     tools/linux/make_data.sh [--dry-run]
set -euo pipefail
cd "$(dirname "$0")/../.."
exec python3 tools/make_data.py "$@"
