-- Venda da cidade por leilão de 24 h (dono 2026-10-14): o governador abre com o lance inicial que quiser; no tenebroso
-- o leilão é obrigatório e começa no preço inicial da cidade. O lance fica guardado (sai das moedas de quem deu) e
-- volta em crédito quando alguém cobre.

alter table cities drop column for_sale;
alter table cities drop column forced_sale;
alter table cities
    add column auction_start   bigint,                  -- lance inicial; nulo = sem leilão
    add column auction_ends_at timestamptz,
    add column auction_forced  boolean not null default false,
    add column auction_bid     bigint,                  -- maior lance
    add column auction_bidder  uuid references characters(id);
