# Testes: sistemas e Queda Eterna

Esteira 4 (`docs/esteiras.md`). Mapa `L_QuedaEterna` (Guardaventos para o que já existe lá). Para cada item, print
se algo estiver errado.

## Cidade

1. Abrir o editor em `L_QuedaEterna`: a capital de Queda Eterna aparece inteira, com texturas e sobre o terreno, sem
   peças cinzas nem flutuando.
2. Play (PIE): o personagem nasce numa rua livre da cidade (fora de estátua ou parede), anda pelas ruas e sobe
   escadas sem cair do mapa.

## NPCs

3. Em `L_QuedaEterna`, Play: os NPCs da cidade aparecem (magistrado, comandante, alquimista, taverneira, guardas,
   ambientes), de pé no chão, com roupa e textura, cada um com a animação parada dele em laço.
4. Encostar num NPC: ele bloqueia a passagem como uma pessoa (não dá para atravessar).
5. Sobre os NPCs não aparece nome. Só o NPC sorteado da semana de cada ofício tem o ícone "!" do jogo (no máximo 6
   na cidade, um por ofício); os outros não têm ícone.
6. Chegar a ~2 m de um NPC com nome, de frente: aparece o cartão do jogo (anel, faixa com o nome e "[E] Conversar");
   apertar E: o NPC vira para o personagem. Perto de um coletável sem NPC em foco, o cartão e o E continuam sendo os
   da coleta.
7. Ao abrir o editor, o Log de Mensagens não mostra mais "SK_NpcQuedaEterna_Skeleton is missing bones" (Ou Shen).
8. Ir até um NPC com "!", E: o cartão diz "[E] Receber"; aparecem no aviso de saque os itens iniciais do ofício
   (ex. guarda: ~1000 minério e 500 pedra). E de novo: o brinde semanal (um material e um argueiro de 100 a 300). Depois disso o
   ícone vira relógio e o cartão volta a "[E] Conversar".
9. Sair e entrar de novo no servidor dedicado com backend: o NPC já recebido continua com relógio e não entrega de
   novo.

## Refino

10. F10 > "Bolsa de dev (peso)": o peso no inventário passa a ".../ 10000000"; de novo volta a 1000.
11. As 5 estações de refino da cidade (fundição, carpintaria, curtume, tecelagem, cantaria): perto de cada uma aparece o
    cartão com o nome do jogo ("Fundição Escala 5"...) e "[E] Refinar".
12. E na fundição: abre a tela de refino (lista à esquerda, painel à direita). Com minério de ferro na bolsa, "Lingote
    de Ferro" mostra quantos dá para fazer; escolher, ajustar a quantidade (−, +, Máx.) e Refinar: some o minério
    (4 por lingote), aparecem os lingotes, as moedas descem pela taxa e o XP de Fundição sobe.
13. Receita sem material: os números ficam vermelhos e o botão Refinar fica apagado. Esc fecha a tela.
14. Com qualquer tela aberta (inventário, janela K, mapa, refino), clicar em botões, abas e na barra de navegação: o
    personagem não ataca. Inclui o clique que troca ou fecha a tela.

## Tutoriais de nível

15. Queda Eterna não tem mais os 4 bonecos de treino na praça. Derrotar 10 inimigos com a espada (Lost do F10):
    aviso "Missão concluída: Espada e escudo...", sobe 2 níveis (banner). F10 > Missões mostra 1/13.
16. Trocar para a machadinha e derrotar 10: mais 2 níveis. Coletar 10 vezes num ofício (Guardaventos): mais 2.
17. Refinar 1 item em cada estação de Queda Eterna: 1 nível por estação.
18. Sair e entrar no servidor com backend: o progresso das missões continua (F10 > Missões).

## Armazém

