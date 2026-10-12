-- Contas do clã: só entram por convite.
create table accounts (
    id            uuid primary key,
    name          text not null,
    password_hash text not null,
    is_admin      boolean not null default false,
    created_at    timestamptz not null default now()
);
create unique index accounts_name_key on accounts (lower(name));

create table invites (
    code        text primary key,
    created_by  uuid references accounts (id),
    created_at  timestamptz not null default now(),
    expires_at  timestamptz not null,
    used_by     uuid references accounts (id),
    used_at     timestamptz
);

-- Sessões do cliente (token opaco; guardamos só o SHA-256).
create table sessions (
    token_hash  bytea primary key,
    account_id  uuid not null references accounts (id) on delete cascade,
    created_at  timestamptz not null default now(),
    expires_at  timestamptz not null
);
create index sessions_account on sessions (account_id);

create table characters (
    id            uuid primary key,
    account_id    uuid not null references accounts (id) on delete cascade,
    name          text not null,
    created_at    timestamptz not null default now(),
    last_seen_at  timestamptz,
    -- Estado salvo pelo servidor de jogo.
    map           text not null default 'L_SouthWindsward',
    pos_x         double precision,
    pos_y         double precision,
    pos_z         double precision,
    yaw           real not null default 0,
    health        real,
    active_weapon text not null default 'Sword',
    loadout       jsonb not null default '{}',
    mastery       jsonb not null default '{}',
    deleted_at    timestamptz
);
create unique index characters_name_key on characters (lower(name)) where deleted_at is null;
create index characters_account on characters (account_id) where deleted_at is null;

-- Passe de entrada: o cliente recebe do backend e entrega ao servidor de jogo, que resgata uma vez.
create table join_tickets (
    ticket_hash   bytea primary key,
    account_id    uuid not null references accounts (id) on delete cascade,
    character_id  uuid not null references characters (id) on delete cascade,
    created_at    timestamptz not null default now(),
    expires_at    timestamptz not null,
    redeemed_at   timestamptz
);
