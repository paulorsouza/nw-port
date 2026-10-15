# Dungeon simples (plano, para outra esteira)

Pedido do dono (2026-10-14), anotado pela esteira 4 junto com a governança das cidades (`docs/plano-governanca.md`).
Fica para outra esteira; aqui só o que foi pedido e o que já se sabe.

## Pedido

- Uma dungeon no início, bem simples, solo ou em grupo.
- Precisa de entrada (item). A entrada é feita com materiais que caem de inimigos; pode ser vendida (mercado) ou
  usada.
- Baús na dungeon; um baú no fim.
- 500 moedas fixas por dungeon concluída. É uma das fontes de moeda nova do port (com PvP e a venda de material às
  cidades).
- Duração média de ~5 minutos para jogadores fortes.

## Ligações com o que existe

- Entrada vai ao mercado: entra na lista do que pode ser vendido (`mercado` em `tools/itens.json`,
  `FNWItemRules::CanTrade`).
- Moedas e itens da dungeon como operações gravadas (`NWOps`), com motivo no `item_log` (ex. `dungeon:<id>`).
- Material da entrada: novo item no catálogo e na tabela de saque das criaturas.
- No jogo, as expedições usavam entrada (tuning orb) feita com material de inimigos: mesma ideia, mais simples.

## Em aberto para a esteira que pegar

Mapa (reaproveitar um trecho de expedição convertido ou montar um simples), inimigos e chefe, conteúdo dos baús,
receita da entrada, limite por dia (se houver), grupo máximo.
