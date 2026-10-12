#!/usr/bin/env bash
# Teste em rede local: nw-core + servidor dedicado (UnrealEditor -server) + N clientes que entram pelo login
# do clã, tiram print e saem.
#   tools/net_test.sh [clientes=2] [mapa=L_SouthWindsward]
#   NW_SERVER=pacote tools/net_test.sh ...   usa o servidor empacotado (deploy/build/LinuxServer)
#   NW_SERVER=docker tools/net_test.sh 10    usa o compose de deploy/ já no ar (contas no banco do container);
#                                            grava docker stats em Saved/NetTest/stats.txt
#   NW_TEST_SECONDS=90                       quanto tempo os clientes ficam no servidor (bots: -nwbot)
#   NW_BOT=                                  clientes sem o bot (padrão: -nwbot, anda e ataca sozinho)
#   NW_CLIENT_EXTRA="-PktLag=150 -ExecCmds=nw.DebugPrediction 1"   argumentos a mais para os clientes
# Logs e prints em Saved/NetTest/. Precisa do editor fechado (usa o mesmo binário). Só o primeiro cliente
# renderiza (e tira print); os outros rodam sem render (-nullrhi): dois clientes com Nanite/Lumen não cabem
# nos 8 GB de VRAM.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
caller_accounts="${NW_TEST_ACCOUNTS:-}"
set -a; source "$root/backend/dev.env"; set +a
[ -n "$caller_accounts" ] && NW_TEST_ACCOUNTS="$caller_accounts"   # quem chamou escolhe as contas
clients="${1:-2}"
map="${2:-L_SouthWindsward}"
ue=/mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor
uproject="$root/unreal/NWPort/NWPort.uproject"
out="$root/unreal/NWPort/Saved/NetTest"
mkdir -p "$out"
rm -f "$out"/*.png

pids=()
# O NWPortServer.sh do pacote chama o binário como filho: mata os filhos também.
cleanup() { for p in "${pids[@]}"; do pkill -INT -P "$p" 2>/dev/null || true; kill "$p" 2>/dev/null || true; done; }
trap cleanup EXIT

seconds="${NW_TEST_SECONDS:-90}"
if [ "${NW_SERVER:-}" = docker ]; then
  invite() { docker compose -f "$root/deploy/compose.yaml" exec -T core nw-core invite 2>/dev/null | awk '{print $1}'; }
  [ -n "$caller_accounts" ] || NW_TEST_ACCOUNTS="${NW_TEST_ACCOUNTS_10:-teste1:Aranha teste2:Besouro teste3:Coruja teste4:Doninha teste5:Esquilo teste6:Falcao teste7:Gaviao teste8:Hiena teste9:Iguana teste10:Javali}"
else
  (cd "$root/backend" && docker compose up -d >/dev/null && cargo build -q)
  core="$root/backend/target/debug/nw-core"
  invite() { "$core" invite 2>/dev/null | awk '{print $1}'; }
  NW_BIND=127.0.0.1:8090 "$core" serve >"$out/nw-core.log" 2>&1 &
  pids+=($!)
fi
until curl -sf "$NW_BACKEND/health" >/dev/null; do sleep 0.3; done

# Contas e personagens de teste (idempotente).
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
    -d "{\"name\":\"$char\"}" | grep -q '"id"\|name_taken' || { echo "falhou criar $char"; exit 1; }
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
    kill -0 "${pids[-1]}" 2>/dev/null || { echo "servidor caiu:"; tail -20 "$out/server.log"; exit 1; }
    sleep 1
  done
fi

# Amostra CPU/RAM/rede do servidor a cada 5 s; se a RAM livre da máquina cair abaixo de 6 GB, encerra os
# clientes (o oomd derruba a sessão gráfica por pressão de memória).
(
  while sleep 5; do
    avail=$(awk '/MemAvailable/{print int($2/1048576)}' /proc/meminfo)
    if [ "${NW_SERVER:-}" = docker ]; then
      echo "$(date +%T) livre=${avail}G $(docker stats --no-stream --format '{{.CPUPerc}} {{.MemUsage}} net={{.NetIO}}' deploy-game-1)"
    else
      echo "$(date +%T) livre=${avail}G"
    fi
    if [ "$avail" -lt 6 ]; then echo "RAM baixa: encerrando clientes"; pkill -INT -f "nwconnect=127.0.0.1" ; fi
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

# Sem passe: o servidor tem que recusar.
"$ue" "$uproject" 127.0.0.1:7777 -game -nullrhi -unattended -nosplash \
  -abslog="$out/client-intruso.log" >/dev/null 2>&1 &
intruder=$!
pids+=($intruder)
for _ in $(seq 60); do grep -q "nw.err.no_ticket" "$out/client-intruso.log" 2>/dev/null && break; sleep 1; done
kill "$intruder" 2>/dev/null || true
echo "--- intruso"
grep -o "nw.err.no_ticket" "$out/client-intruso.log" | head -1 || echo "NÃO foi recusado"
sleep 2
echo "--- servidor"
server_log | grep -E "entrada|saída|recusada" | tail -24
echo "--- nw-core"
[ ! -f "$out/nw-core.log" ] || grep -E "entrou|ERROR|WARN" "$out/nw-core.log" | sed 's/\x1b\[[0-9;]*m//g' | tail -10
echo "--- amostras"
cat "$out/stats.txt"
ls -la "$out"/*.png 2>/dev/null
