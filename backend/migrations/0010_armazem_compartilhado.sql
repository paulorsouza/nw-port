-- Armazém compartilhado (dono, 2026-10-14): um armazém só por personagem ("shared"), aberto em qualquer despensa,
-- com 5 abas. Capacidade = soma das fontes (1000 kg por cidade conhecida, baús das casas, nível da cidade), calculada
-- pelo servidor de jogo e guardada em characters.storage_capacity. O nw-core guarda o peso unitário de cada item
-- (enviado pelo servidor de jogo) para recusar ordem de compra que passe da capacidade. Casas deixam de ter armazém
-- próprio: os itens de "settlement:*" e "house:*" vão para "shared".

alter table characters add column storage_capacity real not null default 0 check (storage_capacity >= 0);
alter table storages add column tab_names jsonb not null default '[]'::jsonb check (jsonb_typeof(tab_names) = 'array');
alter table storage_items
    add column tab smallint not null default 0 check (tab between 0 and 4),
    add column weight real not null default 0 check (weight >= 0);
alter table market_orders add column weight real not null default 0 check (weight >= 0);

alter table storages drop constraint storages_location_check;

insert into storages (character_id, location)
select distinct character_id, 'shared' from storages
on conflict do nothing;

-- Slots renumerados por personagem (cidade e casa podiam usar o mesmo slot).
update storage_items s
   set location = 'shared', slot = m.n
  from (select id, (row_number() over (partition by character_id order by location, slot) - 1)::int as n
          from storage_items where location <> 'shared') m
 where s.id = m.id;

update storages set version = version + 1 where location = 'shared';
delete from storages where location <> 'shared';
update item_serials set location = 'shared', updated_at = now()
 where location like 'settlement:%' or location like 'house:%';

alter table storages add constraint storages_location_check check (location = 'shared');
