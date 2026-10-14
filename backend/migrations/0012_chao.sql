-- Sacos no chão (etapa 2 das gravações, docs/plano-servidor.md "Gravações"): soltar e pegar são gravados no nw-core na
-- mesma transação da bolsa; o saco vive até expires_at e volta no mesmo lugar se o servidor reiniciar. Vencido, o
-- nw-core apaga e registra no item_log (ground_expired).
create table ground_bags (
    id            uuid primary key,
    character_id  uuid not null references characters (id) on delete cascade,
    map           text not null check (length(map) between 1 and 120),
    pos_x         double precision not null,
    pos_y         double precision not null,
    pos_z         double precision not null,
    created_at    timestamptz not null default now(),
    expires_at    timestamptz not null
);
create index ground_bags_owner on ground_bags (character_id);
create index ground_bags_expires on ground_bags (expires_at);

create table ground_items (
    bag_id        uuid not null references ground_bags (id) on delete cascade,
    slot          integer not null check (slot >= 0),
    item_id       text not null,
    quantity      integer not null check (quantity > 0),
    gear_score    integer not null default 0,
    rarity        smallint not null default 0,
    props         jsonb not null default '{}'::jsonb check (jsonb_typeof(props) = 'object'),
    weight        real not null default 0 check (weight >= 0),
    primary key (bag_id, slot)
);
create index ground_items_serial on ground_items ((props ->> 'serial')) where props ? 'serial';
