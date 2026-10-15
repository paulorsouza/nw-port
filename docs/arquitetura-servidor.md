# Arquitetura do servidor (rascunho, 2026-10-14)

Rascunho para o dono aprovar antes de valer para as esteiras.

Meta: um canal (um processo de servidor por mapa) com até 500 jogadores espalhados pelo mundo. Não é 500 no mesmo
lugar: a cidade junta dezenas, o resto fica espalhado. Mais jogadores = mais canais do mesmo mapa; personagem,
armazém e mercado já vivem no nw-core e valem para todos os canais.

Orçamento: o servidor roda a 30 ciclos/s (33 ms por ciclo), a lógica quase toda num núcleo. Com 500 jogadores sobra
~60 µs por jogador para movimento, combate, IA perto dele e replicação. Por isso o custo de cada sistema tem que
crescer com o que está perto do jogador, nunca com o tamanho do mundo nem com o total de jogadores.

## Regras para todas as esteiras

1. **Nada de percorrer o mundo** (`TActorIterator`, `TObjectIterator`, `GetAllActorsOfClass`) em `Tick`, IA, combate
   ou RPC. Busca por perto vai pela grade espacial (`FNWSpatialGrid`, a fazer); busca de um ator conhecido vai por
   registro (ex. `ANWNpc` por variante, `ANWStation` por cidade). Iterar o mundo só no `BeginPlay` ou em ferramenta.
2. **Tick só quando precisa.** Ator ou componente sem nada a fazer fica com o tick desligado; o que pode ser
   temporizador (1 s, 0,25 s) não é tick. Lógica cara por jogador roda em intervalo, não a cada ciclo.
3. **IA só perto de jogador.** Criatura sem jogador a menos de N m não pensa, não anda e não procura alvo
   (`UNWBehaviorTreeComponent`, `UNWBrainComponent`); o `ANWSpawnField` já cria só perto de jogador.
4. **Replicação**: Iris ligado no projeto; propriedades no modelo "avisar quando muda" (push model,
   `MARK_PROPERTY_DIRTY`); listas grandes (bolsa, armazém, itens de saco) como lista incremental
   (`FFastArraySerializer`); ator parado adormecido (`NetDormancy`) e acordado só quando muda (estações, NPCs, sacos,
   portas); distância e frequência de replicação por tipo (`NetCullDistanceSquared`, `NetUpdateFrequency`):
   criatura e jogador longe recebem menos, coletável e NPC só perto.
5. **Só o dono recebe o que é dele** (bolsa, armazém, progressão, F10: `COND_OwnerOnly`), como já é hoje.
6. **RPC do cliente**: só intenção, nunca resultado; o servidor confere distância, dono, quantidade, nível e tempo de
   recarga; limite de frequência por jogador (a fazer, hoje só o chat).
7. **Persistência**: o nw-core é a fonte da verdade; o servidor grava operações pequenas (etapa 2 das gravações) e o
   salvamento periódico só de quem mudou e só do que mudou.
8. **Medir**: cada sistema caro tem um contador de tempo (`SCOPE_CYCLE_COUNTER` / `TRACE_CPUPROFILER_EVENT_SCOPE`) e
   entra no relatório do teste de carga.

## O que hoje vai contra as regras (levantado em 2026-10-14)

