# Esteiras de trabalho

Quatro frentes do port; a de armas tem duas sub-esteiras (1a, 1b). Cada uma pode ser uma sessão na mesma máquina
(mesmo checkout) ou uma pessoa na própria máquina, num branch (seção "Várias pessoas").
A engenharia reversa (RE) do `NewWorld.exe` não é uma esteira: qualquer sessão pede uma pergunta específica pela
skill `nw-re`, que roda o agente `nw-re` em segundo plano.

| Esteira | Skill | Doc de estado | Testes | Mapa de teste |
|---|---|---|---|---|
| 1. Armas | `nw-combate-arma` | `docs/combate-armas.md`, `docs/armas/<arma>.md` | `docs/testes/armas.md` | `L_Arena` |
| 1a. Manopla de gelo | `nw-combate-arma` | `docs/armas/manopla-gelo.md` | `docs/testes/manopla-gelo.md` | `L_Arena` |
| 1b. Espada grande | `nw-combate-arma` | `docs/armas/espada-grande.md` | `docs/testes/espada-grande.md` | `L_Arena` |
| 2. Coleta | `nw-coleta` | `docs/coleta.md` | `docs/testes/coleta.md` | `L_SouthWindsward` |
| 3. Montarias | `nw-montaria` | `docs/montarias.md` | `docs/testes/montarias.md` | `L_SouthWindsward` |
| 4. Sistemas e Queda Eterna | `nw-sistemas` | `docs/sistemas.md` | `docs/testes/sistemas.md` | `L_QuedaEterna` |

Todas usam também a skill `nw-esteira` (regras abaixo) e, quando faltar fonte, a `nw-re`.

## Regras comuns

### Máquina

- Trabalho pesado entra na fila: `tools/pesado.sh <comando>`. Pesado = nwbt, Blender, `build_set.py`,
  `convert_*.sh`, `UnrealEditor-Cmd`, compilação, Ghidra em lote. `build_all.sh` e `ue_import_all.sh` já entram
  sozinhos. Motivo: o `systemd-oomd` fecha programas quando dois trabalhos pesados somam pressão de memória.
- New World original fechado durante trabalho pesado.

### Editor e testes

- O editor é do dono. Compilar e importar exigem o editor fechado: peça para fechar, faça tudo que precisa do
  editor fechado de uma vez, e no fim abra o editor no mapa da sua esteira.
- Nunca rodar o jogo, o cliente ou teste automático por conta própria. Abra o editor e peça o print.
- Cada esteira tem a sua lista de testes em `docs/testes/<esteira>.md`, numerada só nela. Item novo: o que fazer e o
  que deve acontecer, numa ou duas frases.

### Código compartilhado

- Outras sessões editam o mesmo checkout ao mesmo tempo. Arquivo dos outros (tabela abaixo): não mexa; se precisar de
  algo ali, mudança mínima e descrita no relatório.
- Arquivo compartilhado: releia antes de editar, mude só o trecho necessário, não reformate.
- O C++ precisa compilar ao fim de cada mudança sua. Erro de compilação em arquivo de outra esteira: não conserte,
  avise o dono e espere.
- Conjunto do jogador: um arquivo por esteira (`tools/sets/player/armas.json`, `coleta.json`, `montaria.json`),
  juntados ao `tools/sets/sword_shield.json` (personagem, pulo, queda, morte) pelo `tools/setspec.py`. A mesma
  montagem em duas partes dá erro. Quem roda o `build_set.py` leva junto o que as outras mudaram.

### Dados e fontes

- Número, janela, animação e som saem dos dados do jogo, com a fonte (arquivo e ID) no doc da esteira ou em
  comentário curto. Sem fonte nos dados: pergunte pela `nw-re`. Sem resposta: seção "Sem fonte" do doc.
- Antes de implementar regra de combate, ação, montaria ou tela, leia `docs/analise-re/resumo-plano-a.md`: o que a RE
  já leu do `NewWorld.exe` (encadeamento das actionlists, janelas, combos, reação, stamina, giro, homing, montarias,
  inventário, atributos), com a lista de divergências do port. Fato lido ali vale mais que doc antigo do port.