19. Despensa de Queda Eterna (perto da praça): cartão "Despensa · [E] Abrir". E abre a tela com bolsa e despensa.
20. Clicar num item da bolsa: a pilha vai para a despensa e o peso dos dois lados muda. Clicar na despensa: volta.
21. Encher a despensa acima de 1000 kg: aviso "armazém cheio" e o item fica na bolsa.
22. Com o servidor dedicado e o backend: guardar, sair, entrar de novo e abrir a despensa: os itens continuam lá.
23. Tela de refino (fundição) com o layout do jogo: barra de título "FUNDIÇÃO ESCALA 5" com moedas; árvore Aprendido >
    Recursos Refinados > Metais (clicar no cabeçalho fecha/abre); painel central com escala, ícone, nome, "tenho/preciso";
    painel direito com nível do ofício, chance de item extra, quantidade (−/+/número), requisitos verdes/vermelhos,
    botão CONFECCIONAR azul animado, taxa e "+N Fundição"; rodapé ENTER/ESPAÇO/ESC. Comparar com o print do jogo.
24. Confeccionar: o fundo escurece, o total sobe de 0 em ~1 s, aparecem Base e Bônus; clique volta à lista.
25. Refino: segurar e arrastar o marcador da barra de quantidade: o número acompanha o mouse. Segurar + ou −: depois de
    meio segundo repete, e acelera depois de ~2 s.
26. Refino: Espaço na escolha põe a quantidade máxima; Espaço durante a contagem pula para o fim; Espaço no resultado
    volta à lista. O personagem não pula.
27. Refino: deixar a tela aberta ~10 s olhando o botão CONFECCIONAR: o brilho azul corre liso, sem piscar.
28. Refino na fundição: a árvore começa por Recursos Refinados > Metais, Metais Preciosos, Carvão, Prospecção; Conversão
    de Material vem depois. O título fica "FUNDIÇÃO ESCALA 5" (com Ç e Ã maiúsculos).
29. Refino: abaixo do CONFECCIONAR aparece "Taxa de Queda Eterna: 0,26" para 26 lingotes e o XP como "+N Fundição". Moedas
    na barra de título, no inventário e no aviso de saque aparecem com duas casas (50 moedas do personagem novo = 0,50).
30. Refino de 1 Lingote de Ferro: a tela mostra "+44 Fundição" (11 × 4 minérios) e o XP de Fundição sobe 44 na janela K.
31. Refinar uma receita pela primeira vez (ex. Lingote de Aço): a tela mostra o XP triplicado e a Fundição sobe esse
    valor. Na segunda vez, volta ao normal. Sair e entrar com backend: continua normal (a receita fica marcada como feita).

## Loja

32. Inventário e saque: não existe mais poção T1–T4; o personagem novo começa com 5 Poções de vida T5 e o Lost às vezes
    deixa 1 Poção T5. Personagem antigo que tinha T1–T4 entra sem elas.
33. Em Queda Eterna, o mercador (NPC ambiente de mercador) mostra o cartão "Loja · [E] Comprar". E abre a loja com a
    grade: Poção de vida T5 (2,00) e os 5 reagentes de refino (0,20).
34. Clicar num cartão abre o detalhe; arrastar a quantidade e clicar no botão com o preço: as moedas descem, o item
    aparece no aviso de saque e na bolsa. O X volta à grade; Esc fecha.
35. Comprar 100 de um reagente: o cartão ganha o relógio vermelho com o tempo até terça 05:00, e o detalhe mostra
    "Próxima recarga". Sair e entrar com backend: o limite continua.
36. Sem moedas suficientes: o botão fica cinza e aparece "Fundos insuficientes".
37. F10: o menu aparece em duas colunas, inteiro na tela (Extras e Depuração na coluna da direita, com rolagem se
    precisar). Extras > "Ganhar 100,00 moedas": as moedas sobem 100,00 e o botão mostra o saldo.
38. Em Queda Eterna, entrar na igreja ou na taverna: em 1 a 2 s o interior clareia (adaptação do olho); ao sair, a rua
    volta ao brilho de antes, sem ficar estourada.
39. O mercador tem o ícone de loja sobre a cabeça, visível de longe e através das paredes.
40. Conversar com um NPC (E): ele vira de frente para o personagem, não de lado. Parado, antes de conversar, cada NPC
    continua na mesma pose e direção de antes.
41. Tab: a bolsa mostra as seções do jogo em duas colunas (peças quadradas e redondas, tier, quantidade). Os 4 botões
    de ordenação reordenam; clicar de novo no ativo inverte. A seta recolhe a seção.
42. Arrastar uma arma da bolsa para o espaço dela (ou para qualquer ponto do lado do equipamento) equipa; arrastar do
    equipamento para a bolsa desequipa. Os espaços compatíveis acendem durante o arrasto.