| Onde | O quê | Custo com muitos | Dono |
|---|---|---|---|
| `UNWBrainComponent::FindTarget` (feito: `UNWBehaviorTreeComponent::LookForTarget` e `ANWCreature::HasTarget` usam `UNWPlayerGrid`) | cada criatura percorre todos os personagens (jogadores e criaturas) para achar alvo | criaturas × personagens a cada busca | sem dono na tabela (IA); `NWBrainComponent` é das armas |
| `UNWBehaviorTreeComponent::Aggro` | percorre todas as criaturas para chamar o grupo | idem | sem dono na tabela (IA) |
| `ANWCharacter::Tick` (homing do golpe) | percorre todos os personagens a cada ciclo durante o homing | jogadores atacando × personagens | compartilhado |
| `ANWCharacter::TickDummy` | percorre todos os personagens | bonecos × personagens | armas |
| `ANWSpawnField::Step` (feito: `UNWPlayerGrid`) | percorria todos os personagens para achar jogadores | baixo | sem dono na tabela |
| `UNWInteractorComponent` (NPC, loja, despensa) | percorre NPCs e estações a cada RPC | baixo; vira registro | sistemas |
| `ANWLootBag::GroundWeight` | percorre todos os sacos a cada soltar | baixo; vira registro por dono | sistemas |
| `UNWGatherComponent::ServerGather` | percorre todos os componentes de coleta para ver se o ponto está ocupado | jogadores coletando × jogadores | coleta |
| Projeto | sem Iris, sem push model, sem dormência, sem distância de replicação por tipo | replicação cresce com o total | servidor/rede |
| Inventário e armazém | listas replicadas como `TArray` simples (bolsa de 500 espaços) | cada mudança manda mais do que precisa | sistemas |
| Salvamento periódico | regrava a bolsa inteira a cada 5 s mesmo sem mudança | 500 jogadores = ~100 gravações/s grandes no banco | sistemas |

## Banco (um Postgres para todos os canais)

Cenário de conta: 10 canais lotados = 5.000 jogadores no mesmo nw-core e no mesmo Postgres.

- Do jeito de hoje não aguenta: o salvamento periódico regrava a bolsa inteira (até 500 pilhas) de cada jogador a cada
  5 s = ~1.000 gravações/s apagando e inserindo centenas de linhas cada, mais o diff do log.
- Com o desenho (operações pequenas + periódico só do que mudou): ~0,2–0,5 operação de item por jogador por segundo
  (coleta, saque, fabricar, armazém) = 1.000–2.500 transações pequenas/s (5–10 linhas cada), mais posição e vida a
  cada 30–60 s. Um Postgres de 8–16 núcleos com NVMe faz isso com folga; o número sai do teste de carga do nw-core.
- Regras do banco:
  - nada de regravar coleção inteira; gravação = o que mudou;
  - posição, vida e XP não precisam ir ao banco a cada 5 s (30–60 s e ao sair basta; item e moeda vão na hora);
  - `item_log` particionado por dia, com retenção (30–90 dias) e coleta/saque agregados (uma linha por item por
    minuto, não uma por golpe): sem isso são ~200 milhões de linhas por dia nesse cenário;
  - nw-core sem estado em memória, em várias cópias, com PgBouncer na frente do Postgres;
  - transações curtas no mercado (ordens de item popular disputam as mesmas linhas).
- Se um dia passar disso: log em banco separado, réplica de leitura para as listas do mercado e, por último,
  personagens divididos entre bancos por id (o mercado num banco próprio).

## Teste de carga

Duas partes, para medir sem precisar de 500 PCs:

1. **Jogadores simulados no servidor**: comando de servidor `nw.sim <n> [região]` cria n personagens com controle
   de IA (sem conexão), espalhados como jogadores reais (parte na cidade, parte coletando, parte lutando com
   criaturas). Mede CPU de movimento, combate, IA, coleta e persistência (o salvamento dos simulados vai para um
   nw-core de teste).
2. **Bots de verdade** (`tools/linux/net_test.sh <n>`, `-nwbot`, sem render): algumas dezenas, para medir replicação e
   banda de fato.

3. **Carga no nw-core** (script): N personagens gravando no ritmo medido nas partes 1 e 2 (operações e periódico),
   contra um Postgres de teste; mede transações por segundo, latência (p50/p95/p99) e tamanho do log.

Relatório a cada 10 s no log do servidor: tempo do ciclo (média, p95, máximo), tempo por sistema, jogadores,
criaturas ativas, atores replicados, bytes enviados por jogador, gravações por segundo no nw-core. Rodadas: 50, 100,
200, 500 simulados + 20 bots, em Queda Eterna e em Guardaventos.

## Medições

Como rodar (servidor sem janela, sem nw-core, pela fila; editor e New World fechados):

```bash
tools/linux/pesado.sh timeout -s INT 90 /mnt/jogos/UnrealEngine/5.8/Engine/Binaries/Linux/UnrealEditor \
  "$PWD/unreal/NWPort/NWPort.uproject" /Game/NW/Maps/L_QuedaEterna -server -nullrhi -unattended -nosplash \
  -nwperf -nwsim=100 -nwsimraio=300 -nwcsv=300 -abslog="$PWD/unreal/NWPort/Saved/Perf/sim.log"
python3 tools/perf_csv.py --filtro Exclusive/GameThread
```

