-- Brindes dos NPCs (docs/sistemas.md, "Brindes dos NPCs"): itens iniciais uma vez por personagem e brinde semanal.
-- O servidor de jogo sorteia os itens e grava a bolsa junto (mesmo padrão do armazém); o banco só garante que não
-- se pega duas vezes no mesmo período.
create table npc_rewards (
    character_id  uuid not null references characters (id) on delete cascade,
    npc           text not null check (npc ~ '^[A-Za-z0-9_]{1,80}$'),       -- VariantID do jogo
    kind          text not null check (kind in ('starter', 'weekly')),
    period        text not null check (period ~ '^(once|\d{4}-\d{2}-\d{2})$'), -- 'once' ou a terça que abre a semana
    claimed_at    timestamptz not null default now(),
    primary key (character_id, npc, kind, period)
);
