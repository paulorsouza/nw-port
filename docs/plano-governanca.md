# Governança das cidades (plano)

Plano da esteira 4 para dono de cidade sem guerra (pedidos do dono de 2026-10-14). Ainda não é spec; nada de código
até o dono aprovar o plano. Números marcados "proposta" saem do simulador.

## Regras do dono

- A cidade é comprada com moedas por um preço fixo (quem pagar primeiro leva) e pode ser vendida a outro jogador. Sem
  guerra.
- Uma região tem um dono: canais 1.1 e 1.2 são a região 1, mesmo governador e mesmos limites para os jogadores.
- O governador escolhe os preços dentro de faixas: quanto a cidade paga por cada matéria-prima e quanto cobra pelos
  itens que vende. Sem governador, vale o meio da faixa.
- A cidade só compra matéria-prima, nada fabricado. Vende uma lista fixa: poções e catalisadores de refino (fundente e
  os outros).
- Limites por personagem, por semana, na região: a cidade compra até 10 mil de cada matéria-prima e vende até 1 mil
  de cada item da lista.
- Tudo passa pelo balancete da cidade: ele recebe as vendas da lista e o imposto de fabricação, e paga a
  matéria-prima que compra, os buffs, a defesa e o equipamento. O governador não saca.
- A matéria-prima comprada vai para o estoque da cidade e é gasta para evoluir e consertar mesas.
- Nível das mesas melhora um pouco o resultado da fabricação: bem pouco em GS e o mínimo em %.
- Buffs da cidade valem nas mesas daquela cidade (no jogo vinham da casa do jogador).
- Depois da primeira semana do governador, a cada 2 dias a cidade sofre um ataque simulado. Com defesa fraca, mesas
  e armas da cidade perdem nível.
- Prêmio semanal do governador por faixa de resultado: tenebroso 5 mil, ruim 10 mil, bom 25 mil, muito bom 50 mil,
  excelente 100 mil, melhor governador 200 mil. Metas difíceis e proporcionais às outras cidades.
- Tenebroso: a cidade vai à venda obrigatória pelo preço inicial, para não ficar parada. Não é por horas de jogo: é
  por movimento mínimo (abaixo).
- Toda cidade começa com um balancete inicial (valor a calibrar no simulador).
- Moeda nova no jogo: dungeons (solo e em grupo; 500 por dungeon, `docs/plano-dungeon.md`), PvP e os brindes
  semanais dos NPCs, que passam a dar também um pouco de moeda além da matéria-prima (`tools/npc_rewards.json`). Renda alvo de um
  jogador comum: ~40 mil por semana em ~10 h.

## Região e canais

- `region` no nw-core: id, nome, cidade sede (Queda Eterna = região 1). Cada servidor de jogo sabe a região pelo mapa
  (config). Todos os canais da região leem e gravam o mesmo estado no nw-core.
- Semana = período dos brindes dos NPCs (`FNWNpcRewards::Period`, terça 08:00 UTC).

## Cidade

Por região: dono, preço de compra da cidade, balancete, estoque de matéria-prima, tabela de preços, imposto de
fabricação, nível de cada mesa, equipamento de defesa (armas), buffs ativos, números da semana e log append-only
(como `item_log`).

### Preços

`tools/precos_cidade.json`: faixa (mín–máx) e padrão (meio da faixa) por item, em centavos.
- Faixa = 0,4× a 2,1× o preço central do mercado do jogo (`tools/precos_referencia.json`, mediana de ~36 servidores,
  nwmp.gaming.tools), múltiplo de 0,05. Ex.: minério de ferro 0,20–1,00; minério de orichalcum 0,35–1,85.
- Poção de vida: 0,20–2,00. Mana: o port ainda não tem poção de mana; entra na lista quando existir.
- T5 cru no mercado tardio saía mais barato que T4 (nós T5 abundantes): usamos 1,2× o T4.
- O nw-core recusa preço fora da faixa.

### Fluxo das moedas

| Ação | De | Para |
|---|---|---|
| Jogador vende matéria-prima à cidade | balancete | jogador |
| Jogador compra item da lista | jogador | balancete |
| Imposto de fabricação nas mesas da cidade | jogador | balancete |
| Buff, defesa, equipamento | balancete | some |
| Evoluir ou consertar mesa | estoque (matéria-prima) + balancete | some |
| Prêmio do governador | moeda nova | governador |
| Brinde semanal dos NPCs | moeda nova | jogador |
| Balancete inicial da cidade | moeda nova (uma vez) | balancete |
| Compra da cidade | comprador | some (primeira compra) ou vendedor (revenda, taxa de 10% some) |

