-- Posto de trocas: ordens de venda (item em garantia) e de compra (moedas em garantia) por território.
-- O que se compra cai no armazém da cidade do comprador; o que o vendedor recebe vira crédito de moedas, que o
-- servidor de jogo resgata (o personagem pode estar on-line e o PUT state sobrescreveria as moedas).
create table market_orders (
    id            uuid primary key,
    territory     text not null check (territory ~ '^[a-z0-9_]{1,40}$'),
    kind          text not null check (kind in ('sell', 'buy')),
    character_id  uuid not null references characters (id) on delete cascade,
    item_id       text not null,
    quantity      integer not null check (quantity >= 0),   -- o que ainda falta negociar
    gear_score    integer not null default 0,
    rarity        smallint not null default 0,
    unit_price    bigint not null check (unit_price > 0),
    fee           bigint not null default 0 check (fee >= 0),
    status        text not null default 'open' check (status in ('open', 'filled', 'cancelled', 'expired')),
    created_at    timestamptz not null default now(),
    expires_at    timestamptz not null,
    closed_at     timestamptz
);
create index market_orders_open on market_orders (territory, item_id, kind) where status = 'open';
create index market_orders_owner on market_orders (character_id);

create table coin_credits (
    id            uuid primary key,
    character_id  uuid not null references characters (id) on delete cascade,
    amount        bigint not null check (amount > 0),
    reason        text not null,
    order_id      uuid references market_orders (id) on delete set null,
    created_at    timestamptz not null default now(),
    claimed_at    timestamptz
);
create index coin_credits_pending on coin_credits (character_id) where claimed_at is null;
