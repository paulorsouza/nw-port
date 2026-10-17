#!/usr/bin/env bash
# Plays locally: nw-core (Postgres from the dev compose) + dedicated server (UnrealEditor -server) + one client at
# the login screen. Closing the client shuts everything down.
#   tools/linux/play_local.sh [map=L_SouthWindsward]
# Logs in unreal/NWPort/Saved/Play/. Needs the editor closed (uses the same binary).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
root="$(cd "$(dirname "$0")/../.." && pwd)"
set -a; source "$root/backend/dev.env"; set +a
map="${1:-L_SouthWindsward}"
ue="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}/Engine/Binaries/Linux/UnrealEditor"
uproject="$root/unreal/NWPort/NWPort.uproject"
out="$root/unreal/NWPort/Saved/Play"
mkdir -p "$out"

pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done; }
trap cleanup EXIT

(cd "$root/backend" && docker compose up -d >/dev/null && cargo build -q)
NW_BIND=127.0.0.1:8090 "$root/backend/target/debug/nw-core" serve >"$out/nw-core.log" 2>&1 &
pids+=($!)
until curl -sf "$NW_BACKEND/health" >/dev/null; do sleep 0.3; done

"$ue" "$uproject" "/Game/NW/Maps/$map" -server -log -port=7777 -nwadmin -nwbackend="$NW_BACKEND" -unattended -nosplash \
  -abslog="$out/server.log" >/dev/null 2>&1 &
pids+=($!)
for _ in $(seq 300); do
  grep -q "listening on port" "$out/server.log" 2>/dev/null && break
  kill -0 "${pids[-1]}" 2>/dev/null || { echo "server died:"; tail -20 "$out/server.log"; exit 1; }
  sleep 1
done
echo "server up; admin page on http://127.0.0.1:8780/; opening the client"
"$ue" "$uproject" -game -windowed -ResX=1600 -ResY=900 -nwbackend="$NW_BACKEND" -nwconnect=127.0.0.1:7777 \
  -abslog="$out/client.log" >/dev/null 2>&1
