# Testes pendentes (G2 e telas de 2026-10-12)

Já importado (2026-10-12): texturas das telas (etapa 09) e javali/lobo (etapa 12). Abrir o editor, `L_SouthWindsward`,
Play. Para cada item: print se algo estiver errado (de preferência com o console `stat fps`).

## G2 — terreno, água, cenário

1. ~~**Personagem voando.**~~ **Resolvido 2026-10-12:** era o `ChaoSeguranca` (plano invisível do fix_city) acima dos vales; agora fica 10 m abaixo do terreno mais baixo. Original: Andar pelas bordas da cidade e pelos morros fora dela. Onde o personagem ficar acima do
   chão (ou atravessar), print + posição (F10 > o HUD de diagnóstico mostra; ou `nw.HudDebug 1`). Dizer se é sobre
   terreno, rocha, ponte ou ruína.
2. **Pedregulho flutuando.** Mesmo caso para pedras/coletáveis acima do chão: print e posição.
3. **Colisão do cenário.** Encostar em penhascos, ruínas e cercas fora da cidade: atravessa? fica preso?
   Se prender, testar Esc > Desprender (volta 5 s depois).
4. **Água.** (2026-10-12: tiradas as placas da cortina da cachoeira soltas no ar, a ~200 m a leste da praça, e
   descidos os trechos do nível de cima que cobriam o de baixo.) Rio e lagos aparecem? Altura certa (não flutua, não fica enterrada)? Cachoeira visível?
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

## G3 — criaturas (javali e lobo)

**Passo manual, uma vez** (o resto já está importado): abrir `/Game/NW/Creatures/ABP_NW_Creature` (ABP de modelo,
classe NWAnimInstance) e montar o AnimGraph:
`Blend Space Player` → `Slot 'DefaultSlot'` → `Output Pose`. No Blend Space Player, expor o pino *Blend Space* e
ligar à variável **Locomotion**; eixo X = **TurnRate**, eixo Y = **Speed**. Compilar e salvar.

16. **Nascem perto de você.** F10 > Ir para criatura (põe você a 25 m de um spawner de javali ou lobo). Javalis e lobos aparecem nos POIs a até
    60 m e somem quando você se afasta 90 m. Andam pelos waypoints e param nas tarefas (pastar...).
17. **Combate.** Chegar perto: perseguem e atacam (javali: chifrada e investida; lobo: mordidas e bote). Indicador de
    golpe, números de dano. Fugir para longe (40 m da casa deles): desistem e voltam.
18. **Morte.** Morrer para um: a tela de morte diz "Javali"/"Lobo Grisalho".
19. **Esfolar.** Matar e chegar perto da carcaça: cartão "Esfolar Javali" (sem faca: pede a faca). E: animação de
    esfolar, saque do jogo (carne, couro) e "+95 Esfolamento" no aviso.
20. **Escala e chão.** Tamanho certo (javali 1,4×)? Pé no chão ou flutuando/enterrado? Print se algo estiver estranho.

## Rodada 2026-10-12 (depois de compilar)

21. **Voo (F10 > Voar).** Frente segue a câmera (olhar para cima sobe), Espaço sobe, Ctrl desce; atravessa terreno e
    casas. Com Correr rápido ligado voa 4× mais rápido. Desligar no ar: cai normal.
22. **Empurrão.** Apanhar de javali e lobo várias vezes, inclusive em descida: empurra no máximo uns metros, colado ao
    chão, sem voar.
23. **Som das criaturas.** Javali: rugido e whoosh na chifrada, bufo e patas antes da investida, dor e impacto de
    carne ao apanhar, morte. Lobo: mordida no bote, pulo no salto, alerta ao ver você, dor, morte. Apanhando de bicho
    não toca mais o impacto de espada em armadura (só a sua voz de dor). Passos dos bichos ainda sem som (sabido).
24. **Peso do golpe.** Tranco curto na câmera ao acertar e ao apanhar (mais forte quanto maior o dano). Dizer se está
    forte/fraco demais.

