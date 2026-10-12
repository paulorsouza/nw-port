# Esteiras de trabalho

Quatro frentes do port. Cada uma pode ser uma sessão na mesma máquina (mesmo checkout) ou uma pessoa na própria
máquina, num branch (seção "Várias pessoas").
A engenharia reversa (RE) do `NewWorld.exe` não é uma esteira: qualquer sessão pede uma pergunta específica pela
skill `nw-re`, que roda o agente `nw-re` em segundo plano.

| Esteira | Skill | Doc de estado | Testes | Mapa de teste |
|---|---|---|---|---|
| 1. Armas | `nw-combate-arma` | `docs/combate-armas.md`, `docs/armas/<arma>.md` | `docs/testes/armas.md` | `L_Arena` |
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
- Valor vindo da RE entra pelo `FNWRules::Num("id")` (`docs/analise-re/valores/<área>.json`), regra vinda da RE cita o id
  em comentário. `python3 tools/re_check.py` mostra o que falta aplicar.

### Entrega

- Commit e push só quando o dono pedir, e só dos arquivos da esteira (`git add <caminhos>`, nunca `git add -A`).
  Mensagem com o prefixo da esteira (`armas:`, `coleta:`, `montarias:`, `sistemas:`).
- Ao fim de cada item: doc de estado da esteira, testes e a linha da esteira em `docs/ESTADO.md` atualizados.
- Credenciais de teste ficam em `dev.env`; nunca no chat nem em doc. Nada de servidores oficiais.

## Donos dos arquivos

| Esteira | Arquivos |
|---|---|
| Armas | `NWCharacterArmas.cpp`, `tools/sets/player/armas.json`, `tools/etapas/armas.*`, `tools/strings/armas.json`, `docs/analise-re/perguntas/armas-*`; `NWCombatComponent`, `NWProjectile`, `NWFxActor`, `NWBrainComponent`; `tools/make_datatables.py`, `make_abilities.py`, `weapon_report.py`, `tools/ue/make_test_map.py`, `make_fx.py`; `docs/armas/`, `docs/combate-*.md` |
| Coleta | `tools/sets/player/coleta.json`, `tools/etapas/coleta.*`, `docs/analise-re/perguntas/coleta-*`, `tools/strings/coleta.json`; `NWGatherComponent`, `NWGatherField`; `tools/make_gatherables.py`, `convert_gatherables.sh`, `tools/ue/import_gatherables.py`; `docs/coleta.md` |
| Montarias | `NWMountComponent` (já criado, vazio), `tools/sets/player/montaria.json`, `tools/etapas/montarias.*`, `tools/strings/montarias.json`; `NWMount*`; `tools/sets/mount_*.json`; `docs/montarias.md` |
| Sistemas | `NWCharacterSave.cpp`, `tools/strings/sistemas.json`, `tools/etapas/sistemas.*`; `NWHUD`, `NWInventory*`, `NWProgression*`, `NWItems`, `NWGameMenu`, `NWOptions`, `NWWorldMap`, `NWTradeskillMenu`, `NWAttributesMenu`; `backend/`; `tools/make_items.py`, `make_progression.py`, `ui_build_*.py`, `convert_city.sh`, `tools/ue/import_city.py`, `fix_city.py`; `docs/hud-*.md`, `docs/sistemas.md` |
| RE (agente `nw-re`) | `docs/analise-re/`, `tools/re/` |
| Compartilhados | `NWCharacter.h`, `NWCharacter.cpp`, `NWPlayerController`, `SNWDevMenu`, `NWCombatTypes.h`, `NWMasteryMenu`, `NWRules`, `NWInputRegistry`, `NWDevRegistry`, `NWAnimInstance`, `ABP_NW` (dono único, abaixo); `tools/sets/sword_shield.json`, `tools/strings/geral.json`, `build_all.sh`, `ue_import_all.sh`, `make_data.sh`; `docs/ESTADO.md` |

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

## Esteira 4: Sistemas e Queda Eterna

Escopo: só a capital de Queda Eterna (assentamento `settlement_everfall`, slice `*_complete`) com o recorte de
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

## Prompts das sessões

Colar numa sessão nova do Claude Code aberta em `~/workspace/nw-port`.

### Esteira 1: Armas

```
Projeto: port do New World para Unreal 5.8 em ~/workspace/nw-port. Responda em pt-BR, texto direto.
Você é a sessão da esteira 1 (Armas). Leia docs/ESTADO.md e docs/esteiras.md (regras comuns e a seção Esteira 1)
e use as skills nw-esteira e nw-combate-arma. Outras três sessões trabalham no mesmo checkout ao mesmo tempo.

Objetivo: cada arma igual ao jogo em animação, mecânica, habilidades e maestria, som, efeitos e HUD, uma por vez,
na ordem da Esteira 1. Comece polindo as quatro que existem: rode tools/weapon_report.py e compare com
docs/armas/arco.md, docs/armas/rapieira.md e docs/combate-armas.md; me mostre a lista do que falta em cada uma,
na ordem que pretende fazer, antes de mudar código.

Sem fonte nos dados: skill nw-re. Teste: abra o editor em L_Arena e me peça o print; nunca rode o jogo sozinho.
Trabalho pesado pela fila (tools/pesado.sh). Commit e push só quando eu pedir, e só dos arquivos da esteira.
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
Esteira 4. Comece convertendo a cidade (confirme o slice do assentamento de Queda Eterna e a região do terreno) e
montando o mapa; em paralelo, levante em docs/sistemas.md o que o jogo define para cada sistema e o que o port já
tem. Me mostre o plano antes de mexer no backend.

Sem fonte nos dados: skill nw-re. Teste: abra o editor no mapa e me peça o print; nunca rode o jogo sozinho.
Trabalho pesado pela fila (tools/pesado.sh). Commit e push só quando eu pedir, e só dos arquivos da esteira.
```
