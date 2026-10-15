#!/usr/bin/env bash
# Clan network test on this PC (docs/tech/network-test.md): Postgres + nw-core + game server in Docker (project
# nw-rede) and nw-core published by the Cloudflare Tunnel at https://$NW_REDE_HOST (local.env).
#
#     tools/linux/rede.sh build     # server (source engine) + cooks Windsward + images; editor closed
#     tools/linux/rede.sh tunnel    # once, after `cloudflared tunnel login`: creates the tunnel, $NW_REDE_HOST and the
#                             # private route to this PC (friends reach the game through WARP; the ISP uses CGNAT)
#     tools/linux/rede.sh up        # starts everything and the tunnel
#     tools/linux/rede.sh down      # stops everything (the database stays in the nw-rede_db volume)
#     tools/linux/rede.sh invite    # creates an account invite (7 days)
#     tools/linux/rede.sh admin <account> [--revoke]   # grants (or removes) the F10 test commands for the account
#     tools/linux/rede.sh logs      # last lines of the game, nw-core and tunnel logs
#     tools/linux/rede.sh status
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/local_env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_ENGINE="${NW_UE_SOURCE:-}"
OUT="$ROOT/unreal/NWPort/Saved/Rede"
TUNNEL=nw-rede
HOST="${NW_REDE_HOST:?set NW_REDE_HOST (e.g. core.yourdomain.com) in local.env}"
LAN_IP="$(ip -4 route get 1.1.1.1 | grep -o 'src [0-9.]*' | cut -d' ' -f2)"
mkdir -p "$OUT"
tunnel_config() {
    # nw-core at https://$HOST and this PC's network through WARP (warp-routing).
    local id
    id="$(cloudflared tunnel list -o json | python3 -c "import json,sys; print(next(t['id'] for t in json.load(sys.stdin) if t['name']=='$TUNNEL'))")"
    cat > "$OUT/cloudflared.yml" <<CFG
tunnel: $id
credentials-file: $HOME/.cloudflared/$id.json
warp-routing:
  enabled: true
ingress:
  - hostname: $HOST
    service: http://127.0.0.1:8091
  - service: http_status:404
CFG
}
compose() { docker compose -p nw-rede --env-file "$ROOT/deploy/.env" -f "$ROOT/deploy/compose.yaml" -f "$ROOT/deploy/compose.rede.yaml" "$@"; }

case "${1:-status}" in
    build)
        [ -n "$SRC_ENGINE" ] || { echo "set NW_UE_SOURCE (engine 5.8.3 built from source) in local.env" >&2; exit 2; }
        "$ROOT/tools/linux/pesado.sh" bash -c "cd '$SRC_ENGINE' && nice Engine/Build/BatchFiles/Linux/Build.sh NWPortServer Linux Development \
            -Project='$ROOT/unreal/NWPort/NWPort.uproject' -WaitMutex -MaxParallelActions=8" | tee "$OUT/server_build.log"
        grep -q "Result: Succeeded" "$OUT/server_build.log" || { echo "server did not compile (see $OUT/server_build.log)"; exit 1; }
        "$ROOT/tools/linux/pesado.sh" "$ROOT/tools/linux/package_server.sh"
        "$ROOT/tools/linux/pesado.sh" "$ROOT/deploy/build_images.sh"
        ;;
    tunnel)
        cloudflared tunnel list -o json | grep -q "\"name\":\"$TUNNEL\"" || cloudflared tunnel create "$TUNNEL"
        cloudflared tunnel route dns "$TUNNEL" "$HOST" || true
        cloudflared tunnel route ip add "$LAN_IP/32" "$TUNNEL" || true
        tunnel_config
        echo "tunnel $TUNNEL: https://$HOST and private route $LAN_IP/32 (config in $OUT/cloudflared.yml)"
        ;;
    up)
        compose up -d
        if [ ! -f "$OUT/tunnel.pid" ] || ! kill -0 "$(cat "$OUT/tunnel.pid")" 2>/dev/null; then
            tunnel_config
            setsid nohup cloudflared tunnel --config "$OUT/cloudflared.yml" run >"$OUT/tunnel.log" 2>&1 < /dev/null &
            echo $! > "$OUT/tunnel.pid"
        fi
        for _ in $(seq 60); do curl -sf http://127.0.0.1:8091/health >/dev/null && break; sleep 1; done
        echo "nw-core local: $(curl -sf http://127.0.0.1:8091/health || echo down)"
        echo "nw-core public: $(curl -sf "https://$HOST/health" || echo 'down (the tunnel takes a few seconds)')"
        ;;
    down)
        [ -f "$OUT/tunnel.pid" ] && kill "$(cat "$OUT/tunnel.pid")" 2>/dev/null || true
        rm -f "$OUT/tunnel.pid"
        compose down
        ;;
    invite) compose exec core nw-core invite --days 7 ;;
    admin) shift; compose exec core nw-core admin "$@" ;;
    logs)
        compose logs --tail 30 core
        compose exec game tail -n 30 /srv/nw/NWPort/Saved/Logs/NWPort.log 2>/dev/null || compose logs --tail 30 game
        tail -n 15 "$OUT/tunnel.log" 2>/dev/null || true
        ;;
    status)
        compose ps
        echo "tunnel: $( [ -f "$OUT/tunnel.pid" ] && kill -0 "$(cat "$OUT/tunnel.pid")" 2>/dev/null && echo running || echo stopped)"
        ;;
    *) echo "usage: $0 build|tunnel|up|down|invite|admin|logs|status"; exit 2 ;;
esac
