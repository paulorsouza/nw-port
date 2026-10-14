-- Ordem das gravações e log de itens (dono, 2026-10-14; docs/plano-servidor.md "Gravações").
-- Cada resgate do passe abre uma sessão nova do personagem (session + 1, save_seq = 0). Toda gravação de bolsa,
-- moedas ou armazém leva session e seq: o nw-core só aplica se a sessão é a atual e seq > save_seq. Assim uma gravação
-- atrasada (fora de ordem ou de um servidor que caiu) nunca desfaz uma mais nova.
alter table characters
    add column session  bigint not null default 0,
    add column save_seq bigint not null default 0;

-- Log só de acréscimo: uma linha por diferença de item ou de moedas em cada gravação, e pelas entregas do próprio
-- nw-core (mercado). Sem chave estrangeira: o log fica mesmo se o personagem for apagado.
create table item_log (
    id            bigserial primary key,
    at            timestamptz not null default now(),
    character_id  uuid not null,
    session       bigint,                 -- nulo = ação do próprio nw-core
    seq           bigint,
    reason        text not null,
    place         text not null,          -- bag, equip, storage:<aba>, market:<ordem>, coins, credit
    item_id       text,                   -- nulo = moedas
    quantity      bigint not null,        -- diferença (+ entrou, − saiu)
    gear_score    integer not null default 0,
    rarity        smallint not null default 0,
    props         jsonb not null default '{}'::jsonb,
    serial        text generated always as (props ->> 'serial') stored
);
create index item_log_character on item_log (character_id, at);
create index item_log_item on item_log (item_id, at);
create index item_log_serial on item_log (serial) where serial is not null;