- Valor vindo da RE entra pelo `FNWRules::Num("id")` (`docs/analise-re/valores/<área>.json`), regra vinda da RE cita o id
  em comentário. `python3 tools/re_check.py` mostra o que falta aplicar.

### Telas

- Tela do jogo nova ou refeita: roteiro de `docs/hud/README.md` (spec da tela, captura sem o jogo com
  `tools/ui_shot.sh`, comparação com o print do original). O print ao dono fica para o fim.
- `FNWUILayout` é compartilhado: o modo fiel ao LyShine (`bExactText`) é ligado por tela, sem mudar as outras.

### Entrega

- Commit e push só quando o dono pedir, e só dos arquivos da esteira (`git add <caminhos>`, nunca `git add -A`).
  Mensagem com o prefixo da esteira (`armas:`, `coleta:`, `montarias:`, `sistemas:`).
- Ao fim de cada item: doc de estado da esteira, testes e a linha da esteira em `docs/ESTADO.md` atualizados.
- Credenciais de teste ficam em `dev.env`; nunca no chat nem em doc. Nada de servidores oficiais.

## Donos dos arquivos

| Esteira | Arquivos |
|---|---|
| Armas (1, 1a, 1b) | `NWCharacterArmas.cpp`, `tools/sets/player/armas.json`, `tools/etapas/armas.*`, `tools/strings/armas.json`, `docs/analise-re/perguntas/armas-*`; `NWCombatComponent`, `NWProjectile`, `NWFxActor`, `NWBrainComponent`; `tools/make_datatables.py`, `make_abilities.py`, `weapon_report.py`, `tools/ue/make_test_map.py`, `make_fx.py`; `docs/armas/`, `docs/combate-*.md`. Sub-esteiras: código novo da arma em `NWArma<Nome>.cpp` próprio, parte do conjunto em `tools/sets/player/armas_<arma>.json` (listada em `parts`), doc e testes da arma; o resto da esteira 1 é compartilhado entre as três |
| Coleta | `tools/sets/player/coleta.json`, `tools/etapas/coleta.*`, `docs/analise-re/perguntas/coleta-*`, `tools/strings/coleta.json`; `NWGatherComponent`, `NWGatherField`; `tools/make_gatherables.py`, `convert_gatherables.sh`, `tools/ue/import_gatherables.py`; `docs/coleta.md` |
| Montarias | `NWMountComponent` (já criado, vazio), `tools/sets/player/montaria.json`, `tools/etapas/montarias.*`, `tools/strings/montarias.json`; `NWMount*`; `tools/sets/mount_*.json`; `docs/montarias.md` |
| Sistemas | `NWCharacterSave.cpp`, `tools/strings/sistemas.json`, `tools/etapas/sistemas.*`; `NWHUD`, `NWInventory*`, `NWProgression*`, `NWItems`, `NWGameMenu`, `NWOptions`, `NWWorldMap`, `NWTradeskillMenu`, `NWAttributesMenu`; `backend/`; `tools/make_items.py`, `make_progression.py`, `ui_build_*.py`, `convert_city.sh`, `tools/ue/import_city.py`, `fix_city.py`; `NWRefiningMenu`, `NWStorageMenu`, `NWCrafting`, `NWInteractable`, `NWNpc*`, `NWEvents`, `NWUIShotCommandlet`; `tools/ui_screens/`, `ui_shots/`, `ui_shot.sh`, `ui_styles.py`, `ui_compare.py`, `make_crafting.py`, `make_npcs.py`; `docs/hud/`, `docs/hud-*.md`, `docs/sistemas.md` |
| RE (agente `nw-re`) | `docs/analise-re/`, `tools/re/` |
| Compartilhados | `NWCharacter.h`, `NWCharacter.cpp`, `NWPlayerController`, `SNWDevMenu`, `NWCombatTypes.h`, `NWMasteryMenu`, `NWRules`, `NWInputRegistry`, `NWDevRegistry`, `NWUILayout`, `tools/ui_canvas.py`, `NWAnimInstance`, `ABP_NW` (dono único, abaixo); `tools/sets/sword_shield.json`, `tools/strings/geral.json`, `build_all.sh`, `ue_import_all.sh`, `make_data.sh`; `docs/ESTADO.md` |

