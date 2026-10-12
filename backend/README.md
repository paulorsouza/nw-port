# nw-core

Backend do servidor do clã: contas por convite, sessões, personagens e o passe de entrada que o servidor de
jogo resgata. Rust (axum + sqlx) e Postgres.

## Rodar local

```bash
docker compose up -d
export DATABASE_URL=postgres://nw:nw@localhost:55433/nw
cargo run -- invite                      # gera um convite (aplica as migrações)
NW_SERVER_SECRET=dev-secret-0123456789 cargo run -- serve   # API em 0.0.0.0:8090
cargo run -- admin <conta>               # dá admin (pode gerar convites pela API)
```

Testes (cada teste cria um banco temporário no Postgres de `DATABASE_URL`):

```bash
DATABASE_URL=postgres://nw:nw@localhost:55433/nw cargo test
```

## API

Cliente (`Authorization: Bearer <token>`):

| Método | Rota | Corpo | Resposta |
|---|---|---|---|
| POST | `/v1/auth/register` | `invite`, `name`, `password` | 201 `token`, `expires_at`, `account` |
| POST | `/v1/auth/login` | `name`, `password` | `token`, `expires_at`, `account` |
| POST | `/v1/auth/logout` | | 204 |
| GET | `/v1/me` | | `account`, `characters` |
| POST | `/v1/invites` | (admin) | 201 `code`, `expires_at` |
| GET | `/v1/characters` | | lista |
| POST | `/v1/characters` | `name` | 201 personagem |
| DELETE | `/v1/characters/{id}` | | 204 |
| POST | `/v1/characters/{id}/join` | | `ticket` (uso único, 60 s), `expires_at` |

Servidor de jogo (`X-NW-Server-Secret`):

| Método | Rota | Corpo | Resposta |
|---|---|---|---|
| POST | `/internal/tickets/redeem` | `ticket` | `account_id`, `account_name`, `character` |
| PUT | `/internal/characters/{id}/state` | estado completo | 204 |

Estado do personagem: `map`, `position` (`x`, `y`, `z` em cm do Unreal, ou null), `yaw`, `health`,
`active_weapon`, `loadout` e `mastery` (JSON livre, por arma).

Erros: `{"error": "<código>", "message": "<texto para o jogador>"}`.

Regras: conta 3–20 caracteres `[A-Za-z0-9_]`, senha 8–128, personagem 3–16 letras, até 3 personagens
por conta, nomes únicos sem diferenciar maiúsculas. Senhas em Argon2id; tokens guardados como SHA-256.
