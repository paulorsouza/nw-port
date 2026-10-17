#!/usr/bin/env bash
# Demo on this PC with the packaged Linux server (tools/linux/package_server.sh): nw-core with the map list, one
# dedicated server per map with the admin page, and one client at the login screen. Teleporters move the player
# between the servers. Closing the client (or Ctrl+C with --servers) shuts everything down.
#   tools/linux/demo_local.sh [--servers]
# Admin pages: Windsward http://127.0.0.1:8780, Everfall :8781, Arena :8782 (docs/tech/admin.md).
# Logs in unreal/NWPort/Saved/Demo/. Needs the editor closed (the client uses the same binary).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
root="$(cd "$(dirname "$0")/../.." && pwd)"
set -a; source "$root/backend/dev.env"; set +a
server="$root/deploy/build/LinuxServer/NWPortServer.sh"
ue="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}/Engine/Binaries/Linux/UnrealEditor"
uproject="$root/unreal/NWPort/NWPort.uproject"
out="$root/unreal/NWPort/Saved/Demo"
backend="http://127.0.0.1:8092"
maps=(L_SouthWindsward:7777 L_QuedaEterna:7778 L_Arena:7779)
[ -x "$server" ] || { echo "no packaged server: run tools/linux/package_server.sh"; exit 1; }
mkdir -p "$out"

pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done; }
trap cleanup EXIT INT TERM

# Database, nw-core binary and the editor account come from the dev setup; the demo nw-core runs on its own port
# with NW_MAP_SERVERS, so the dev one (8090) stays as it is.
"$root/tools/linux/backend_dev.sh" >/dev/null
list=""
for m in "${maps[@]}"; do list+="${m%%:*}=127.0.0.1:${m##*:},"; done
NW_BIND=127.0.0.1:8092 NW_MAP_SERVERS="${list%,}" "$root/backend/target/debug/nw-core" serve >"$out/nw-core.log" 2>&1 &
pids+=($!)
for _ in $(seq 100); do curl -sf "$backend/health" >/dev/null && break; sleep 0.3; done
curl -sf "$backend/health" >/dev/null || { echo "nw-core did not start:"; tail -20 "$out/nw-core.log"; exit 1; }

declare -A server_pid
for m in "${maps[@]}"; do
  map="${m%%:*}" port="${m##*:}"
  "$server" "/Game/NW/Maps/$map" -log -port="$port" -nwadmin -nwbackend="$backend" -abslog="$out/$map.log" >/dev/null 2>&1 &
  pids+=($!)
  server_pid[$map]=$!
done
for m in "${maps[@]}"; do
  map="${m%%:*}"
  for _ in $(seq 300); do
    grep -q "Admin page on" "$out/$map.log" 2>/dev/null && break
    kill -0 "${server_pid[$map]}" 2>/dev/null || break
    sleep 1
  done
  grep -o "Admin page on .*" "$out/$map.log" | sed "s/^/$map: /" || { echo "$map did not start:"; tail -20 "$out/$map.log"; exit 1; }
done

if [ "${1:-}" = "--servers" ]; then
  echo "servers up (nw-core $backend); Ctrl+C stops"
  wait
else
  "$ue" "$uproject" -game -windowed -ResX=1600 -ResY=900 -nwbackend="$backend" -nwconnect=127.0.0.1:7777 \
    -abslog="$out/client.log" >/dev/null 2>&1
fi