## Pontos de extensão (para não mexer no que é compartilhado)

| Preciso de | Use |
|---|---|
| Tecla nova (aparece em Atalhos) | `NW_REGISTER_INPUT` no seu `.cpp` (`NWInputRegistry.h`) |
| Botão novo no F10 | `NW_REGISTER_DEV` no seu `.cpp` (`NWDevRegistry.h`); executa no servidor |
| Trocar a locomoção do personagem (montado, coletando) | `ANWCharacter::SetLocomotionOverride` / `ClearLocomotionOverride` |
| Montagem nova do jogador | sua parte em `tools/sets/player/<esteira>.json` |
| Texto novo | `tools/strings/<esteira>.json` (`textos` ou `chaves_do_jogo`) |
| Etapa de conversão ou import | `tools/etapas/<esteira>.build.sh` / `.ue.sh` (`tools/etapas/README.md`) |
| Fato da RE | `docs/analise-re/valores/<área>.json`; pergunta em `docs/analise-re/perguntas/<esteira>-NNN.md` |

`ABP_NW` (grafo de animação do jogador) é binário e não se mescla: tem um dono só (o dono do repositório). O resto
usa montagens nos slots `DefaultSlot`/`UpperBody` e o override de locomoção. Mudança no grafo: pedir ao dono, que
faz num commit só com o ABP.

## Várias pessoas

- Cada pessoa: própria instalação do New World na build de `tools/jogo.versao` (`tools/jogo_versao.sh` confere; o
  `build_all.sh` para se for outra), pipeline completo (`docs/SETUP.md`). Extração, `Content` e `Data/` não vão para
  o git: cada um gera os seus.
- Um branch por esteira a partir da `main` (`armas`, `coleta`, `montarias`, `sistemas`). Mesclar na `main` a cada
  item fechado, no máximo a cada poucos dias; antes, trazer a `main` para o branch e rodar `tools/check.sh --cpp`.
- Mudança em arquivo compartilhado: PR pequeno e separado, mesclado logo, para os outros trabalharem em cima.
- Quem mescla na `main`: o dono do repositório (ou quem ele indicar).
- Engenharia reversa só na máquina com o Ghidra montado (`docs/ghidra.md`); os outros abrem a pergunta no arquivo
  e mesclam, e quem tem o Ghidra roda o agente.

## RE sob demanda (skill `nw-re`)

1. A sessão cria a pergunta em `docs/analise-re/perguntas/<esteira>-NNN.md` (para quê, o que o port precisa).
2. Roda o agente `nw-re` em segundo plano com o id. Segue trabalhando em outra coisa.
3. O agente responde no arquivo da pergunta, grava os fatos em `valores/<área>.json` e os endereços em
   `simbolos.md`.
4. A sessão aplica: `python3 tools/make_rules.py`, `FNWRules::Num` ou id citado, `python3 tools/re_check.py`, item
   de teste, e marca a pergunta como aplicada.

Dano, crítico, grit e mitigação são de servidor e não estão no cliente (`docs/analise-re/combate-cliente.md`); não
adianta perguntar por eles.

## Esteira 1: Armas

Uma arma por vez, até ficar igual ao jogo. Ordem:

1. Polir as quatro que já existem: espada e escudo, machadinha, arco, rapieira (habilidades que faltam, árvore de
   maestria inteira, sons, efeitos).
2. Lança, machado grande, martelo de guerra, espada grande, mangual.
3. Mosquete, bacamarte.
4. Cajado de fogo, cajado da vida, manopla de gelo, manopla do vazio. A primeira arma mágica traz a mana.

Ciclo por arma: roteiro da skill `nw-combate-arma` (levantamento → receita → conversão → ações → C++ → import →
teste) e depois o polimento:

| Parte | Pronto quando |
|---|---|
| Animação | toda ação do jogo; locomoção com a arma; sacar, guardar e trocar; mira; transições sem estalo |
| Mecânica | combo, carga, cancelamentos e janelas do `.adb`; stamina ou mana; recargas; PowerLevel, HitStun, empurrão |
| Habilidades | todas as da árvore, com upgrades e perks passivos da maestria |
| Som | golpe no ar, impacto, voz, habilidades |
| Efeitos | rastro da arma, habilidades, status |
| HUD | ícones, recargas, munição ou mana, retícula |
| Rede | o outro jogador vê igual; predição do dono sem tranco |