- A cidade não cria moeda: o balancete começa com o valor inicial e depois só gira o que os jogadores pagam. Sem moeda no balancete, a cidade para de
  comprar matéria-prima. O jogo do governador é equilibrar preço de compra e de venda.
- O balancete nunca fica negativo.
- O governador também pode vender matéria-prima à própria cidade, com o mesmo limite dos outros e recebendo no máximo
  o padrão (meio da faixa), não o preço que ele pôs.

### Mesas, buffs e imposto

- Evoluir mesa: lista de matéria-prima do estoque + moedas do balancete, 24 h (como os projetos do jogo:
  `javelindata_territoryprogression`, forja nível 1/2/3 = 100 / 5 mil / 10 mil moedas). Efeito por nível (proposta):
  +2 GS no sorteio e +1 ponto na qualidade mínima, até o nível 3.
- Consertar mesa (depois de ataque perdido): matéria-prima do estoque, menos que evoluir.
- Buff da cidade (dura a semana; vale para quem usa as mesas da cidade): +XP de ofício, chance de item extra no
  refino, material a menos, imposto menor (proposta). Pago com o balancete.
- Imposto de fabricação: % sobre o preço de referência do resultado, faixa 0–10% (proposta).

### Defesa e ataques simulados

- Defesa da cidade = equipamento comprado (armas da guarda, com nível) + melhorias de defesa, pagos pelo balancete.
- Depois da primeira semana do governador, a cada 2 dias: ataque simulado com força que cresce com o nível da cidade
  e com a semana (proposta). Defesa ≥ força: nada acontece. Abaixo: 1 a 3 mesas e parte das armas perdem um nível.
- Resultado no log da cidade e aviso aos jogadores da região.

### Prêmio e metas

- Pontuação semanal da cidade = moedas movimentadas (compra de matéria-prima + venda da lista + impostos) + projetos
  concluídos (mesas, defesa, buffs), por jogador ativo da região.
- Faixa pela razão entre a pontuação e a média das cidades (proposta): ruim < 0,8; bom ≥ 0,8; muito bom ≥ 1,2;
  excelente ≥ 1,6; melhor governador = maior pontuação, se excelente. Enquanto houver uma cidade só, a média é a das
  últimas 4 semanas dela.
- Tenebroso (abaixo de ruim): na semana a cidade comprou menos de 5 mil de matéria-prima no total (metade do limite
  de um personagem, somando todos os tipos), vendeu menos de 500 itens da lista no total, ou nenhum buff foi comprado.
  Se ninguém mais negocia, o próprio governador tem de fazer isso.
- Tenebroso: prêmio de 5 mil e venda obrigatória pelo preço inicial; o primeiro que pagar leva, o valor vai para o
  governador que sai, e ele não tira a cidade da venda.

### Compra e venda da cidade

- Primeira compra: preço fixo, o valor some.
- Revenda: o dono põe à venda por um preço; quem pagar leva. Taxa de 10% some. Carência de 2 semanas depois de
  comprar (proposta). Balancete, estoque, mesas e defesa ficam com a cidade.
- Sem dono: preços no meio da faixa, imposto padrão, sem ataques e sem prêmio.

## Balanceamento

Simulador (`tools/sim_economia.py`): jogadores por semana, coleta por hora, dungeons (500 por entrada), compra de
matéria-prima pela cidade, lista, imposto, projetos, ataques e prêmio. Saída: moeda criada e destruída por semana,
renda do jogador comum (alvo ~40 mil por semana em ~10 h), balancete e faixa do governador.

Leitura já possível: como a cidade não cria moeda, a renda alvo vem das dungeons (40 mil = 80 dungeons de 500 por
semana), do PvP e da moeda dos brindes semanais; o simulador divide quanto vem de cada uma. Vender material à cidade só traz moeda que outros jogadores gastaram na lista.

### Primeiros resultados (2026-10-14)

`tools/sim_economia.py` com `tools/economia.json` (parâmetros "proposta"): 4 cidades com governadores ativo, médio,
ganancioso (compra barato, vende caro, imposto 10%) e ausente; 10, 50 e 200 jogadores por região; 12 semanas.

