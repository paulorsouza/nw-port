-- Armazém: um por personagem em cada local ("settlement:<território>" ou "house:<id da casa>"). Fica fora do
-- PUT state (que regrava a bolsa inteira): o servidor de jogo grava bolsa e armazém juntos numa transação, com
-- versão por armazém para não apagar o que o mercado entregou nesse meio-tempo.
create table storages (
    character_id  uuid not null references characters (id) on delete cascade,
    location      text not null check (location ~ '^(settlement:[a-z0-9_]{1,40}|house:[0-9a-f-]{36})$'),
    version       bigint not null default 0,
    primary key (character_id, location)
);

create table storage_items (
    id            uuid primary key,
    character_id  uuid not null,
    location      text not null,
    item_id       text not null,
    quantity      integer not null check (quantity > 0),
    slot          integer not null check (slot >= 0),
    gear_score    integer not null default 0,
    rarity        smallint not null default 0,
    created_at    timestamptz not null default now(),
    foreign key (character_id, location) references storages (character_id, location) on delete cascade,
    unique (character_id, location, slot)
);
create index storage_items_owner on storage_items (character_id, location);