A arma está pronta quando os testes dela passam com o dono, o doc da arma não tem pendência que dependa só de RE e
o `re_check` não mostra nada da arma.

### O que a RE já resolveu para o combate

Fonte: `docs/analise-re/resumo-plano-a.md` (regras), `combate-cage.md`, `cage-runtime.md`, `combate-cliente.md`.
Vale para toda arma, inclusive as das sub-esteiras:

- **Encadeamento**: o combo do jogador vem dos procclips `CAGE-TransitionInstant` / `CAGE-TransitionBuffered` do `.adb`
  do fragmento (`Input`, `BackTicks`, `Condition`, `NewFragment`/`NewAction`), não das condições da actionlist.
  Ação sai pela `DeactivationCondition` (em geral `IfFragmentDone ExitOffsetSeconds`), nunca pelo fim da animação.
  Interrupção pelo `.grid` (`Never`/`Always`/`Sometimes`).
- **Janelas**: procclip `CAGE-Named` liga o marcador pelo nome; parâmetros com `<Conditional Name>` valem dentro da
  janela, senão `<Default>`.
- **Movimento por animação** (`AnimationDrivenMotion`), **giro** (linear ou exponencial `1 − (1 − r)^(30·dt)`) e
  **homing** do ataque (procclip `Homing`: raio, ângulo, `MoveToDistance`, `MaxMoveDistance`, `MoveToDuration`).
- **Reação**: primeira entrada da tabela com o `PowerLevel` do golpe e o lado; empurrão e hit stun em quadros de
  1/30 s; hit stop não existe.
- **Stamina** 100, regeneração 33,33/s, esquiva 45 (50 rolagem lenta), bloqueio 10.

Divergências que o port ainda tem e a esteira 1 corrige (uma vez, vale para todas as armas):

| Ponto | Port hoje | Jogo |
|---|---|---|
| Esquiva | precisa ter 45 de stamina | entra com qualquer stamina > 0 |
| Exaustão | sem esquiva e sem bloqueio até recuperar parte | efeito de 10 s (sem esquiva, sprint −15%), sai com stamina ≥ 1 |
| Regeneração no bloqueio | regenera após atraso | desligada enquanto bloqueia; volta 1 s após soltar |
| Bloqueio | ao segurar o botão | só dentro da janela `BlockActive` da animação |

Em aberto na RE (`resumo-plano-a.md` "Em aberto", `combate-cage.md` §11): dano-base por GS (A, B, passo, r, k),
tique da regeneração de stamina, pontuação do alvo do homing, duração do tick do `BackTicks`, `Decel` da velocidade
herdada, `SmoothTime`/`StartTime`/`HoldTimeRequired` e travas dos procclips de combo, condições nomeadas
(`CanAttack_*`). Pode perguntar de novo, com pista nova (endereço, offset, função candidata de `simbolos.md`).
Dano, crítico, grit e mitigação são de servidor: não perguntar.

Dúvida de combate vira pergunta (`docs/analise-re/perguntas/armas-NNN.md`, ou `<arma>-NNN` nas sub-esteiras), não
chute: regra com prova H, item em aberto ou comportamento que os dados não explicam. Enquanto a resposta não vem, o
item fica "aguardando <id>" no doc da arma e a sessão segue com outro. Antes de abrir, procure em `perguntas/` e
`valores/`; no máximo dois agentes `nw-re` por sessão ao mesmo tempo (o Ghidra é compartilhado).

Matriz RE × port: seção de `docs/combate-armas.md`, mantida pela esteira 1. Uma linha por regra da RE, com o que o
port faz (arquivo e função), a divergência, a prova e o id da pergunta aberta. As sub-esteiras leem antes do
levantamento da arma.

### Sub-esteiras de arma (1a e 1b)

Uma sessão por arma, ao mesmo tempo que a esteira 1 (que segue polindo as quatro armas que existem e corrige as
divergências acima). Cada sub-esteira faz o ciclo inteiro da arma dela (tabela acima) e só ela.