43. No armazém: arrastar entre bolsa e armazém, Shift + clique, a seta da seção (move a seção inteira) e "Pegar tudo".
44. Ctrl + clique numa pilha: o diálogo começa na metade; Mover/Pegar leva só a parte escolhida; sem armazém, Soltar
    pede confirmação e destrói só a parte escolhida.
45. Arrastar da bolsa para "Soltar itens" (à direita, sem armazém aberto) solta a pilha numa bolsa no chão (teste 72).
46. Buscar: digitar no campo filtra a lista pelo nome, sem acento ("fundicao" acha Fundição); clicar fora devolve o
    teclado ao jogo.
47. Forja, oficina e repositório arcano abrem a tela; receita de arma ou ferramenta mostra a faixa de Gear score e de
    Qualidade (ou o aviso de treino) e o botão Fabricar, sem a quantidade.
48. Abaixo do nível 50 do ofício, Fabricar é treino: gasta material, dá XP e não gera item.
49. F10 "+50 níveis nos ofícios de fabricar" e "Materiais de fabricar", Fabricar: material e taxa saem e o item aparece na
    hora na bolsa. Tooltip com atributo, qualidade, perks com valor e ícone, slot de gema vazio (moldura do soquete), "Vincula ao equipar" e "Feito
    por" quando a qualidade passa de 80%. Bolsa cheia: não fabrica e avisa.
50. Equipar o item fabricado: o tooltip passa a "Vinculado". Sair e entrar de novo: o item continua igual.
51. Na forja, "Melhorar itens" (canto de cima): lista os itens fabricados da bolsa feitos ali, com GS, atributo e
    qualidade; botões Sortear atributo, Sortear GS e Desvincular (só se vinculado), com o ícone do material e
    "custo / tem" embaixo (vermelho se faltar). Clicar abre a tela de resultado com o item depois da mudança. Cada um muda
    só a sua parte; Sortear GS conta do GS antigo ao novo, dentro da faixa da receita (sobe ou desce).
52. As receitas de arma e ferramenta pedem só lingote, tábua e couro do tier do item.
53. Fabricar arma ou ferramenta do nível 50 em diante: tela de resultado do jogo (re-local/craftroll-equipamento.md):
    GS contando até o do item, raridade, dano base, atributo, as perks uma a uma com ícone e o slot de gema vazio; nome
    na cor da raridade com a qualidade embaixo. Clique durante a animação termina; outro clique volta à mesa. Item com
    no máximo 1 perk de habilidade.
54. Item com qualidade de 80% ou mais: "Feito por <nick>"; sair e entrar de novo, guardar e retirar do armazém: continua
    igual (o número de série vai junto e o nw-core não aceita o mesmo item em dois lugares).
55. F10 "Gemas e Joalheria 200": na estação de equipamentos (Alfaiataria) a lista tem Gemas Lapidadas; lapidar uma Ônix
    Imperfeita gasta a bruta e o reagente e dá XP de Joalheria. Na forja aparecem as 2 cápsulas de vidro rúnico; na
    cantaria, os 14 vidros (cápsula + gema Pura).
