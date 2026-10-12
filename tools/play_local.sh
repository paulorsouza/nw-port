#!/usr/bin/env bash
# Joga local: nw-core (Postgres do compose de dev) + servidor dedicado (UnrealEditor -server) + um cliente na
# tela de entrada. Fechar o cliente encerra tudo.
#   tools/play_local.sh [mapa=L_SouthWindsward]
# Logs em unreal/NWPort/Saved/Play/. Precisa do editor fechado (usa o mesmo binário).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
set -a; source "$root/backend/dev.env"; set +a
map="${1:-L_SouthWindsward}"
ue=/mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor
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

"$ue" "$uproject" "/Game/NW/Maps/$map" -server -log -port=7777 -nwbackend="$NW_BACKEND" -unattended -nosplash \
  -abslog="$out/server.log" >/dev/null 2>&1 &
pids+=($!)
for _ in $(seq 300); do
  grep -q "listening on port" "$out/server.log" 2>/dev/null && break
  kill -0 "${pids[-1]}" 2>/dev/null || { echo "servidor caiu:"; tail -20 "$out/server.log"; exit 1; }
  sleep 1
done
echo "servidor no ar; abrindo o cliente"
"$ue" "$uproject" -game -windowed -ResX=1600 -ResY=900 -nwbackend="$NW_BACKEND" -nwconnect=127.0.0.1:7777 \
  -abslog="$out/client.log" >/dev/null 2>&1