- Arquivos próprios: `NWArma<Nome>.cpp`/`.h`, `tools/sets/player/armas_<arma>.json` (acrescentar em `parts` do
  `tools/sets/sword_shield.json`, mudança mínima), `docs/armas/<arma>.md`, `docs/testes/<arma>.md`, perguntas
  `docs/analise-re/perguntas/<arma>-NNN.md`. Prefixo de commit: `armas(<arma>):`.
- O que é de todas as armas (`NWCombatComponent`, `NWCharacterArmas.cpp`, `NWProjectile`, `make_abilities.py`,
  maestria): mudança mínima, descrita no relatório, avisando a esteira 1. Mecânica comum nova (ex.: mana) nasce
  num componente ou arquivo novo, para não brigar com a esteira 1.
- `build_set.py` monta o conjunto inteiro: quem roda leva o que as outras sub-esteiras mudaram; combinar pelo
  relatório antes de rodar.

**1a. Manopla de gelo** (`IceGauntlet`). Primeira arma mágica do port: traz a **mana** (recurso, regeneração, custo
por golpe e habilidade, HUD) para as outras mágicas usarem depois. Projéteis de gelo, canalização, status de frio
(lentidão, congelamento) e as habilidades da árvore.

**1b. Espada grande** (`Greatsword`). Duas posturas (ofensiva e defensiva) com troca e as habilidades de cada uma,
combos longos, carga do pesado, empurrões e o bloqueio da arma.

## Esteira 2: Coleta

Ofícios de coleta: Mineração, Corte de madeira, Colheita, Esfolamento; Pesca por último (sistema próprio).
Por ofício:

| Parte | Pronto quando |
|---|---|
| Animação | começo, laço e fim por ferramenta; ferramenta na mão; arma guardada durante a coleta e de volta depois |
| Regras | velocidade por ferramenta e nível, XP, bônus de saque, nível mínimo, condições das tabelas de saque |
| Som | golpe da ferramenta por material, esgotar, saque |
| Efeitos | impacto por material, esgotar (`DepletedVFX`), respawn |
| HUD | cartão de interação (inclusive o de várias opções), barra de progresso, aviso de saque, bússola e rastreio |
| Visual | coletável no chão (sem flutuar), folhagem e copas corretas, sem estalo de LOD |

Esfolar depende de criatura morta (IA e criaturas são da esteira 1 hoje); combine pelo relatório.
Coletar é uma ação de actionlist como as de combate: começo, laço e fim saem pelas regras de `cage-runtime.md`
(saída pela `DeactivationCondition`, `NoAutoBlendOut`, interrupção pelo `.grid`), não por tempo fixo.
Pendências conhecidas: `docs/pendencias.md`, seção "Coleta: polimento".

## Esteira 3: Montarias

Dados: `extract/data/.../datatables/mounts/` (`javelindata_mounts`, `_mounttypes`, `_mount_movement`), aparências
e tinturas em `javelindata_itemdefinitions_mount*`. Ordem: cavalo, lobo, felino (leão), depois os outros tipos
(urso...), aparências e tinturas.

| Parte | Pronto quando |
|---|---|
| Conversão | malha, esqueleto e animações da montaria (conjunto `tools/sets/mount_<tipo>.json`) |
| Cavaleiro | animações do jogador montado (tags de montaria nos `.adb` do jogador) |
| Invocar e desmontar | tempo de invocação, interrupção por dano, desmontar ao apanhar ou atacar, recarga |
| Movimento | velocidades, aceleração, giro, salto, galope e o recurso dele (`mount_movement`) |
| Câmera e som | câmera montada; passos, voz, invocar |
| HUD | ícone, recarga, recurso do galope |
| Rede | outro jogador vê montaria e cavaleiro iguais |

Já lido pela RE (`docs/analise-re/montarias.md`, resumo em `resumo-plano-a.md`): velocidade só pelo nível de
montaria em 5 faixas (0, 50, 100, 150, 200+, `javelindata_tradeskillriding`) e por superfície, galope ×1,12 (as
linhas por espécie não são usadas); galope exige stamina da montaria ≥ 15, gasta 20/s e recupera 10/s; giro montado
com taxa interpolada pela velocidade (cavalo 0,11 → 0,09 entre 0 e 15 m/s), fórmula exponencial do giro do
jogador. Em aberto: aceleração montada.

