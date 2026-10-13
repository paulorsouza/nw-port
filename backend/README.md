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
| GET | `/internal/characters/{id}/storage/{local}` | | `version`, `items` |
| PUT | `/internal/characters/{id}/storage/{local}` | `version`, `items`, `inventory`, `coins` | `version`, `items`; 409 `storage_changed` |
| GET | `/internal/characters/{id}/houses` | | lista |
| POST | `/internal/characters/{id}/houses` | `territory`, `address`, `house_type`, `price`, `inventory`, `coins` | 201 casa; 409 `house_owned` |
| POST | `/internal/characters/{id}/credits/claim` | | `amount` |
| GET | `/internal/market/{território}/orders` | filtros `item_id`, `kind`, `character_id` | ordens abertas |
| POST | `/internal/market/{território}/orders` | `character_id`, `kind` (`sell`/`buy`), `item_id`, `quantity`, `gear_score`, `rarity`, `props`, `unit_price`, `fee`, `duration_hours`, `inventory`, `coins` | 201 ordem; 400 `item_bound` |
| POST | `/internal/market/orders/{id}/fill` | `character_id`, `quantity`, `inventory`, `coins` (compra: `gear_score`, `rarity`, `props` do item entregue) | ordem; 409 `order_closed`/`order_quantity`; 400 `item_bound` |
| POST | `/internal/market/orders/{id}/cancel` | `character_id` | 204 |
| POST | `/internal/characters/{id}/serials` | | 201 `serial` (número de série do item assinado; o personagem é o artesão) |
| GET | `/internal/characters/{id}/npc-rewards` | `period` (terça da semana) | `claims`: brindes já pegos (`once` + o período) |
| POST | `/internal/characters/{id}/npc-rewards` | `npc`, `kind` (`starter`/`weekly`), `period` (`once`/`AAAA-MM-DD`), `inventory`, `coins` | 201; 409 `already_claimed` |

Item (bolsa, armazém e ordem): `item_id`, `quantity`, `slot` (bolsa: `container`), `gear_score`, `rarity` e `props`
(objeto até 2 KB com atributo, qualidade, perks, gema, vínculo e assinatura; `docs/itens.md`). O banco não interpreta
`props`; só `bound: true` impede o posto de trocas. No armazém, só empilha item com `props` igual.
Item assinado leva `props.serial` (emitido por `/serials`): toda gravação confere que o número existe (400
`unknown_serial`) e aparece uma vez só somando bolsa, armazém e ordens abertas (409 `item_duplicate`); o registro
(`item_serials`) guarda o dono e o local atuais.

Armazém, mercado e casas ficam fora do `state`, que regrava a bolsa inteira. Cada ação leva a bolsa e as moedas
do personagem como ficam depois dela (`inventory`, `coins`), gravadas na mesma transação da ação.
- `{local}`: `settlement:<território>` ou `house:<id da casa>`.
- O armazém tem versão: o PUT com uma versão velha devolve 409 (ler de novo e refazer).
- O mercado entrega o item comprado no armazém da cidade da ordem.
- O vendedor recebe em `coin_credits`. O servidor de jogo resgata com `credits/claim`, soma às moedas e salva.
- Ordem cancelada ou vencida devolve o item ao armazém (venda) ou a garantia em crédito (compra).
- Taxas e duração vêm do servidor de jogo.

Estado do personagem: `map`, `position` (`x`, `y`, `z` em cm do Unreal, ou null), `yaw`, `health`,
`active_weapon`, `loadout` e `mastery` (JSON livre, por arma).

Erros: `{"error": "<código>", "message": "<texto para o jogador>"}`.

Regras: conta 3–20 caracteres `[A-Za-z0-9_]`, senha 8–128, personagem 3–16 letras, até 3 personagens
por conta, nomes únicos sem diferenciar maiúsculas. Senhas em Argon2id; tokens guardados como SHA-256.
