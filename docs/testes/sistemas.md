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
45. Arrastar da bolsa para "Soltar itens" (à direita, sem armazém aberto) pede confirmação e destrói a pilha.
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
53. Fabricar arma ou ferramenta do nível 50 em diante: tela de resultado do jogo (docs/analise-re/craftroll-equipamento.md):
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
61. Com `tools/backend_dev.sh` no ar, Play: o log diz "Play com backend: Exilado (editor)". Pegar itens, andar, parar o
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

