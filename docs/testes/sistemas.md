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

15. Em Queda Eterna, a uns 12 m a leste da praça, há 4 bonecos em fila. Derrotar 10 inimigos com a espada:
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
