-- Nível, XP, atributos e ofícios do personagem. O servidor de jogo calcula tudo com as tabelas do jogo e salva o
-- objeto inteiro: {"level": 3, "xp": 1200, "spent": [0,0,0,0,5], "trade": [640,0,110,0]}.
alter table characters add column progression jsonb not null default '{}'::jsonb;