56. Tooltip de uma gema na bolsa: os efeitos do port (ex.: Diamante Brilhante "+2% de resistência física", "+2% de
    resistência mágica") e "Acopla em armaduras e escudos" ou "Acopla em armas", sem a descrição do jogo.
57. Arrastar uma Esmeralda para uma espada fabricada sem gema: "Acoplar Gema" com o efeito; Confirmar tira 1 da pilha e
    o tooltip da espada mostra o ícone, o nome e "+5% de chance de acerto crítico". Ônix numa espada não abre nada;
    gema em ferramenta ou em item do saque também não.
58. Arrastar outra gema para a espada que já tem gema: não abre nada e avisa "Retire a gema antes". A miniatura da
    espada na bolsa e no equipamento mostra o ícone da gema no canto de baixo à direita.
59. Shift + clique direito na espada com gema: "Retirar gema" com o custo; Confirmar devolve a gema e o slot fica vazio.
    Com a bolsa cheia, avisa e nada muda.
60. Vidro rúnico de Ônix de Punição num escudo: tooltip com +5% de resistência física e +3% de dano físico. Sair e
    entrar de novo, guardar e retirar do armazém: a gema continua no item. Item equipado também aceita gema.
61. Com `tools/linux/backend_dev.sh` no ar, Play: o log diz "Play com backend: Exilado (editor)". Pegar itens, andar, parar o
    Play e dar Play de novo: mesma bolsa, mesmos níveis e mesma posição. Com o nw-core parado, aparece "Jogando sem
    backend" e o Play segue com o kit inicial.
62. Abrir "Melhorar itens" na forja, sair e abrir uma mesa de refino: abre na lista de receitas. Voltar à forja: também
    abre nas receitas.
63. Lista de receitas da mesa: a última linha visível termina inteira acima da borda de baixo; rolar mostra o resto.
64. Fabricar uma arma: no fim do roll aparecem à esquerda Continuar a Confeccionar, Equipar e Sair da Estação. Equipar
    põe o item no espaço dele e volta à mesa; Sair fecha a estação; Continuar (ou clicar fora) volta à lista. Ferramenta
    mostra "Equipar Ferramenta".
65. Sons: abrir e fechar a mesa, clicar numa receita e em Fabricar/Refinar tocam o som do jogo; no roll de arma toca a
    raridade, um som por perk e o da gema; no refino, o de completar. Clicar para pular não dispara todos de uma vez.
66. F10 "Armaduras e joias (níveis e materiais)": na forja aparecem as armaduras de placa; na estação de equipamentos as
    de couro e tecido e as joias. Cada receita pede só lingote, couro e tecido do tier (joia: um lingote precioso).
67. Fabricar uma peça de armadura: tela de resultado com o painel Resistência (Físico e Mágico, base → valor do item),
    atributo, 3 perks de armadura e o slot de gema; joia sem slot e anel com 2 perks.
68. Equipar cada peça e cada joia no seu espaço (arrastar, clique direito ou Equipar no fim da fabricação). Tooltip com
    a resistência e comparação com o equipado; GS do painel muda. Sair e entrar: tudo continua equipado.
69. Gema de armadura (Ônix, Opala, Diamante, Safira, Jaspe) acopla na armadura; gema de arma não.
70. Bolsa aberta pelo Tab: a roda do mouse rola a lista.
71. A bolsa aceita até 500 pilhas; o peso dá a sobrecarga. Com os 500 espaços ocupados, fabricar, coletar e o F10
    continuam: o que não cabe aparece numa bolsa no chão, com o aviso "Bolsa cheia: o item foi para o chão".
72. Arrastar uma pilha para "Soltar itens": sai da bolsa e aparece um saco aos pés com o cartão "N itens" e [E] Pegar
    tudo. E devolve tudo para a bolsa. Ctrl + clique > Soltar solta só a parte escolhida.
73. O saco some 3 minutos depois de solto (era 2 até 2026-10-14). Soltar mais de 1000 kg no total recusa com "Você já tem 1000 kg de itens no
    chão".
74. Mesa de fabricar > Melhorar itens: cada item mostra os materiais da receita (lingote, tábua, couro) com a quantidade
    na bolsa; clicar num muda o ícone e o custo embaixo dos botões. Sem material suficiente, o custo fica vermelho.
75. Botão "Refinar GS" com só "GS a–b" embaixo. Refinar várias vezes: sobe e desce perto de metade das vezes, quase
    sempre 1 ou 2 pontos, nunca fora de a–b. Embaixo de Sortear atributo: "25% cada".
76. Soltar itens no chão e apertar [E] no saco ("Abrir"): abre o inventário com a janela "Itens Derrubados" à direita,
    ícone da bolsa, aviso de 3 minutos, "N itens", peso / 1000 e a lista dos itens soltos.
77. Na janela da bolsa: arrastar um item para o inventário, clique direito e Ctrl + clique > Pegar com parte da pilha
    passam só aquilo; a seta da seção pega a seção; Pegar Tudo pega o resto. Arrastar da bolsa para a janela do saco
    não faz nada.
78. Esvaziar o saco com a tela aberta: a janela some e fica só o inventário; o saco some do chão. Saco esquecido some
    em 3 minutos.
79. Personagem novo entra em Queda Eterna: aviso "Cidade conhecida: armazém +1000 kg". Abrir a despensa: peso / 1000.
    Sair e entrar: continua conhecida, capacidade igual.
80. Guardar itens numa despensa e abrir a outra despensa da cidade: os mesmos itens (armazém compartilhado). Itens
    guardados antes desta versão aparecem na Aba 1.
81. Abas: 5 abas embaixo da busca. Trocar de aba mostra só os itens dela; guardar vai para a aba aberta; arrastar da
    bolsa para outra aba guarda nela; arrastar do armazém para outra aba passa a pilha. Pegar Tudo pega só a aba aberta.
82. Clique duplo na aba aberta: campo de texto; digitar um nome e Enter. Sair e entrar: o nome continua.
83. Peso acima da capacidade (ex. pelo F10 se houver, ou capacidade menor): não guarda ("armazém cheio"), retira normal.
84. Play, guardar e retirar do armazém, fabricar um item, parar o Play. `tools/item_log.py --personagem Exilado`: cada
    ação aparece com o motivo (storage, craft:<receita>), o local (bag, storage:<aba>) e a quantidade +/−.
85. Ficar parado 10 s no Play: o log não ganha linhas novas (salvamento periódico só grava diferença).
86. Parar o Play logo depois de soltar um item no chão (antes de 5 s): ao voltar, o item não está na bolsa nem no chão
    (o chão ainda não é salvo: etapa 2); no log aparece a saída da bolsa com o motivo do salvamento.
87. Play do editor: F10 abre e os botões funcionam; cada uso aparece em `tools/item_log.py --personagem Exilado` com o
    motivo `dev:<comando>` (quando mexe em item ou moedas).
88. Teste em rede com uma conta sem admin: F10 não abre; com `tools/linux/rede.sh admin <conta>` e entrando de novo, abre.
89. Soltar um item no chão, parar o Play e dar Play de novo: o saco está no mesmo lugar, com os itens e o tempo que
    sobrava; pegar devolve à bolsa. `tools/item_log.py --personagem Exilado`: `drop` (bag −, ground:<saco> +) e
    `ground_take` (ground −, bag +).
90. Soltar, esperar os 3 minutos com o Play aberto: o saco some; parar e voltar: não volta (no log: `ground_expired`).
91. Com o nw-core desligado no meio do Play (`tools/linux/backend_dev.sh down`): fabricar, guardar e soltar mostram "Não foi
    possível gravar; nada mudou" e nada sai da bolsa; ligar de novo (`tools/linux/backend_dev.sh`) e repetir: funciona.
92. Fabricar com a bolsa cheia: o item vai para um saco no chão, que volta depois de parar e dar Play de novo.
93. Fabricar, refinar e melhorar várias vezes seguidas e rápido: tudo entra na ordem, sem item a mais nem a menos
    (comparar com o log).
94. Coletar alguns minérios e matar uma criatura: o item e o aviso de saque aparecem normalmente; no log, `gather` e
    `loot:<tabela>` com só o que entrou (sem regravar a bolsa).
95. Ficar parado 10 s: o salvamento periódico não grava nada da bolsa (nenhuma linha nova no log).
96. Comprar na loja do mercador, encaixar e retirar uma gema, destruir um item: cada um aparece no log (`shop:...`,
    `socket`, `unsocket`, `destroy`).
97. Pegar o brinde semanal de um NPC: os itens entram e o log mostra `npc_reward:...`; tentar de novo: não entra nada.
98. Queda Eterna: [E] no Entreposto Comercial abre a tela (spinner até o livro chegar) com Comprar, Vender e
    "Meus Pedidos n/50". Esc ou o X no canto fecham. Moedas e preços com o ícone de moeda. Mouse sobre o nome de um
    item na tabela: quadro com atributo, qualidade e vantagens. Longe do posto, nada abre.
99. Vender: só aparecem itens fabricados, gemas lapidadas e refinados finais (Asmodeum e outros T5.1); minério e
    lingote não. Escolher um item: painel com atributo, qualidade, vantagens com valor; o campo de preço já com o menor
    preço anunciado, se houver. Preço, duração, Enter ou Fazer Pedido de Venda: o item sai da lista na hora e o pedido
    aparece em Meus Pedidos com spinner, que some quando o servidor grava.
100. Meus Pedidos > Cancelar: a linha apaga com spinner e some; o item volta no armazém compartilhado.
101. Comprar (com 2 jogadores, `tools/linux/net_test.sh` ou duas contas): a venda de A aparece para B com o nome de A em
     Vendedor; comprar: as moedas de B caem na hora, o item vai para o armazém de B; A reabre o posto e recebe.
102. Filtros: escala, raridade, GS mínimo, atributo, vantagem e Pode Pagar reduzem a lista; Limpar Tudo volta; clicar
     no título de uma coluna ordena.
103. Recusa: vender com o nw-core desligado (`tools/linux/backend_dev.sh down`): a linha aparece, some e o item volta à lista
     com a linha piscando em vermelho e o aviso.
104. Criar 50 pedidos: o botão de vender apaga no 50º.
105. `tools/item_log.py --personagem Exilado`: `market_post_sell`, `market_buy` e `market_credit` com o item e as
     moedas de cada ação.

Governança das cidades (docs/plano-governanca.md):

106. Queda Eterna: [E] no Quadro do Projeto de Vila abre a tela com Vender à Cidade, Comprar e Cidade; no Quadro de
     Planejamento do Território, sem governador, Resumo e Governo com o preço da cidade.
107. Vender à Cidade: escolher minério de ferro, quantidade, Vender: o minério sai da bolsa, as moedas entram na hora
     e o limite da semana sobe; o spinner some quando o nw-core grava. Pedir mais que o limite (5 mil): recusa.
108. Comprar: poção ou fundente pela lista; as moedas caem e o item entra na bolsa.
109. Governo > Comprar a Cidade (com moedas suficientes, F10 dá moedas): passa a governador; o Quadro de Planejamento
     mostra Resumo, Preços, Mesas, Defesa e Governo.
110. Preços: mudar o preço do minério de ferro e o imposto, Salvar; no Quadro do Projeto de Vila o preço novo aparece.
     Preço fora da faixa fica vermelho e não salva.
111. Mesas: com material no estoque (vendido à cidade), Evoluir a fundição: nível 1. Buff +10% XP de ofício na
     fundição: aparece em Cidade; refinar na fundição dá mais XP.
112. Fabricar numa mesa com nível: GS e qualidade mínima um pouco maiores; as moedas caem o custo + o imposto da
     cidade, e o balancete sobe o imposto.
113. Defesa: Comprar Arma soma uma arma e desconta do balancete.
114. `tools/item_log.py --personagem Exilado`: `city_sell:*`, `city_buy:*`, `city_buy_city` com itens e moedas.
115. Brinde semanal de um NPC: além do material, 200 a 400 moedas.
116. Governo, como governador depois da carência (ou com `owner_since` antigo no banco): lance inicial e Pôr em
     Leilão: aparece "Leilão", termina em 24 h; sem lance, Cancelar Leilão.
117. Com outra conta (`tools/linux/net_test.sh`): Dar Lance no mínimo: as moedas caem; outra conta cobre: o primeiro recebe
     o lance de volta (crédito ao abrir o quadro). No fim (ou `POST /internal/cities/advance` com `now` 25 h depois),
     o maior lance vira governador e o antigo recebe o lance menos 10%.
118. Quadro de Planejamento: o topo mostra o balancete da cidade (moeda grande, âmbar); Resumo sem a meta da semana;
     as moedas do jogador aparecem só em Governo (comprar a cidade, lance).
119. Mesas: cada mesa mostra a escala (começa na 2) e a cesta da próxima (estoque / pedido, vermelho se falta). Com o
     estoque completo, Evoluir: a mesa fica "Em obra" com o tempo; depois de 24 h (ou `POST /internal/cities/advance`
     com `now`), sobe de escala. Na escala 5, a próxima é "Evoluir para Nível 1", bem mais cara.
120. Fabricar numa mesa com escala 3+: GS e qualidade mínima um pouco maiores (escala 3: +1 GS; 5: +3; cada nível +1).
121. Defesa: cinco armas do forte (Balista, Repetidor, Explosivo, Tonel de Óleo Quente, Trombeta de Resiliência), escala
     1 a 3, cesta e obra de 24 h; a defesa é a soma das escalas.

