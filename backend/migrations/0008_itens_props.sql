-- Itens do port (docs/itens.md): atributo, qualidade, perks, gema, vínculo e assinatura vão num JSON do item. O banco
-- guarda e devolve sem interpretar; só o vínculo é lido (item vinculado não vai ao posto de trocas).
alter table items add column props jsonb not null default '{}'::jsonb check (jsonb_typeof(props) = 'object');
alter table storage_items add column props jsonb not null default '{}'::jsonb check (jsonb_typeof(props) = 'object');
alter table market_orders add column props jsonb not null default '{}'::jsonb check (jsonb_typeof(props) = 'object');