## Esteira 4: Sistemas e Queda Eterna

Escopo: só a capital de Queda Eterna (slice `settlement_northwindsward_complete`, capital `ef_persistence` em `r_+04_+02`) com o recorte de
terreno em volta, num mapa próprio `L_QuedaEterna`. Guardaventos continua como está. Ordem:

1. Converter a cidade e montar o mapa jogável (skill `nw-convert-city`).
2. NPCs da cidade: posição dos slices, aparência, animação parada, nome.
3. Estações de refino e de ofício: interação, receitas das tabelas, tempo, XP, nível mínimo.
4. Armazém da cidade.
5. Lojas e mercadores.
6. Mercado (posto de trocas): ordens de compra e venda no nw-core, taxas das tabelas.
7. Casas: lotes, compra, interior, mobília, armazém da casa.
8. Nível e XP com fontes; HUDs que faltarem em cada sistema.

Cada sistema está pronto quando funciona de ponta a ponta com o servidor dedicado e o nw-core (persistência), tem
as telas do jogo e o dono testou. Mudança de banco é migration nova no `backend/` (`docs/plano-servidor.md`).

Estado (2026-10-13): itens 1–4 feitos e testados pelo dono (cidade, NPCs com brindes, estações de refino com a tela
do jogo, armazém); nível pelos tutoriais (item 8, parte). Backend do mercado e das casas pronto, sem tela. Detalhe em
`docs/sistemas.md`.

Telas: roteiro, regras do LyShine e anotações do dono para a próxima sessão em `docs/hud/README.md`. Próximo de
telas: acertos do refino anotados lá, depois armazém, atributos, ofícios, inventário e maestria (com a esteira 1).

## Prompts das sessões

Colar numa sessão nova do Claude Code aberta em `~/workspace/nw-port`.

### Esteira 1: Armas

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da esteira 1 (Armas). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns e a seção Esteira 1)
e use as skills nw-esteira, nw-combate-arma e nw-re. As sub-esteiras 1a e 1b e outras sessões trabalham no mesmo
checkout ao mesmo tempo.

Base: a RE do combate feita no Ghidra. Leia inteiros docs/analise-re/resumo-plano-a.md, combate-cage.md,
combate-cliente.md e cage-runtime.md, e rode python3 tools/re_check.py. Fato com prova C ou R vale mais que doc
antigo do port (combate-regras-jogo.md, plano-mecanicas.md) e que o código atual.

1. Matriz RE × port, seção nova em docs/combate-armas.md: uma linha por regra da RE (encadeamento pelos procclips
   CAGE-Transition*, saída pela DeactivationCondition, interrupção pelo .grid, janelas CAGE-Named e
   Default/Conditional, AnimationDrivenMotion, reação por PowerLevel e lado, empurrão nos três eixos, hit stun,
   stamina, esquiva, bloqueio e exaustão, giro, homing, recargas e status), com o que o port faz hoje (arquivo e
   função), a divergência, a prova e o que falta. Entram também os itens "falta" do re_check do combate.
2. Perguntas: toda linha com prova H, item em aberto da RE ou dúvida sua vira pergunta para o agente nw-re
   (docs/analise-re/perguntas/armas-NNN.md, uma por dúvida, com o que o port precisa e as pistas da RE). Comece
   rodando as abertas armas-001 a 003.
3. Me mostre a matriz e a lista de perguntas antes de mudar código. Depois corrija o que vale para todas as armas
   (divergências da Esteira 1, encadeamento pelos procclips, janelas, giro, homing, empurrão), aplicando cada
   resposta pela skill nw-re.
4. Por último, o polimento das quatro armas que existem, uma por vez: tools/weapon_report.py, docs/armas/<arma>.md
   e a matriz; me mostre o que falta na arma antes de mexer nela.

Sem chute: número, janela ou comportamento sem fonte não entra no código; fica "aguardando armas-NNN" no doc e você
segue com outro item. Mudança comum que afete a manopla de gelo (1a) ou a espada grande (1b): descreva no relatório.