2026-10-14, binário do editor como servidor (`-server`; o servidor empacotado deve ser mais rápido), Queda Eterna,
simulados andando o tempo todo (pior caso), sem criaturas, sem conexões:

| Simulados | Trabalho/ciclo | Movimento | Tick | Animação | Física | Obs. |
|---|---|---|---|---|---|---|
| 0 | ~1 ms | | | | | |
| 50 | 9,3 ms | 3,4 | 1,1 | 4,0 | 0,6 | como está hoje |
| 50 | 6,7 ms | 3,4 | 1,0 | 0,8 | 0,6 | `-nwexp=anim` |
| 100 | 12,8 ms | 6,7 | 2,0 | 1,4 | 1,0 | `anim` |
| 200 | 24,7 ms | 13,2 | 4,2 | 2,7 | 2,0 | `anim` |
| 50 (L_Arena, chão simples) | 5,9 ms | 2,9 | 1,1 | 0,9 | | `anim`; 60 personagens |

Leitura:
- Cresce em linha reta: ~120 µs por personagem andando (com `anim`), o dobro do orçamento de 60 µs. Teto deste
  binário: ~260 personagens andando ao mesmo tempo.
- Animação no servidor era a maior parte (80 µs/personagem): só montagem (golpe, esquiva) no servidor cortou 80%.
- Movimento (`CharacterMovement`): ~65 µs por personagem andando na cidade, ~49 µs no chão simples da arena. A
  cidade usa colisão por triângulo (`complex as simple`, `import_city.py`/`fix_city.py`), mais cara.
- Tick do personagem: ~20 µs, inclui câmera (`UpdateCamera`, `UpdateCameraClipHide`), que não serve no servidor.
- `meshcol` (malha sem colisão) e `floor` (menos checagem de chão): sem ganho medido.

Depois dos itens 1 e 2 (2026-10-14: servidor só anima com montagem; câmera só no jogador local; malha sem colisão no
servidor quando não é alvo de golpe), 200 simulados no binário do editor: 24,1 → 18,1 ms (movimento 12,2, animação
2,3, tick 2,0, física 0,5).

Servidor empacotado (`tools/linux/package_server.sh`, Development, `deploy/build/LinuxServer/NWPortServer.sh <mapa> -nwperf
-nwsim=<n> -nwcsv=300`), Queda Eterna, simulados andando o tempo todo:

| Simulados | Trabalho/ciclo | Movimento | Animação | Tick | Física | Memória |
|---|---|---|---|---|---|---|
| 50 | 3,5 ms | 2,4 | 0,5 | 0,3 | 0,2 | 1,0 GB |
| 200 | 13,3 ms | 8,8 | 1,5 | 1,3 | 0,4 | 1,0 GB |
| 500 | 32,3 ms (p95 35) | 22,3 | 3,7 | 3,4 | 0,8 | 1,1 GB |

- ~65 µs por personagem andando; 500 andando ao mesmo tempo encostam no limite de 33 ms. O movimento é 70% do custo
  (~44 µs por personagem andando).
- Falta medir: criaturas (Guardaventos), conexões de verdade (replicação e banda), gravação no nw-core, e a mistura
  real (jogador parado, coletando, no menu custa menos que andando o tempo todo).

Guardaventos (`L_SouthWindsward`), servidor empacotado, 200 simulados em 900 m, ~170 criaturas criadas pelo spawn em
volta deles (perfil 70 s depois, `-nwcsvdelay=70`):

| Situação | Trabalho/ciclo | Movimento | Tick | Ciclos/s |
|---|---|---|---|---|
| Antes | 37 ms (p95 51) | 13,2 | 13,4 | ~25 |
| Grade de jogadores + visão a cada 0,25 s (`UNWPlayerGrid`) | 31–35 ms | 12,1 | 13,0 | ~27 |
| + foco da coleta só no jogador local (`UNWGatherComponent`) | **26 ms** (p95 29) | 13,0 | 6,9 | 30 |

