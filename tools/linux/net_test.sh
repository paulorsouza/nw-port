#!/usr/bin/env bash
# Local network test: nw-core + dedicated server (UnrealEditor -server) + N clients that log in through the
# clan login, take a screenshot and leave.
#   tools/linux/net_test.sh [clients=2] [map=L_SouthWindsward]
#   NW_SERVER=pacote tools/linux/net_test.sh ...   uses the packaged server (deploy/build/LinuxServer)
#   NW_SERVER=docker tools/linux/net_test.sh 10    uses the deploy/ compose already up (accounts in the container DB);
#                                            writes docker stats to Saved/NetTest/stats.txt
#   NW_TEST_SECONDS=90                       how long the clients stay on the server (bots: -nwbot)
#   NW_BOT=                                  clients without the bot (default: -nwbot, walks and attacks on its own)
#   NW_CLIENT_EXTRA="-PktLag=150 -ExecCmds=nw.DebugPrediction 1"   extra arguments for the clients
# Logs and screenshots in Saved/NetTest/. Needs the editor closed (uses the same binary). Only the first client
# renders (and takes the screenshot); the others run without rendering (-nullrhi): two clients with Nanite/Lumen do
# not fit in 8 GB of VRAM.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
root="$(cd "$(dirname "$0")/../.." && pwd)"
caller_accounts="${NW_TEST_ACCOUNTS:-}"
set -a; source "$root/backend/dev.env"; set +a
[ -n "$caller_accounts" ] && NW_TEST_ACCOUNTS="$caller_accounts"   # the caller picks the accounts
clients="${1:-2}"
map="${2:-L_SouthWindsward}"
ue="${NW_UE:-/mnt/jogos/UnrealEngine/5.8}/Engine/Binaries/Linux/UnrealEditor"
uproject="$root/unreal/NWPort/NWPort.uproject"
out="$root/unreal/NWPort/Saved/NetTest"
mkdir -p "$out"
rm -f "$out"/*.png

pids=()
# The package's NWPortServer.sh runs the binary as a child: kill the children too.
cleanup() { for p in "${pids[@]}"; do pkill -INT -P "$p" 2>/dev/null || true; kill "$p" 2>/dev/null || true; done; }
trap cleanup EXIT

seconds="${NW_TEST_SECONDS:-90}"
if [ "${NW_SERVER:-}" = docker ]; then
  invite() { docker compose -f "$root/deploy/compose.yaml" exec -T core nw-core invite 2>/dev/null | awk '{print $1}'; }
  admin() { docker compose -f "$root/deploy/compose.yaml" exec -T core nw-core admin "$1" >/dev/null 2>&1 || true; }
  [ -n "$caller_accounts" ] || NW_TEST_ACCOUNTS="${NW_TEST_ACCOUNTS_10:-teste1:Aranha teste2:Besouro teste3:Coruja teste4:Doninha teste5:Esquilo teste6:Falcao teste7:Gaviao teste8:Hiena teste9:Iguana teste10:Javali}"
else
  (cd "$root/backend" && docker compose up -d >/dev/null && cargo build -q)
  core="$root/backend/target/debug/nw-core"
  invite() { "$core" invite 2>/dev/null | awk '{print $1}'; }
  admin() { "$core" admin "$1" >/dev/null 2>&1 || true; }
  NW_BIND=127.0.0.1:8090 "$core" serve >"$out/nw-core.log" 2>&1 &
  pids+=($!)
fi
until curl -sf "$NW_BACKEND/health" >/dev/null; do sleep 0.3; done

# Test accounts and characters (idempotent).
for pair in $NW_TEST_ACCOUNTS; do
  acc="${pair%%:*}" char="${pair#*:}"
  token=$(curl -s -XPOST "$NW_BACKEND/v1/auth/login" -H content-type:application/json \
    -d "{\"name\":\"$acc\",\"password\":\"$NW_TEST_PASSWORD\"}" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("token",""))')
  if [ -z "$token" ]; then
    code=$(invite)
    token=$(curl -s -XPOST "$NW_BACKEND/v1/auth/register" -H content-type:application/json \
      -d "{\"invite\":\"$code\",\"name\":\"$acc\",\"password\":\"$NW_TEST_PASSWORD\"}" | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])')
  fi
  curl -s -XPOST "$NW_BACKEND/v1/characters" -H "authorization: Bearer $token" -H content-type:application/json \
    -d "{\"name\":\"$char\"}" | grep -q '"id"\|name_taken' || { echo "failed to create $char"; exit 1; }
  admin "$acc"   # test accounts use F10 (-nwtestdev); the server only accepts it from admins
done

server_log() { if [ "${NW_SERVER:-}" = docker ]; then docker exec deploy-game-1 cat /srv/nw/NWPort/Saved/Logs/NWPort.log; else cat "$out/server.log"; fi; }
if [ "${NW_SERVER:-}" = docker ]; then
  :
elif [ "${NW_SERVER:-}" = pacote ]; then
  "$root/deploy/build/LinuxServer/NWPortServer.sh" "/Game/NW/Maps/$map" -log -port=7777 -nwbackend="$NW_BACKEND" \
    -abslog="$out/server.log" >/dev/null 2>&1 &
else
  "$ue" "$uproject" "/Game/NW/Maps/$map" -server -log -port=7777 -nwbackend="$NW_BACKEND" -unattended -nosplash \
    -abslog="$out/server.log" >/dev/null 2>&1 &
fi
if [ "${NW_SERVER:-}" != docker ]; then
  pids+=($!)
  for _ in $(seq 300); do
    grep -q "listening on port" "$out/server.log" 2>/dev/null && break
    kill -0 "${pids[-1]}" 2>/dev/null || { echo "server died:"; tail -20 "$out/server.log"; exit 1; }
    sleep 1
  done
fi

# Samples server CPU/RAM/network every 5 s; if the machine's free RAM drops below 6 GB, shuts down the
# clients (oomd kills the graphical session under memory pressure).
(
  while sleep 5; do
    avail=$(awk '/MemAvailable/{print int($2/1048576)}' /proc/meminfo)
    if [ "${NW_SERVER:-}" = docker ]; then
      echo "$(date +%T) free=${avail}G $(docker stats --no-stream --format '{{.CPUPerc}} {{.MemUsage}} net={{.NetIO}}' deploy-game-1)"
    else
      echo "$(date +%T) free=${avail}G"
    fi
    if [ "$avail" -lt 6 ]; then echo "low RAM: shutting down clients"; pkill -INT -f "nwconnect=127.0.0.1" ; fi
  done
) > "$out/stats.txt" 2>&1 &
pids+=($!)

i=0
client_pids=()
for pair in $NW_TEST_ACCOUNTS; do
  acc="${pair%%:*}" char="${pair#*:}"
  [ "$i" -ge "$clients" ] && break
  i=$((i + 1))
  if [ "$i" -eq 1 ]; then
    view=(-windowed -ResX=1280 -ResY=720 -ExecCmds="r.SetRes 1280x720w,DisableAllScreenMessages" -nwshot="$out/$acc.png" -nwshotdelay=$((seconds - 10)))
  else
    view=(-nullrhi -nwexitafter=$seconds -ini:Engine:[ConsoleVariables]:t.MaxFPS=60)
  fi
  NW_PASSWORD="$NW_TEST_PASSWORD" NW_SERVER_SECRET= "$ue" "$uproject" /Game/NW/Maps/L_Arena -game "${view[@]}" \
    -nwbackend="$NW_BACKEND" -nwlogin="$acc" -nwchar="$char" -nwconnect=127.0.0.1:7777 ${NW_BOT--nwbot} ${NW_CLIENT_EXTRA:-} \
    -unattended -nosplash -abslog="$out/client-$acc.log" >/dev/null 2>&1 &
  client_pids+=($!)
  pids+=($!)
  sleep 3
done
for p in "${client_pids[@]}"; do wait "$p" || true; done

# No ticket: the server must refuse.
"$ue" "$uproject" 127.0.0.1:7777 -game -nullrhi -unattended -nosplash \
  -abslog="$out/client-intruso.log" >/dev/null 2>&1 &
intruder=$!
pids+=($intruder)
for _ in $(seq 60); do grep -q "nw.err.no_ticket" "$out/client-intruso.log" 2>/dev/null && break; sleep 1; done
kill "$intruder" 2>/dev/null || true
echo "--- intruder"
grep -o "nw.err.no_ticket" "$out/client-intruso.log" | head -1 || echo "was NOT refused"
sleep 2
echo "--- server"
server_log | grep -E "join|leave|refused" | tail -24
echo "--- nw-core"
[ ! -f "$out/nw-core.log" ] || grep -E "joined|ERROR|WARN" "$out/nw-core.log" | sed 's/\x1b\[[0-9;]*m//g' | tail -10
echo "--- samples"
cat "$out/stats.txt"
ls -la "$out"/*.png 2>/dev/null