Teste: abra o editor em L_Arena e me peça o print; nunca rode o jogo sozinho. Trabalho pesado pela fila
(tools/pesado.sh; no Windows, python tools/pesado.py). Commit e push só quando eu pedir, e só dos arquivos da esteira.
```

### Esteira 1a: Manopla de gelo

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da sub-esteira 1a (Manopla de gelo). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns,
Esteira 1 e "Sub-esteiras de arma") e use as skills nw-esteira, nw-combate-arma e nw-re. A esteira 1, a 1b e
outras sessões trabalham no mesmo checkout ao mesmo tempo.

Base: a RE do combate feita no Ghidra. Leia inteiros docs/analise-re/resumo-plano-a.md, combate-cage.md,
combate-cliente.md e cage-runtime.md, e a matriz RE × port de docs/combate-armas.md (da esteira 1; se ainda não
existir, siga pela RE).

Objetivo: manopla de gelo igual ao jogo (animação, mecânica, habilidades e maestria, som, efeitos, HUD, rede), e a
mana, que ela traz para as armas mágicas.

1. Levantamento em docs/armas/manopla-gelo.md, lendo a arma pelas regras da RE: procclips de combo
   (CAGE-TransitionInstant/Buffered: Input, BackTicks, Condition, NewFragment/NewAction), janelas CAGE-Named e os
   Conditional que dependem delas, saída de cada ação e entradas no .grid, Homing e CameraRelativeFacing por golpe,
   AnimationDrivenMotion, ConsumeCharge, SetCooldownTimer, ModifyStatusEffects, PerformActiveAbility; tabelas de
   habilidades, status de frio (lentidão, congelamento) e projéteis; VFX e sons.
2. Mana: leia o que estiver no slice do jogador e nas tabelas. O que só o exe diz (regeneração, atraso, custo por
   quadro na canalização, cargas) vira pergunta.
3. Perguntas: toda dúvida, prova H ou peça sem regra lida vira pergunta para o agente nw-re
   (docs/analise-re/perguntas/manopla-gelo-NNN.md, uma por dúvida, com as pistas da RE).
4. Me mostre o levantamento, as perguntas abertas e o plano, com o que precisa mudar em arquivo comum, antes de
   mudar código.

Sem chute: o que não tem fonte fica "aguardando manopla-gelo-NNN" no doc e você segue com outro item.

Teste: abra o editor em L_Arena e me peça o print; nunca rode o jogo sozinho. Trabalho pesado pela fila
(tools/pesado.sh; no Windows, python tools/pesado.py). Commit e push só quando eu pedir, e só dos arquivos da
sub-esteira.
```

### Esteira 1b: Espada grande

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da sub-esteira 1b (Espada grande). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns,
Esteira 1 e "Sub-esteiras de arma") e use as skills nw-esteira, nw-combate-arma e nw-re. A esteira 1, a 1a e
outras sessões trabalham no mesmo checkout ao mesmo tempo.

Base: a RE do combate feita no Ghidra. Leia inteiros docs/analise-re/resumo-plano-a.md, combate-cage.md,
combate-cliente.md e cage-runtime.md, e a matriz RE × port de docs/combate-armas.md (da esteira 1; se ainda não
existir, siga pela RE).

Objetivo: espada grande igual ao jogo (as duas posturas e a troca, combos, carga, habilidades e maestria, som,
efeitos, HUD, rede).

1. Levantamento em docs/armas/espada-grande.md, lendo a arma pelas regras da RE: postura pelas tags
   (SetMannequinTag/IfMannequinTag) e condições nomeadas, procclips de combo (Input, BackTicks, Condition,
   NewFragment/NewAction, HoldTimeRequired na carga), janelas CAGE-Named e os Conditional que dependem delas, saída
   de cada ação e entradas no .grid, Homing por golpe, AnimationDrivenMotion, stamina (Greatsword_Offense_Attack 10),
   bloqueio na janela BlockActive, empurrão e reação de cada golpe na damagetable; tabelas de habilidades, VFX e sons.
