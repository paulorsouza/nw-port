# nw-core

Backend of the clan server: accounts by invite, sessions, characters, and the entry ticket that the game server
redeems. Rust (axum + sqlx) and Postgres.

Architecture and persistence rules: [docs/tech/architecture.md](../docs/tech/architecture.md) and
[docs/tech/server.md](../docs/tech/server.md). Testing from the editor: [docs/tech/backend-testing.md](../docs/tech/backend-testing.md).

## Run locally

```bash
docker compose up -d
export DATABASE_URL=postgres://nw:nw@localhost:55433/nw
cargo run -- invite                      # generates an invite (applies the migrations)
NW_SERVER_SECRET=dev-secret-0123456789 cargo run -- serve   # API on 0.0.0.0:8090
cargo run -- admin <conta>               # makes the account admin (can generate invites through the API)
```

Tests (each test creates a temporary database in the Postgres at `DATABASE_URL`):

```bash
DATABASE_URL=postgres://nw:nw@localhost:55433/nw cargo test
```

## API

Client (`Authorization: Bearer <token>`):

| Method | Route | Body | Response |
|---|---|---|---|
| POST | `/v1/auth/register` | `invite`, `name`, `password` | 201 `token`, `expires_at`, `account` |
| POST | `/v1/auth/login` | `name`, `password` | `token`, `expires_at`, `account` |
| POST | `/v1/auth/logout` | | 204 |
| GET | `/v1/me` | | `account`, `characters` |
| POST | `/v1/invites` | (admin) | 201 `code`, `expires_at` |
| GET | `/v1/characters` | | list |
| POST | `/v1/characters` | `name` | 201 character |
| DELETE | `/v1/characters/{id}` | | 204 |
| POST | `/v1/characters/{id}/join` | | `ticket` (single use, 60 s), `expires_at` |

Game server (`X-NW-Server-Secret`):

| Method | Route | Body | Response |
|---|---|---|---|
| POST | `/internal/tickets/redeem` | `ticket` | `account_id`, `account_name`, `character` |
| PUT | `/internal/characters/{id}/state` | full state; the whole bag (`inventory`) or only the slots that changed (`inventory_patch`: `container`, `slot`, `item` or null) | 204 |
| GET | `/internal/characters/{id}/storage/shared` | | `version`, `items`, `tab_names`, `capacity` |
| PUT | `/internal/characters/{id}/storage/shared` | `version`, `items`, `inventory`, `coins`; optional `tab_names`, `capacity` | same as GET; 409 `storage_changed` |
| GET | `/internal/characters/{id}/houses` | | list |
| POST | `/internal/characters/{id}/houses` | `territory`, `address`, `house_type`, `price`, `inventory`, `coins` | 201 house; 409 `house_owned` |
| GET | `/internal/characters/{id}/credits` | | `amount`, `until` (credits to claim up to that instant) |
| POST | `/internal/characters/{id}/credits/claim` | `session`, `seq`, `reason`, `until` | `amount` (sums only the credits up to `until`) |
| GET | `/internal/market/{territory}/orders` | filters `item_id`, `kind`, `character_id` | open orders |
| POST | `/internal/market/{territory}/orders` | `character_id`, `kind` (`sell`/`buy`), `item_id`, `quantity`, `gear_score`, `rarity`, `props`, `unit_price`, `fee`, `duration_hours`, `weight`, `inventory`, `coins` | 201 order; 400 `item_bound`; 409 `storage_full` (buy), `too_many_orders` (50 open) |
| POST | `/internal/market/orders/{id}/fill` | `character_id`, `quantity`, `inventory`, `coins` (buy order: `gear_score`, `rarity`, `props` of the delivered item) | order; 409 `order_closed`/`order_quantity`/`storage_full` (buying from a sell order); 400 `item_bound` |
| POST | `/internal/market/orders/{id}/cancel` | `character_id` | 204 |
| GET | `/internal/characters/{id}/ground` | | live loot bags (`id`, `map`, `position`, `expires_at`, `items`); deletes expired ones |
| POST | `/internal/characters/{id}/ground` | `map`, `position`, `ttl_seconds`, `items`, `inventory`, `coins` | 201 loot bag |
| POST | `/internal/ground/{bag}/take` | `character_id`, `items` (what remains), `inventory`, `coins` | `remaining`; 409 `ground_gone` |
| POST | `/internal/characters/{id}/coins-cap` | `amount`, `reason` | 204: records coins cut by the cap (20 million) |
| GET | `/internal/item-log` | filters `character_id`, `character`, `item_id`, `serial`, `since`, `limit` | log rows, newest first |
| POST | `/internal/characters/{id}/serials` | | 201 `serial` (serial number of a signed item; the character is the crafter) |
| GET | `/internal/characters/{id}/npc-rewards` | `period` (Tuesday of the week) | `claims`: rewards already taken (`once` + the period) |
| POST | `/internal/characters/{id}/npc-rewards` | `npc`, `kind` (`starter`/`weekly`), `period` (`once`/`AAAA-MM-DD`), `inventory`, `coins` | 201; 409 `already_claimed` |
| GET | `/internal/regions/{region}/city` | `character_id` (optional: limits already used this week) | city: owner, balance sheet, prices (`buy`/`sell` with range, limit and usage), tax, stations, stock, buffs, weapons, attacks, week |
| PUT | `/internal/regions/{region}/city` | `character_id` (governor), `buy_prices`, `sell_prices` (item → cents), `tax` | city; 400 `price_out_of_range`/`tax_out_of_range`; 409 `not_governor` |
| POST | `/internal/regions/{region}/city/buy` | `character_id`, `price` (what the player saw), `inventory`, `coins` | ownerless city at the fixed price; 409 `price_changed`/`city_not_for_sale` |
| POST | `/internal/regions/{region}/city/sale` | `character_id` (governor), `price` (opening bid; null cancels) | opens or cancels the 24 h auction; 409 `city_grace`/`forced_sale`/`auction_running`/`auction_has_bids` |
| POST | `/internal/regions/{region}/city/bid` | `character_id`, `amount`, `inventory`, `coins` (already without the bid) | city; 409 `no_auction`/`bid_too_low`/`own_city` |
| POST | `/internal/regions/{region}/city/sell-raw` | `character_id`, `item_id`, `quantity`, `unit_price`, `inventory`, `coins` | city; 409 `price_changed`/`city_limit`/`city_no_funds` |
| POST | `/internal/regions/{region}/city/buy-item` | same (item from the list) | city; 409 `price_changed`/`city_limit` |
| POST | `/internal/regions/{region}/city/projects` | `character_id` (governor), `kind` (`station`/`buff`/`weapon`), `station`, `buff`, `weapon` | city; 24 h project (station: tier up to 5, then level or repair; weapon: tier); 409 `city_no_stock`/`city_no_funds`/`buff_active`/`station_max`/`weapon_max`/`work_running` |
| GET | `/internal/regions/{region}/city/log` | `limit` | city log, newest first |
| POST | `/internal/cities/advance` | `now` (optional, tests) | 204: week rollover and due attacks (nw-core runs this by itself every minute) |

