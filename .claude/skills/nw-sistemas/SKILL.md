---
name: nw-sistemas
description: Esteira 4 — sistemas do New World na capital de Queda Eterna (mapa L_QuedaEterna) — NPCs, estações de refino e ofício, armazém, lojas, mercado, casas, nível e XP, e as telas de cada um, com servidor dedicado e nw-core. Usar quando pedirem algo desses sistemas, de HUD/telas ou de Queda Eterna.
---

# Sistemas e Queda Eterna (esteira 4)

Regras de trabalho: skill `nw-esteira` e `docs/esteiras.md`. Estado e levantamento: `docs/sistemas.md` (crie no
primeiro item). Testes: `docs/testes/sistemas.md`, mapa `L_QuedaEterna`.

## Escopo

Só a capital de Queda Eterna (`settlement_everfall`, slice `*_complete`) e o recorte de terreno em volta, num mapa
próprio. Outras cidades e o território inteiro de Queda Eterna, só com pedido do dono. Guardaventos continua.

## Ordem

1. Cidade e mapa: skill `nw-convert-city` (`tools/pesado.sh tools/convert_city.sh <slice>`), depois os scripts de
   `tools/ue/` das etapas 04 e 05 do `ue_import_all.sh` apontando para o mapa novo.
2. NPCs: posições e aparência dos slices da cidade, animação parada, nome.
3. Estações de refino e ofício: interação, receitas (`datatables/javelindata_crafting*`), tempo, XP de ofício
   (`javelindata_tradeskill*`), nível mínimo.
4. Armazém.
5. Lojas e mercadores.
6. Mercado (posto de trocas): ordens de compra e venda no nw-core, taxas das tabelas.
7. Casas: lotes, compra, interior, mobília, armazém da casa.
8. Nível e XP com fontes; telas que faltarem.

## Como

- Levante cada sistema em `docs/sistemas.md` antes do código: tabelas do jogo, telas (`docs/hud-original.md`,
  `tools/ui_canvas.py` para o layout), o que o port já tem.
- Telas: layouts do jogo por `tools/ui_build_*.py` → `Data/UI/*.json`, desenhadas em C++ (padrão de
  `NWInventoryMenu`, `NWTradeskillMenu`). Textos em `tools/strings/sistemas.json`
  (pt-BR e en-US).
- Persistência: o servidor dedicado manda, o nw-core (`backend/`, Rust + Postgres) guarda. Mudança de banco é
  migration nova; plano em `docs/plano-servidor.md`. Mostre o plano ao dono antes de mexer no backend.
- Itens e catálogos: `tools/make_items.py` e `tools/make_data.sh` (skill `nw-make-data`).
- Compilar e importar com o editor fechado, abrir no mapa, pedir print.
