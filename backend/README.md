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
| PUT | `/internal/characters/{id}/state` | estado completo; a bolsa inteira (`inventory`) ou só os espaços que mudaram (`inventory_patch`: `container`, `slot`, `item` ou nulo) | 204 |
| GET | `/internal/characters/{id}/storage/shared` | | `version`, `items`, `tab_names`, `capacity` |
| PUT | `/internal/characters/{id}/storage/shared` | `version`, `items`, `inventory`, `coins`; opcionais `tab_names`, `capacity` | igual ao GET; 409 `storage_changed` |
| GET | `/internal/characters/{id}/houses` | | lista |
| POST | `/internal/characters/{id}/houses` | `territory`, `address`, `house_type`, `price`, `inventory`, `coins` | 201 casa; 409 `house_owned` |
| GET | `/internal/characters/{id}/credits` | | `amount`, `until` (créditos a resgatar até aquele instante) |
| POST | `/internal/characters/{id}/credits/claim` | `session`, `seq`, `reason`, `until` | `amount` (soma só os créditos até `until`) |
| GET | `/internal/market/{território}/orders` | filtros `item_id`, `kind`, `character_id` | ordens abertas |
| POST | `/internal/market/{território}/orders` | `character_id`, `kind` (`sell`/`buy`), `item_id`, `quantity`, `gear_score`, `rarity`, `props`, `unit_price`, `fee`, `duration_hours`, `weight`, `inventory`, `coins` | 201 ordem; 400 `item_bound`; 409 `storage_full` (compra), `too_many_orders` (50 abertas) |
| POST | `/internal/market/orders/{id}/fill` | `character_id`, `quantity`, `inventory`, `coins` (compra: `gear_score`, `rarity`, `props` do item entregue) | ordem; 409 `order_closed`/`order_quantity`/`storage_full` (comprar de venda); 400 `item_bound` |
| POST | `/internal/market/orders/{id}/cancel` | `character_id` | 204 |
| GET | `/internal/characters/{id}/ground` | | sacos vivos (`id`, `map`, `position`, `expires_at`, `items`); apaga os vencidos |
| POST | `/internal/characters/{id}/ground` | `map`, `position`, `ttl_seconds`, `items`, `inventory`, `coins` | 201 saco |
| POST | `/internal/ground/{saco}/take` | `character_id`, `items` (o que sobra), `inventory`, `coins` | `remaining`; 409 `ground_gone` |
| GET | `/internal/item-log` | filtros `character_id`, `character`, `item_id`, `serial`, `since`, `limit` | linhas do log, mais novas primeiro |
| POST | `/internal/characters/{id}/serials` | | 201 `serial` (número de série do item assinado; o personagem é o artesão) |
| GET | `/internal/characters/{id}/npc-rewards` | `period` (terça da semana) | `claims`: brindes já pegos (`once` + o período) |
| POST | `/internal/characters/{id}/npc-rewards` | `npc`, `kind` (`starter`/`weekly`), `period` (`once`/`AAAA-MM-DD`), `inventory`, `coins` | 201; 409 `already_claimed` |
| GET | `/internal/regions/{região}/city` | `character_id` (opcional: limites já usados na semana) | cidade: dono, balancete, preços (`buy`/`sell` com faixa, limite e uso), imposto, mesas, estoque, buffs, armas, ataques, semana |
| PUT | `/internal/regions/{região}/city` | `character_id` (governador), `buy_prices`, `sell_prices` (item → centavos), `tax` | cidade; 400 `price_out_of_range`/`tax_out_of_range`; 409 `not_governor` |
| POST | `/internal/regions/{região}/city/buy` | `character_id`, `price` (o que viu), `inventory`, `coins` | cidade sem dono pelo preço fixo; 409 `price_changed`/`city_not_for_sale` |
| POST | `/internal/regions/{região}/city/sale` | `character_id` (governador), `price` (lance inicial; nulo cancela) | abre ou cancela o leilão de 24 h; 409 `city_grace`/`forced_sale`/`auction_running`/`auction_has_bids` |
| POST | `/internal/regions/{região}/city/bid` | `character_id`, `amount`, `inventory`, `coins` (já sem o lance) | cidade; 409 `no_auction`/`bid_too_low`/`own_city` |
| POST | `/internal/regions/{região}/city/sell-raw` | `character_id`, `item_id`, `quantity`, `unit_price`, `inventory`, `coins` | cidade; 409 `price_changed`/`city_limit`/`city_no_funds` |
| POST | `/internal/regions/{região}/city/buy-item` | idem (item da lista) | cidade; 409 `price_changed`/`city_limit` |
| POST | `/internal/regions/{região}/city/projects` | `character_id` (governador), `kind` (`station`/`buff`/`weapon`), `station`, `buff` | cidade; 409 `city_no_stock`/`city_no_funds`/`buff_active`/`station_max` |
| GET | `/internal/regions/{região}/city/log` | `limit` | log da cidade, mais novo primeiro |
| POST | `/internal/cities/advance` | `now` (opcional, testes) | 204: virada da semana e ataques vencidos (o nw-core roda sozinho a cada minuto) |