## Items

Item (bag, storage and order): `item_id`, `quantity`, `slot` (bag: `container`), `gear_score`, `rarity` and `props`
(object up to 2 KB with attribute, quality, perks, gem, bind and signature; [docs/game/items.md](../docs/game/items.md)).
The database does not interpret `props`; only `bound: true` blocks the Trading Post. In storage, items stack only when
`props` are equal.

A signed item carries `props.serial` (issued by `/serials`). Every write checks that the number exists (400
`unknown_serial`) and that it appears only once across bag, storage and open orders (409 `item_duplicate`). The
registry (`item_serials`) keeps the current owner and location.

## Storage, market and houses

Storage, market and houses are outside `state`, which rewrites the whole bag. Each action carries the character's bag
and coins as they are after the action (`inventory`, `coins`), written in the same transaction as the action.

- Shared storage (`shared`, migration 0010): one per character, opened at any storage shed. Items have `tab` (0–4) and
  `weight` (unit weight in kg, from the game server); `tab_names` holds the tab names. `capacity` is the sum of the
  sources (1000 kg per known city, house chests, city level), computed by the game server.
- Storage has a version: a PUT with an old version returns 409 (read again and redo).
- A buy (buy order or buying from a sell order) only goes through if storage + open buy orders + the purchase fit in
  `capacity` (409 `storage_full`). Sell orders have no limit. The market delivers to storage even above capacity
  (tab 1). There is one market (the game server uses the territory `global`).
- The seller is paid in `coin_credits`. The game server reads `credits`, claims with `credits/claim` up to the `until`
  it read, and adds the amount to the coins in the same write.
- A cancelled or expired order returns the item to storage (sell) or the deposit as credit (buy).
- No fees in the port (`fee` = 0); duration comes from the game server. Up to 50 open orders per character.

## Cities

Migration 0013, [docs/game/economy.md](../docs/game/economy.md). One city per region. Rules in
`tools/governanca.json` and price ranges in `tools/precos_cidade.json` (read at build time).

- The city buys raw materials (paid from the balance sheet) and sells its list (paid into the balance sheet). The
  governor gets at most the default price when selling to their own city.
- Crafting tax comes in `state` (`city_tax`: `region`, `station`, `amount`).
- Governor prize, city sale and an outbid bid become `coin_credits` (`city_prize`, `city_sold`, `city_bid_refund`).
- Selling a city that has an owner is a 24 h auction (migration 0014): by the governor (their opening bid, 10% fee)
  or forced when the week's result is in the lowest band ("tenebroso", below "bad"), at the opening price.

## Character state

`map`, `position` (`x`, `y`, `z` in Unreal cm, or null), `yaw`, `health`, `active_weapon`, `loadout` and `mastery`
(free JSON, per weapon).

## Errors

`{"error": "<código>", "message": "<texto para o jogador>"}` (`message` is text for the player).

## Rules

Account 3–20 characters `[A-Za-z0-9_]`, password 8–128, character 3–16 letters, up to 3 characters per account,
unique names, case-insensitive. Passwords in Argon2id; tokens stored as SHA-256.
