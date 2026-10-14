#!/usr/bin/env bash
# nw-core local de teste, para o Play do editor e os testes com backend (docs/teste-backend.md).
#
#     tools/backend_dev.sh            # sobe (se não estiver no ar) e garante a conta e o personagem do editor
#     tools/backend_dev.sh status     # no ar ou não
#     tools/backend_dev.sh down       # para o nw-core (o banco continua, com tudo salvo)
#     tools/backend_dev.sh log        # últimas linhas do log do nw-core
#
# Postgres: container nw-core-db (backend/compose.yaml), volume nw-core-db; os dados ficam entre sessões.
# nw-core: 127.0.0.1:8090, log em unreal/NWPort/Saved/Play/nw-core.log. Valores de teste em backend/dev.env.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
set -a; source "$ROOT/backend/dev.env"; set +a
OUT="$ROOT/unreal/NWPort/Saved/Play"
PID="$OUT/nw-core.pid"
LOG="$OUT/nw-core.log"
BIN="$ROOT/backend/target/debug/nw-core"
mkdir -p "$OUT"

up() { curl -sf "$NW_BACKEND/health" >/dev/null 2>&1; }

json() { python3 -c "import json,sys; print(json.load(sys.stdin)$1)" 2>/dev/null; }

ensure_account() {
    local account="$NW_PIE_ACCOUNT" character="$NW_PIE_CHARACTER" body token
    body=$(printf '{"name":"%s","password":"%s"}' "$account" "$NW_TEST_PASSWORD")
    if ! token=$(curl -sf -X POST "$NW_BACKEND/v1/auth/login" -H 'Content-Type: application/json' -d "$body" | json '["token"]'); then
        local invite
        invite=$("$BIN" invite 2>/dev/null | awk '{print $1}')
        curl -sf -X POST "$NW_BACKEND/v1/auth/register" -H 'Content-Type: application/json' \
            -d "$(printf '{"invite":"%s","name":"%s","password":"%s"}' "$invite" "$account" "$NW_TEST_PASSWORD")" >/dev/null
        token=$(curl -sf -X POST "$NW_BACKEND/v1/auth/login" -H 'Content-Type: application/json' -d "$body" | json '["token"]')
        echo "conta $account criada"
    fi
    if ! curl -sf "$NW_BACKEND/v1/me" -H "Authorization: Bearer $token" \
            | python3 -c "import json,sys; sys.exit(0 if any(c['name'].lower()=='$character'.lower() for c in json.load(sys.stdin)['characters']) else 1)"; then
        curl -sf -X POST "$NW_BACKEND/v1/characters" -H 'Content-Type: application/json' -H "Authorization: Bearer $token" \
            -d "$(printf '{"name":"%s"}' "$character")" >/dev/null
        echo "personagem $character criado"
    fi
    echo "conta do editor: $account / $character"
}

case "${1:-up}" in
    up)
        if ! up; then
            (cd "$ROOT/backend" && docker compose up -d >/dev/null)
            # Compilar é trabalho pesado (fila); só quando o fonte mudou.
            if [ ! -x "$BIN" ] || [ -n "$(find "$ROOT/backend/src" "$ROOT/backend/migrations" "$ROOT/backend/Cargo.toml" -newer "$BIN" -print -quit)" ]; then
                "$ROOT/tools/pesado.sh" bash -c "cd '$ROOT/backend' && cargo build -q"
            fi
            NW_BIND=127.0.0.1:8090 setsid nohup "$BIN" serve >"$LOG" 2>&1 < /dev/null &
            echo $! > "$PID"
            for _ in $(seq 100); do up && break; sleep 0.3; done
            up || { echo "nw-core não subiu:"; tail -20 "$LOG"; exit 1; }
            echo "nw-core no ar em $NW_BACKEND"
        else
            echo "nw-core já está no ar em $NW_BACKEND"
        fi
        ensure_account
        ;;
    status) up && echo "no ar ($NW_BACKEND)" || { echo "fora do ar"; exit 1; } ;;
    down)
        if [ -f "$PID" ] && kill "$(cat "$PID")" 2>/dev/null; then echo "nw-core parado"; else echo "nw-core não estava rodando por este script"; fi
        rm -f "$PID"
        ;;
    log) tail -40 "$LOG" ;;
    *) echo "uso: $0 [up|status|down|log]"; exit 2 ;;
esac