Item (bolsa, armazém e ordem): `item_id`, `quantity`, `slot` (bolsa: `container`), `gear_score`, `rarity` e `props`
(objeto até 2 KB com atributo, qualidade, perks, gema, vínculo e assinatura; `docs/itens.md`). O banco não interpreta
`props`; só `bound: true` impede o posto de trocas. No armazém, só empilha item com `props` igual.
Item assinado leva `props.serial` (emitido por `/serials`): toda gravação confere que o número existe (400
`unknown_serial`) e aparece uma vez só somando bolsa, armazém e ordens abertas (409 `item_duplicate`); o registro
(`item_serials`) guarda o dono e o local atuais.

Armazém, mercado e casas ficam fora do `state`, que regrava a bolsa inteira. Cada ação leva a bolsa e as moedas
do personagem como ficam depois dela (`inventory`, `coins`), gravadas na mesma transação da ação.
- Armazém compartilhado (`shared`, migration 0010): um por personagem, aberto em qualquer despensa; item com `tab`
  (0–4) e `weight` (peso unitário em kg, do servidor de jogo); `tab_names` com os nomes das abas. `capacity` é a soma
  das fontes (1000 kg por cidade conhecida, baús das casas, nível da cidade) feita pelo servidor de jogo.
- O armazém tem versão: o PUT com uma versão velha devolve 409 (ler de novo e refazer).
- Compra (ordem de compra ou comprar de uma venda) só passa se armazém + compras abertas + a compra couberem em
  `capacity` (409 `storage_full`). Venda não tem limite. O mercado entrega no armazém mesmo acima da capacidade (aba
  1); o mercado é um só (o servidor de jogo usa o território `global`).
- O vendedor recebe em `coin_credits`. O servidor de jogo consulta `credits`, resgata com `credits/claim` até o
  `until` lido e soma às moedas na mesma gravação.
- Ordem cancelada ou vencida devolve o item ao armazém (venda) ou a garantia em crédito (compra).
- Sem taxas no port (`fee` = 0); duração vem do servidor de jogo. Até 50 ordens abertas por personagem.

- Cidades (migration 0013, `docs/plano-governanca.md`): uma por região, regras em `tools/governanca.json` e faixas
  de preço em `tools/precos_cidade.json` (lidos no build). A cidade compra matéria-prima (sai do balancete) e vende
  a lista (entra no balancete); o governador recebe no máximo o padrão vendendo à própria cidade. Imposto de
  fabricação vem no `state` (`city_tax`: `region`, `station`, `amount`). Prêmio do governador, venda da cidade e lance coberto
  viram `coin_credits` (`city_prize`, `city_sold`, `city_bid_refund`). Venda da cidade com dono é leilão de 24 h
  (migration 0014): do governador (lance inicial dele, taxa de 10%) ou obrigatório no tenebroso (preço inicial).

Estado do personagem: `map`, `position` (`x`, `y`, `z` em cm do Unreal, ou null), `yaw`, `health`,
`active_weapon`, `loadout` e `mastery` (JSON livre, por arma).

Erros: `{"error": "<código>", "message": "<texto para o jogador>"}`.

Regras: conta 3–20 caracteres `[A-Za-z0-9_]`, senha 8–128, personagem 3–16 letras, até 3 personagens
por conta, nomes únicos sem diferenciar maiúsculas. Senhas em Argon2id; tokens guardados como SHA-256.