## Rodada combate: arena, regras do jogo, arco e rapieira (2026-10-12)

**Passo manual, uma vez (arco):** o arco atira andando, com animações só do tronco. No `ABP_NW`
(`/Game/NW/sword_shield/ABP_NW`), AnimGraph:

1. Na saída do `Slot 'DefaultSlot'`, criar `New Save cached pose` chamado **Base**; ligar o Slot nele.
2. Dois `Use cached pose 'Base'`.
3. Nó `Slot`, nome **UpperBody** (no Details, Slot Name; se não estiver na lista, Window > Anim Slot Manager > Add
   Slot `UpperBody` no `DefaultGroup`). Source = um dos `Use cached pose 'Base'`.
4. Nó `Layered blend per bone`: Base Pose = o outro `Use cached pose 'Base'`; Blend Poses 0 = saída do Slot UpperBody.
   No Details: Layer Setup > 0 > Branch Filters > + > Bone Name **Spine1**, Blend Depth 0; marcar
   **Mesh Space Rotation Blend**.
5. Saída do Layered blend → `Output Pose`. Compilar e salvar.

Sem esse passo o arco atira, mas o tronco não anima (a flecha sai mesmo assim).

**Arena** (`L_Arena`: abre sozinha no Play; ou F10 > Ir para a arena):

25. **Cenário.** Muros e arquibancada da arena da Legião, braseiros acesos, estandartes, rochas por fora, chão de
    terra. FPS parado no centro e olhando o muro. Print se alguma peça estiver fora do lugar (afundada, voando,
    girada) ou se o chão ficar estranho.
26. **Zonas.** Pista de tiro ao sul (placas 10 a 50 m e bonecos), bonecos de treino ao norte, travessia junto ao
    muro norte, círculo de duelo a oeste com um Lost dentro (entra no círculo: ele vem).
27. **Ondas (F10 > Arena).** Javalis: de longe bufam e cavam antes da investida (dá para esquivar), de perto
    chifram. Matilha: cercam pelos lados e atacam um de cada vez, saltam de longe. Lost: rodeia, entra com golpes e
    recua. Duelista: levanta o escudo e esquiva quando você ataca. Arqueiro: fica longe, recua se você chega,
    Tiro Evasivo quando encostam. Duelista de rapieira: combos, Tondo, investida. Onda mista.

**Regras do jogo** (docs/combate-regras-jogo.md):

28. **Reação.** Em outro jogador, o golpe leve da espada não interrompe (só estremece); o pesado sim. Bonecos
    reagem a tudo (resistência 0).
    Contra Lost: golpe leve às vezes não interrompe (resistência a stagger 20); depois de uma reação, 2 s em que
    leves não interrompem. Contra javali/lobo o leve interrompe.
29. **Crítico.** Número dourado e grande. Bater pelas costas é sempre crítico.
30. **Empurrão.** Começa forte e freia (antes era constante); alguns golpes empurram para o lado.
31. **Tremida de câmera.** A do jogo é bem pequena (1–2 cm). O tranco extra do port continua (console
    `nw.HitKick 0` desliga; `nw.ShakeScale 2` dobra a do jogo). Dizer qual fica melhor.
32. **Stamina.** Regenera 33/s; com a guarda levantada, 10/s; exausto só sai com a barra cheia.
33. **Velocidade das animações.** Pesado da espada e giratório um pouco mais rápidos (como no jogo).

**Arco** (F10 > Trocar par de armas até "Bow + Rapier"; tecla 1):

34. Clique sem mirar: tiro rápido. Botão direito: mira (câmera chega perto, anda devagar). Com a mira, segurar o
    esquerdo puxa; soltar antes de 0,5 s = tiro normal, depois = carregado (mais forte). Depois do tiro recarrega
    sozinho e volta à mira.
35. Flecha cai depois de ~5 m de reta (mirar acima de alvo longe). Acertar a cabeça de um boneco: dano maior.
36. Habilidades: Tiro Rápido (3 flechas, a última empurra), Tiro Evasivo (salta para trás e atira), Tiro Penetrante
    (atravessa vários). Trocar no K: Disparo Venenoso (dano por segundo), Flecha Explosiva (explode em 2,5 m),
    Chuva de Flechas (área de 4 m onde a câmera aponta).

