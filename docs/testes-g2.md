# Testes pendentes (G2 e telas de 2026-10-12)

Antes de tudo, com o editor fechado: `tools/ue_import_all.sh 09` (texturas das telas novas). Depois abrir o editor,
`L_SouthWindsward`, Play. Para cada item: print se algo estiver errado (de preferência com o console `stat fps`).

## G2 — terreno, água, cenário

1. **Personagem voando.** Andar pelas bordas da cidade e pelos morros fora dela. Onde o personagem ficar acima do
   chão (ou atravessar), print + posição (F10 > o HUD de diagnóstico mostra; ou `nw.HudDebug 1`). Dizer se é sobre
   terreno, rocha, ponte ou ruína.
2. **Pedregulho flutuando.** Mesmo caso para pedras/coletáveis acima do chão: print e posição.
3. **Colisão do cenário.** Encostar em penhascos, ruínas e cercas fora da cidade: atravessa? fica preso?
   Se prender, testar Esc > Desprender (volta 5 s depois).
4. **Água.** Rio e lagos aparecem? Altura certa (não flutua, não fica enterrada)? Cachoeira visível?
5. **Chão.** Hoje é uma textura só (sabido). Só avisar se tiver buraco ou emenda visível entre pedaços.
6. **Árvores da cidade.** Copas com placas coloridas (sabido); avisar se apareceu algo pior.
7. **Desempenho.** FPS parado na praça, correndo para fora da cidade e olhando o vale de cima de um morro.
   Engasgos ao virar a câmera?
8. **Coleta.** F10 > Ir para minério; coletar; o aviso de saque mostra o item e "+64 Mineração" com a barra.
   Coletar uma árvore grande: deve pedir nível (só com personagem do servidor; no Play do editor fica livre).

## Telas novas

9. **Inventário (Tab).** Tooltip num item; numa arma da bolsa com outra equipada aparecem os dois lado a lado.
   Barra de navegação no alto (Inventário / Personagem / Mapa) troca de tela.
10. **Janela K.** Abas Maestria em Armas / Hab. Mercantes / Atributos. F10 > Ganhar 1 nível: banner de nível,
    barra de XP embaixo, selo de pontos na aba Personagem; em Atributos, +/− e Confirmar. F10 > Ofícios +1 nível:
    círculos da aba Hab. Mercantes.
11. **Mapa (M).** Arrastar, roda do mouse, botões o/+/−, POIs com nome ao aproximar, botão direito põe marcador
    (aparece na bússola com a distância), ícone do jogador virado para onde olha.
12. **Bússola.** POIs próximos aparecem na tira no rumo certo (conferir com o mapa).
13. **Configurações (Esc).** Aba Atalhos: trocar uma tecla, Esc cancela, Restaurar Padrões; corrida automática
    com "=". Jogo > Números de dano empilhados. Vídeo > Brilho.
14. **Combate.** Apanhar de um Lost pelos lados e por trás: seta e brilho vermelho do lado certo.
15. **Avisos.** Encher a bolsa: faixa amarela "bolsa cheia" no alto, não no chat.