- O tick caro era o foco do [E] da coleta: no servidor, criaturas e simulados contam como "controlados localmente" e
  procuravam coletável a cada ciclo (~7 ms com 370 personagens). Mesmo erro da câmera; regra nova abaixo.
- A árvore de comportamento das criaturas custa ~21 µs por criatura por ciclo (`Behavior` no rastreamento).

Teste de 1 hora (2026-10-14, servidor empacotado com o código do commit b884219, Guardaventos, 200 simulados em 900 m,
~160 criaturas em volta deles, sem conexões e sem nw-core; `soak.log` em `unreal/NWPort/Saved/Perf/`):

| Minutos | Trabalho/ciclo méd | p95 | Máx | Criaturas | Memória |
|---|---|---|---|---|---|
| 0–5 | 24,9 ms | 27,4 | 36 | 162 | 2,76 → 2,79 GB |
| 15–20 | 28,0 | 30,5 | 42 | 168 | 2,75 GB |
| 30–35 | 27,5 | 30,1 | 45 | 156 | 2,76 GB |
| 55–60 | 28,1 | 30,6 | 40 | 155 | 2,76 GB |

- Sem vazamento: memória parada em ~2,76 GB por 60 min (+12 MB entre os minutos 15 e 60).
- Sem piora: depois dos primeiros 15 min o ciclo fica em 27–29 ms; 30 ciclos/s o tempo todo, nenhum relatório abaixo
  de 29.
- A subida de ~25 para ~28 ms nos primeiros 15 min acompanha os simulados presos em parede (`is stuck`, 26 mil linhas;
  alguns presos a hora toda): o simulado anda reto e não desvia. É defeito do teste (`nw.sim`), não do jogo.
- Um ciclo de ~45 ms a cada ~10 min (20.18.26, 20.28.37, 20.38.48...). Causa não achada; não é nada do port com
  período de 600 s. Fica anotado.
- Log: 78 mil avisos `GetSocketInfoByName ... No SkeletalMesh` (97% de `NWCreature`): criatura criada no servidor
  sem malha procura soquete. Só ruído de log hoje; corrigir junto com os caminhos abertos.

Regra que saiu disso: **"controlado localmente" no servidor inclui toda IA.** Lógica de jogador local (câmera, foco
do [E], HUD, tremida) confere `IsLocallyControlled() && IsPlayerControlled()`.

## Caminhos abertos (otimização para depois)

Medidos e anotados; voltar quando um canal encher:

1. Movimento: ~44 µs por personagem andando, maior custo restante. Opções: criaturas longe de jogador com movimento
   menos frequente e `NavWalking`; colisão da cidade mais simples (a cidade usa triângulo por triângulo; ~25% do
   movimento); `MoveComponent` e checagem de chão são o grosso.
2. Envio de movimento do cliente a no máximo 30/s (15/s parado) e detecção de tempo acelerado ligada
   (`DefaultGame.ini`, `GameNetworkManager`): falta o teste em rede com o dono (andar, pular, esquivar sem "puxar").
3. Animação só com montagem no servidor e malha sem colisão no servidor (`ANWCharacter`): falta o teste de combate do
   dono (golpe, combo, esquiva, homing, arco) no editor e em rede.
4. Rede de verdade: bots conectados (`tools/linux/net_test.sh`) para medir replicação e banda; depois Iris, distância de
   replicação por tipo, dormência.
5. Banco: teste de carga do nw-core (`tools/`). Salvamento só do que mudou: feito (2026-10-14, `inventory_patch`).
6. Os outros pontos da tabela "O que hoje vai contra as regras" (homing do golpe, bonecos, `ServerGather`, sacos no
   chão, NPCs e estações por registro).

## Ordem de trabalho

1. Contadores por sistema + relatório + `nw.sim` (sem mudar comportamento). Medir o código de hoje.
2. Atacar pela ordem do que custa mais (provável: busca de alvo da IA → grade espacial; IA dormindo longe de
   jogador; Iris + distância por tipo + dormência; inventário incremental; salvamento só do que mudou).
3. Medir de novo depois de cada item.
4. Canais (registro de servidores no nw-core, escolha do canal na entrada, grupo junto, chat entre canais) quando
   um canal encher.