**Rapieira** (tecla 2 no par "Bow + Rapier"):

37. Combo de 3; o segundo golpe acerta duas vezes. Segurar o ataque: estocada carregada em 0,667 s.
38. Esquiva é um passo curto. Q Tondo (sangra: números de dano a cada segundo, até 3 acúmulos). R Ripostar: se
    apanhar no primeiro segundo, contra-ataca e atordoa (o atordoamento acaba no primeiro dano). F Evadir: passo
    rápido que sai de qualquer ação; atacar logo depois sai um golpe rápido.
39. No K, trocar para Brandir e Terminar: depois do Brandir, atacar faz o Terminar, que consome o sangramento
    (número grande). Rajada: 5 estocadas. Fleche: avanço longo.

**Correções depois do primeiro teste (2026-10-12 noite)**

40. Arco mirando: a segunda flecha carrega segurando o botão (puxa a partir de 0,733 s da recarga, como no jogo);
    clique durante a recarga fica guardado e sai em 0,667 s, sem precisar clicar duas vezes. Clique logo ao
    levantar o arco também sai.
41. Arco sem mirar com o botão segurado: tiros rápidos seguidos (1 por segundo).
42. Escudo: com o par arco + rapieira some; com espada no par, volta. Arco guardado vai nas costas (print se ficar
    torto).
43. Reações por força do golpe: golpe leve faz um tranco pequeno (R1), pesado um maior (R3), golpes de força 4
    derrubam e o personagem levanta. Habilidades encadeiam nas janelas do jogo (ex.: Golpe de Escudo logo depois
    do combo só quando o jogo deixa).
44. Golpe "puxa" para o inimigo à frente (até ~2,75 m, cone de 45°) durante a janela de homing.

**Segunda correção do arco (2026-10-12 noite)**

45. Mira → puxar → soltar → recarga → mira: sem o arco baixar entre uma e outra (encadeamento começa quando a anterior
    começa a sair; transições de 0,1 s). Levantar o arco não trava mais a puxada.
46. Flecha na mão direita enquanto mira e puxa; some no disparo e volta na recarga.
47. Flecha em voo com rastro luminoso de 2,5 m (no jogo é partícula).
48. Habilidades com efeito: Flecha Explosiva (clarão laranja), Disparo Venenoso (nuvem verde de 3 m por 6 s que
    envenena quem entra), Chuva de Flechas (flechas caindo na área antes do acerto).

## IA das criaturas pelas árvores do jogo (2026-10-12 noite)

Lobo e javali agora seguem a árvore de comportamento do jogo (`docs/ia-criaturas.md`). Arena: F10 > Arena > Matilha /
Javalis (já nascem em combate). Guardaventos: F10 > Ir para criatura.

49. Lobo te vê só de frente e a até 12 m; ao ver, rosna (alerta) e só depois ataca. Os outros lobos a 7,5 m entram junto.
50. Lobo: de longe corre muito rápido (9 m/s) e dá bote ou corre e morde; a média distância às vezes anda de lado
    para se reposicionar; de perto morde, faz combo de duas mordidas ou pula para trás e dá o bote.
51. Javali: passivo. Passa por ele e não ataca; bate nele e ele (e os javalis a 20 m) vêm. Investida com preparo.
52. Fugir para longe (70 m da casa deles): voltam correndo e recuperam a vida.
53. Céu limpo em qualquer mapa: sem formas grandes no ar nem sombra delas no chão. A capa e a saia da armadura ficam
    no corpo do personagem.
54. Golpes sem pausa no acerto (o jogo não tem hit stop; o tranco de câmera do `nw.HitKick` continua). Reação a golpe
    dura 0,2 s + HitStun/30 e a animação termina junto, mais rápida nos golpes leves. Empurrão com reação: rápido no começo,
    para quando a reação acaba.
