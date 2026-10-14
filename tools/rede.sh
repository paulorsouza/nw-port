#!/usr/bin/env bash
# Teste em rede do clã neste PC (docs/rede-teste.md): Postgres + nw-core + servidor de jogo em Docker (projeto
# nw-rede) e o nw-core publicado pelo Cloudflare Tunnel em https://<NW_REDE_HOST>.
#
#     tools/rede.sh build     # servidor (engine do fonte) + cozinha Guardaventos + imagens; editor fechado
#     tools/rede.sh tunnel    # uma vez, depois do `cloudflared tunnel login`: cria o túnel, <NW_REDE_HOST> e a rota
#                             # privada para este PC (os amigos chegam no jogo pelo WARP; o provedor usa CGNAT)
#     tools/rede.sh up        # sobe tudo e o túnel
#     tools/rede.sh down      # para tudo (o banco fica no volume nw-rede_db)
#     tools/rede.sh invite    # gera um convite de conta (7 dias)
#     tools/rede.sh logs      # últimas linhas do jogo, do nw-core e do túnel
#     tools/rede.sh status
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_ENGINE=/mnt/jogos/UnrealEngine/src-5.8.3
OUT="$ROOT/unreal/NWPort/Saved/Rede"
TUNNEL=nw-rede
HOST=<NW_REDE_HOST>
LAN_IP="$(ip -4 route get 1.1.1.1 | grep -o 'src [0-9.]*' | cut -d' ' -f2)"
mkdir -p "$OUT"
tunnel_config() {
    # nw-core em https://<NW_REDE_HOST> e a rede deste PC pelo WARP (warp-routing).
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
        "$ROOT/tools/pesado.sh" bash -c "cd '$SRC_ENGINE' && nice Engine/Build/BatchFiles/Linux/Build.sh NWPortServer Linux Development \
            -Project='$ROOT/unreal/NWPort/NWPort.uproject' -WaitMutex -MaxParallelActions=8" | tee "$OUT/server_build.log"
        grep -q "Result: Succeeded" "$OUT/server_build.log" || { echo "servidor não compilou (ver $OUT/server_build.log)"; exit 1; }
        "$ROOT/tools/pesado.sh" "$ROOT/tools/package_server.sh"
        "$ROOT/tools/pesado.sh" "$ROOT/deploy/build_images.sh"
        ;;
    tunnel)
        cloudflared tunnel list -o json | grep -q "\"name\":\"$TUNNEL\"" || cloudflared tunnel create "$TUNNEL"
        cloudflared tunnel route dns "$TUNNEL" "$HOST" || true
        cloudflared tunnel route ip add "$LAN_IP/32" "$TUNNEL" || true
        tunnel_config
        echo "túnel $TUNNEL: https://$HOST e rota privada $LAN_IP/32 (config em $OUT/cloudflared.yml)"
        ;;
    up)
        compose up -d
        if [ ! -f "$OUT/tunnel.pid" ] || ! kill -0 "$(cat "$OUT/tunnel.pid")" 2>/dev/null; then
            tunnel_config
            setsid nohup cloudflared tunnel --config "$OUT/cloudflared.yml" run >"$OUT/tunnel.log" 2>&1 < /dev/null &
            echo $! > "$OUT/tunnel.pid"
        fi
        for _ in $(seq 60); do curl -sf http://127.0.0.1:8091/health >/dev/null && break; sleep 1; done
        echo "nw-core local: $(curl -sf http://127.0.0.1:8091/health || echo fora)"
        echo "nw-core público: $(curl -sf https://<NW_REDE_HOST>/health || echo 'fora (o túnel leva alguns segundos)')"
        ;;
    down)
        [ -f "$OUT/tunnel.pid" ] && kill "$(cat "$OUT/tunnel.pid")" 2>/dev/null || true
        rm -f "$OUT/tunnel.pid"
        compose down
        ;;
    invite) compose exec core nw-core invite --days 7 ;;
    logs)
        compose logs --tail 30 core
        compose exec game tail -n 30 /srv/nw/NWPort/Saved/Logs/NWPort.log 2>/dev/null || compose logs --tail 30 game
        tail -n 15 "$OUT/tunnel.log" 2>/dev/null || true
        ;;
    status)
        compose ps
        echo "túnel: $( [ -f "$OUT/tunnel.pid" ] && kill -0 "$(cat "$OUT/tunnel.pid")" 2>/dev/null && echo rodando || echo parado)"
        ;;
    *) echo "uso: $0 build|tunnel|up|down|invite|logs|status"; exit 2 ;;
esac
