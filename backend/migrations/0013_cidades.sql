-- Governança das cidades (docs/plano-governanca.md): uma cidade por região, dono por compra, balancete, estoque de
-- matéria-prima, preços do NPC, imposto, mesas, defesa, buffs, limites semanais por personagem, semanas e log.
-- Moedas em centavos. As regras (faixas, custos, prêmios) vêm de tools/governanca.json e tools/precos_cidade.json.

create table cities (
    region          text primary key,
    name            text not null,
    owner_id        uuid references characters(id),
    owner_since     timestamptz,
    paid            bigint not null default 0,          -- quanto o dono pagou (venda obrigatória volta a este preço inicial)
    for_sale        bigint,                             -- preço pedido; nulo = não está à venda
    forced_sale     boolean not null default false,     -- tenebroso: à venda pelo preço inicial, o dono não tira
    treasury        bigint not null check (treasury >= 0),
    tax             real not null,
    buy_prices      jsonb not null default '{}',        -- matéria-prima -> centavos (ausente = padrão)
    sell_prices     jsonb not null default '{}',        -- item da lista -> centavos
    weapons         integer not null default 0,
    next_attack_at  timestamptz,
    last_attack     jsonb,
    week            text not null                       -- semana corrente (início, AAAA-MM-DD)
);

-- Nível de cada mesa; "reached" = maior nível já alcançado (conserto até ele não conta na meta).
create table city_stations (
    region   text not null references cities(region),
    station  text not null,
    level    integer not null default 0,
    reached  integer not null default 0,
    crafts   integer not null default 0,                -- fabricações na semana (manutenção)
    primary key (region, station)
);

create table city_stock (
    region   text not null references cities(region),
    item_id  text not null,
    quantity bigint not null default 0 check (quantity >= 0),
    primary key (region, item_id)
);

-- Buffs comprados para a semana (vale nas mesas da cidade).
create table city_buffs (
    region   text not null references cities(region),
    week     text not null,
    station  text not null,
    buff     text not null,
    primary key (region, week, station, buff)
);

-- Limites semanais por personagem e item, na região (iguais em todos os canais).
create table city_limits (
    region       text not null references cities(region),
    week         text not null,
    character_id uuid not null references characters(id),
    item_id      text not null,
    sold         bigint not null default 0,             -- vendido à cidade (matéria-prima)
    bought       bigint not null default 0,             -- comprado da lista
    primary key (region, week, character_id, item_id)
);

-- Números de cada semana: base da meta, faixa e prêmio.
create table city_weeks (
    region      text not null references cities(region),
    week        text not null,
    owner_id    uuid,
    raw_units   bigint not null default 0,
    raw_coins   bigint not null default 0,
    repair_value bigint not null default 0,             -- valor da matéria-prima gasta em conserto (sai da meta)
    list_units  bigint not null default 0,
    list_coins  bigint not null default 0,
    tax_coins   bigint not null default 0,
    project_value bigint not null default 0,            -- níveis novos e buffs
    buffs       integer not null default 0,
    score       double precision,
    tier        text,
    prize       bigint,
    primary key (region, week)
);

-- Log da cidade (append-only): compras, vendas, imposto, projetos, ataques, prêmio, troca de dono.
create table city_log (
    serial       bigint generated always as identity primary key,
    region       text not null references cities(region),
    at           timestamptz not null default now(),
    kind         text not null,
    character_id uuid,
    item_id      text,
    quantity     bigint not null default 0,
    coins        bigint not null default 0,
    detail       jsonb not null default '{}'
);
create index city_log_region_at on city_log (region, at);
