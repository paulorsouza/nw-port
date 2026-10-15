# Testes: manopla de gelo

Sub-esteira 1a (`docs/esteiras.md`). Mapa `L_Arena`. Para cada item, print se algo estiver errado. Levantamento:
`docs/armas/manopla-gelo.md`.

## Parte 1: arma, mana, leve, pesado, bloqueio, esquiva

1. F10 > Manopla de gelo + espada, tecla 1: a luva aparece no braço direito e acompanha mão e dedos; cristal na palma.
   Tecla 2 troca para a espada (a luva some).
2. Clique rápido: lança o projétil de gelo em 0,167 s pelo centro da tela, andando (só o tronco anima). Some a 50 m.
3. Segurar o clique: aos 0,667 s sai o pesado sozinho, projétil maior. Alvo atingido fica 10% mais lento por 3 s.
4. Cliques seguidos: o próximo só sai depois da janela do jogo (~0,27 s após o disparo, buffer até 0,93 s).
5. Botão direito: pose de bloqueio da manopla; bloquear gasta stamina como as outras armas.
6. Shift com as 4 direções: passo curto de 1 mão para cada lado, invulnerável no começo.
7. Barra de mana (azul, acima da stamina) cheia. F10 > Encher mana e Mana infinita respondem.
8. Com outro jogador: ele vê a luva no seu braço e os projéteis.
9. Clicar sem parar: cada clique solto antes da janela fica guardado e o próximo leve sai quando a janela abre
   (~0,83 s); o golpe em curso não é cortado antes de lançar o projétil.
9b. Correndo (Shift segurado) e clicando: sai o leve normal (a manopla não tem golpe de corrida).

## Parte 2: habilidades

10. Q/R/F com a manopla: Tempestade, Vento Arrepiante e Estacas (padrão). Na tela K dá para trocar pela Chuva, Torre e
    Sepultado. Cada uma gasta a mana da tabela (15/20/25) e entra em recarga (barra azul desce).
11. Estacas: 7 estacas em linha de ~8 m na direção da câmera e a Estaca Poderosa no fim (empurra). Clicar o ataque
    logo depois de lançar solta a Estaca Poderosa antes.
12. Vento Arrepiante: 2 s soprando à frente, anda devagar; o boneco a até ~6 m leva vários acertos. Sem empurrão.
13. Tempestade: área azul de 5 m onde a câmera aponta (até 20 m), 5 s, dano a cada 0,33 s e inimigo mais lento.
14. Chuva: arco de caixas ~2–3 m à frente por 4 s; quem entra fica quase parado 1 s e lento enquanto dentro e 3 s
    depois de sair.
15. Torre: aparece 2 m à frente, atira no inimigo mais perto até 20 m por 15 s; outra torre apaga a anterior; recarga
    de 10 s começa quando ela some.
16. Sepultado: tumba de gelo por 10 s, sem dano recebido, sem andar, mana enchendo; ataque básico quebra (dano em
    volta, gasta 10 de mana); recarga de 30 s começa ao sair. A pose da tumba fica parada os 10 s, sem voltar ao
    idle no meio.
