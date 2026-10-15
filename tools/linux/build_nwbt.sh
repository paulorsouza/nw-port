#!/usr/bin/env bash
# Builds bin/nwbt and bin/dbgwater on Linux through the heavy-job queue: fork base commit + patches/nwbt.patch
# (tools/build_nwbt.py, the same as on Windows; patches/README.md). Arguments go to build_nwbt.py (e.g. --test).
#
#     tools/linux/build_nwbt.sh [--test]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
source tools/linux/env.sh
exec tools/linux/heavy.sh python3 tools/build_nwbt.py "$@"
