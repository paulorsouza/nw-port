#!/usr/bin/env bash
# Packages the dedicated server into deploy/build/LinuxServer.
#   tools/linux/package_server.sh [--skip-cook]
# 1. Cooks the content for LinuxServer with the installed editor (same CompatibleChangelist as the source engine).
# 2. Stages the folder (NWPortServer binary already built by the source engine + .pak) with the source engine's UAT.
# The result contains game assets extracted from your install: keep it on your own machines only.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
# UAT's MSBuild leaves nodes alive for reuse, and they inherit the heavy-job queue lock
# (tools/linux/heavy.sh): the queue stayed stuck after packaging.
export MSBUILDDISABLENODEREUSE=1 DOTNET_CLI_USE_MSBUILD_SERVER=0
root="$(cd "$(dirname "$0")/../.." && pwd)"
uproject="$root/unreal/NWPort/NWPort.uproject"
installed="${NW_UE:?set NW_UE in local.env}"
source_engine="${NW_UE_SOURCE:?set NW_UE_SOURCE (engine 5.8.3 built from source) in local.env}"
out="$root/deploy/build"
# Cooked maps (NW_SERVER_MAPS replaces the list).
maps="${NW_SERVER_MAPS:-/Game/NW/Maps/L_SouthWindsward+/Game/NW/Maps/L_QuedaEterna+/Game/NW/Maps/L_Arena}"

if [ "${1:-}" != "--skip-cook" ]; then
  "$installed/Engine/Binaries/Linux/UnrealEditor-Cmd" "$uproject" -run=cook -targetplatform=LinuxServer -map="$maps" \
    -unattended -nopause -nosplash -abslog="$root/unreal/NWPort/Saved/Logs/cook-server.log"
fi

"$source_engine/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$uproject" -platform=Linux \
  -server -serverplatform=Linux -noclient -skipbuild -skipcook -stage -pak -archive -archivedirectory="$out" \
  -unattended -nop4 -utf8output
ls -la "$out/LinuxServer"
