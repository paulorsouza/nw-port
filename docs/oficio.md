# Ofício do port

Regras do port para fabricar, refinar e evoluir as mesas. Decididas com o dono em 2026-10-13. Itens fabricados (GS,
qualidade, perks): `docs/itens.md`. Estado das estações de Queda Eterna e dados do jogo: `docs/sistemas.md` §2.
Levantamento da tela e do GS do jogo: `re-local/oficio.md`.

## Mesas

- Mesas de Queda Eterna que fabricam itens do port: ferraria (armeiro), engenharia, alquimia (arcano) e a estação de
  equipamentos (joalheria: gemas lapidadas). Cozinha e armoraria entram quando houver comida com efeito e armadura.
- Vidro rúnico: a cápsula sai da forja e o vidro (cápsula + gema Pura) da cantaria, porque Queda Eterna não tem a
  estação de runas do jogo.
- A mesa evolui com material investido nela. Não se fabrica item para evoluir a mesa.

## Fabricar

- Refinar e fabricar são na hora, como no jogo. O item fabricado vai para a bolsa.
- Fabricar arma, escudo ou ferramenta só a partir do nível 50 do ofício da mesa. Antes disso a mesa faz só treino: gasta
  o material, dá XP do ofício e não gera item.
- Sem espaço na bolsa, não fabrica (nada se perde).

## Materiais

- Fabricar usa só material refinado, um por espaço da receita, do tier do item: lingote, tábua e couro curtido (ex.: espada
  T3 = lingote T3, tábua T3, couro T3). Não há escolha entre materiais alternativos como no jogo.

## Vínculo

- Desvincular um item vinculado ao equipar é uma fabricação. O custo sobe com a qualidade do item (`docs/itens.md`).

## Ideia descartada: fabricar com tempo e espectros (2026-10-13)

Feita, testada pelo dono e descartada no mesmo dia ("a ideia do espectro é ruim"). Fica registrada para não voltar:
- Fabricar levava horas (base 3 h no T2 a 12 h no T5, menos até 50% pelo nível do ofício e até 25% por itens).
- Sem espectro o personagem ficava preso na mesa; com espectro (até 5, um por mesa, liberado no nível 50) ele podia sair e
  o item ia para o armazém da cidade no fim. O espectro só aparecia para o dono.
- Cancelar devolvia o material e perdia o tempo. A entrega no armazém usava uma chave por fabricação no nw-core para não
  duplicar.
- Removido do código, dos dados e do nw-core (a migração `0008` ficou só com `props`).

## Pendente

- Material e XP do treino (hoje: o da receita).
- Material e níveis da evolução das mesas.
