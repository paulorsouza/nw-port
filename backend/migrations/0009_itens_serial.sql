-- Itens assinados (docs/itens.md, Assinatura): o número de série é emitido aqui e segue o item em props.serial. O
-- registro guarda dono e local atuais; toda gravação com item assinado confere que o número existe e que aparece uma vez
-- só somando bolsa, armazém e ordens abertas do posto de trocas (src/routes/serials.rs).
create table crafter_sequences (
    character_id  uuid primary key references characters (id) on delete cascade,
    last          bigint not null default 0
);

create table item_serials (
    serial        text primary key,
    crafter_id    uuid references characters (id) on delete set null,
    character_id  uuid references characters (id) on delete set null,
    location      text,
    created_at    timestamptz not null default now(),
    updated_at    timestamptz not null default now()
);

create index items_serial on items ((props ->> 'serial')) where props ? 'serial';
create index storage_items_serial on storage_items ((props ->> 'serial')) where props ? 'serial';
create index market_orders_serial on market_orders ((props ->> 'serial')) where props ? 'serial';