2. Perguntas: toda dúvida, prova H ou peça sem regra lida (HoldTimeRequired, travas SingleActivation*, onde ficam as
   condições nomeadas) vira pergunta para o agente nw-re (docs/analise-re/perguntas/espada-grande-NNN.md, uma por
   dúvida, com as pistas da RE).
3. Me mostre o levantamento, as perguntas abertas e o plano, com o que precisa mudar em arquivo comum, antes de
   mudar código.

Sem chute: o que não tem fonte fica "aguardando espada-grande-NNN" no doc e você segue com outro item.

Teste: abra o editor em L_Arena e me peça o print; nunca rode o jogo sozinho. Trabalho pesado pela fila
(tools/pesado.sh; no Windows, python tools/pesado.py). Commit e push só quando eu pedir, e só dos arquivos da
sub-esteira.
```

### Esteira 2: Coleta

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da esteira 2 (Coleta). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns e a seção Esteira 2)
e use as skills nw-esteira e nw-coleta. Outras três sessões trabalham no mesmo checkout ao mesmo tempo.

Objetivo: Mineração, Corte de madeira, Colheita e Esfolamento iguais ao jogo em animação, som, efeitos, HUD e
regras, sem bugs visuais; Pesca no fim. Comece pelo levantamento: o que o port faz hoje
(NWGatherComponent, NWGatherField, docs/pendencias.md "Coleta: polimento") contra o que o jogo define (fragmentos
de coleta nos .adb do jogador, tabelas de ofício e de saque, sons, VFX das variações). Grave em docs/coleta.md e
me mostre a lista do que falta, por ofício, antes de mudar código.

Sem fonte nos dados: skill nw-re. Teste: abra o editor em L_SouthWindsward e me peça o print; nunca rode o jogo
sozinho. Trabalho pesado pela fila (tools/pesado.sh). Commit e push só quando eu pedir, e só dos arquivos da
esteira.
```

### Esteira 3: Montarias

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da esteira 3 (Montarias). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns e a seção
Esteira 3) e use as skills nw-esteira e nw-montaria. Outras três sessões trabalham no mesmo checkout ao mesmo tempo.

Objetivo: montarias iguais ao jogo: conversão, animações da montaria e do cavaleiro, invocar e desmontar,
movimento, câmera, som, HUD e rede. Ordem: cavalo, lobo, felino (leão), outros tipos, aparências e tinturas.
Comece pelo levantamento em docs/montarias.md: tabelas de mounts/ (javelindata_mounts, _mounttypes,
_mount_movement), malhas e .adb das montarias (adb/mounts/), player_mount.adb do cavaleiro, slices e actionlists
CAGE de montar. Me mostre o plano antes de mudar código.

Sem fonte nos dados: skill nw-re. Teste: abra o editor em L_SouthWindsward e me peça o print; nunca rode o jogo
sozinho. Trabalho pesado pela fila (tools/pesado.sh). Commit e push só quando eu pedir, e só dos arquivos da
esteira.
```

### Esteira 4: Sistemas e Queda Eterna

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da esteira 4 (Sistemas e Queda Eterna). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns e a
seção Esteira 4) e use as skills nw-esteira, nw-sistemas e nw-convert-city. Outras três sessões trabalham no
mesmo checkout ao mesmo tempo.

Objetivo: só a capital de Queda Eterna, num mapa próprio L_QuedaEterna, com os sistemas do jogo funcionando nela:
NPCs, estações de refino e ofício, armazém, lojas, mercado, casas, nível e XP, e as telas de cada um. Ordem da
Esteira 4. Cidade, NPCs, refino e armazém já estão feitos (docs/sistemas.md). Telas: siga docs/hud/README.md
(captura sem o jogo com tools/ui_shot.sh, comparação com o print do original) e comece pelas anotações do dono lá
(slider da quantidade, Espaço, botão que pisca). Depois lojas, mercado e casas. Me mostre o plano antes de mexer no
backend.

Sem fonte nos dados: skill nw-re. Teste: abra o editor no mapa e me peça o print; nunca rode o jogo sozinho.
Trabalho pesado pela fila (tools/pesado.sh). Commit e push só quando eu pedir, e só dos arquivos da esteira.
```
