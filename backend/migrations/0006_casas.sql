-- Casas: compra e armazém da casa (local "house:<id>" em storages). Decoração e interior ficam para depois.
-- Os lotes são instanciados no jogo: vários personagens podem ter o mesmo endereço; um por território.
create table houses (
    id            uuid primary key,
    character_id  uuid not null references characters (id) on delete cascade,
    territory     text not null check (territory ~ '^[a-z0-9_]{1,40}$'),
    address       text not null check (length(address) between 1 and 120),  -- slice do lote (cityd_house_address_NNN)
    house_type    text not null,                                            -- HouseTypeID (javelindata_housetypes)
    price         bigint not null check (price >= 0),
    purchased_at  timestamptz not null default now(),
    unique (character_id, territory)
);
