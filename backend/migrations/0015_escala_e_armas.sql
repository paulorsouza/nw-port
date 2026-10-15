-- Governança v2 (dono 2026-10-15): mesa da cidade com escala (2 a 5) e, depois da 5, níveis; obras de 24 h; armas do
-- forte do jogo por tipo, com escala (defesa = soma das escalas). "level"/"reached" passam a ser os níveis depois da
-- escala 5 (recomeçam do zero).

alter table city_stations
    add column tier         integer not null default 2,
    add column work         text,                         -- obra em andamento: 'tier', 'level' ou 'repair'
    add column work_target  integer,
    add column work_ends_at timestamptz;
update city_stations set level = 0, reached = 0;

create table city_weapons (
    region       text not null references cities(region),
    weapon       text not null,
    tier         integer not null default 1,
    work_target  integer,
    work_ends_at timestamptz,
    primary key (region, weapon)
);

alter table cities drop column weapons;