1. **Inflação.** Moeda criada ~21 mil por jogador por semana (dungeon 18,8 mil, brinde 2 mil, prêmios); destruída
   menos de 3 mil (mesas, buffs, armas, compra da cidade). A moeda acumula ~18 mil por jogador por semana. Faltam
   sorvedouros grandes fora da cidade (custo de fabricação maior, conserto de equipamento, entrada de dungeon, etc.).
2. **Renda.** 10 h com metade em dungeon (8 min, 500 cada) = ~19 mil; com brinde ~20 mil. Para 40 mil: 1.000 por
   dungeon, quase todo o tempo em dungeon, ou o PvP pagar a outra metade.
3. **Meta por movimento é manipulável.** Contando imposto e lista, o ganancioso acumula balancete e empata com o
   ativo; contando só gasto, perder ataque e consertar vira "movimento". Proposta: meta = matéria-prima comprada para
   evoluir e manter + projetos novos + nível médio das mesas; conserto e o que entra no balancete não contam.
4. **Tamanho da região.** A necessidade de material da cidade não cresce com os jogadores; dividida por jogador, a
   meta fica pequena em região cheia. Proposta: manutenção das mesas proporcional ao uso (fabricações na semana),
   e meta sem dividir por jogador, comparando cidades de tamanho parecido.
5. **Balancete inicial de 150 mil** sustenta um governador ativo (fica em ~60 mil na semana 12 com 50 jogadores); o
   médio chega perto de zero. Ataques com variação de ±30% derrubam mesas mesmo com defesa justa: governador precisa
   de margem (~30%).

### Segunda rodada (2026-10-14, dono: dungeon paga 1000, sem custos novos)

Mudanças: meta = matéria-prima comprada para evoluir e manter (conserto não conta) + níveis alcançados pela primeira
vez + buffs + nível médio das mesas; manutenção das mesas proporcional ao uso; faixa comparada com a média das cidades
que não ficaram no tenebroso; governador ativo mantém 30% de folga na defesa.

| Jogadores por região | Ativo | Médio | Ganancioso | Ausente |
|---|---|---|---|---|
| 10 | 54,6 mil/semana | 32,1 mil | 11,3 mil | 5 mil (tenebroso) |
| 50 | 54,2 mil | 27,9 mil | 12,5 mil | 5 mil (tenebroso) |
| 200 | 28,3 mil | 28,8 mil | 27,9 mil | 5 mil (tenebroso) |

- Renda do jogador comum: ~40 mil por semana (dungeon a 1000, metade do tempo; brinde 2 mil). Bate com o alvo.
- Até 50 jogadores, o ativo fica perto dos 50 mil pedidos, o ganancioso cai para o ruim e o ausente é tenebroso.
- Com 200 jogadores a cidade do ativo chega ao nível 3 em todas as mesas em poucas semanas e o balancete passa de 1
  milhão (imposto de muita gente) sem ter onde gastar: região cheia precisa de mais objetivos (mais níveis, buffs ou
  defesa que crescem com a população).
- Moeda: criada ~40 mil por jogador por semana, destruída ~0,8 mil. O Refinar GS consome lingotes, não moeda; a
  demanda segura o valor dos lingotes, mas a quantidade de moeda cresce ~39 mil por jogador por semana.

## Ordem

1. Simulador e números (faixas já em `tools/precos_cidade.json`).
2. ~~Backend~~ (feito 2026-10-14): regiões, cidade, balancete, estoque, preços, limites por personagem, log, compra
   e venda da cidade, virada da semana (manutenção, meta, prêmio, tenebroso), ataques a cada 2 dias, imposto no
   `state`. Migration 0013, `backend/src/routes/city.rs`, regras em `tools/governanca.json`, testes
   `cidade_compra_precos_limites_e_projetos` e `cidade_semana_tenebroso_e_venda_obrigatoria`. Evoluir mesa ainda é
   imediato (as 24 h do jogo ficam para quando houver tela).
3. Servidor de jogo: NPC da cidade (operações gravadas, `NWOps`), imposto, nível e buffs nas mesas, região por mapa.
4. Telas (estilo `NWSlate`): NPC da cidade, painel do governador (preços, imposto, balancete, estoque, projetos,
   defesa, ataques, metas), compra e venda da cidade.
5. Ajuste dos números; testes do dono.

Fora desta esteira: dungeon simples (`docs/plano-dungeon.md`), PvP.
