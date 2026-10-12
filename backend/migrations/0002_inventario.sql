-- F3: moedas e itens do personagem. O servidor de jogo é autoritativo no inventário e salva o conjunto inteiro
-- (troca numa transação); o backend guarda e devolve no passe de entrada.
alter table characters add column coins bigint not null default 0 check (coins >= 0);

create table items (
    id            uuid primary key,
    character_id  uuid not null references characters (id) on delete cascade,
    item_id       text not null,                       -- ItemID do jogo (Data/Items/items.*.json)
    quantity      integer not null check (quantity > 0),
    container     text not null check (container in ('bag', 'equip')),
    slot          integer not null check (slot >= 0),
    gear_score    integer not null default 0,
    rarity        smallint not null default 0,
    created_at    timestamptz not null default now(),
    unique (character_id, container, slot)
);
create index items_character on items (character_id);
