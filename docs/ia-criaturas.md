# IA das criaturas: árvores do jogo

As criaturas usam a árvore de comportamento do próprio jogo (Server.pak), interpretada no port. A semântica dos nós
vem da engenharia reversa em `re-local/ia-mobs.md`; o que o jogo roda só no servidor e foi cortado do cliente
(sensor de alvo, movimento) é feito no port com os números do slice da criatura.

## Peças

| Peça | Onde |
|---|---|
| Exportar árvore + perfil | `tools/make_ai.py` → `unreal/NWPort/Data/AI/<conjunto>.json` (roda em `tools/make_data.sh`) |
| Conferir sem a Unreal | `python3 tools/ai_sim.py <conjunto> Target_Dist=5 --tag Alerted --vezes 50 --trace` |
| Interpretador | `UNWBehaviorTreeComponent` (C++), ligado pelo `ANWCreature` quando existe `Data/AI/<conjunto>.json` |
| Sem árvore | `UNWBrainComponent` (estilos do port), como antes |

Fontes: árvore em `extract/serverpak/json/sharedassets/ai/behaviortrees/` (extração em
`re-local/ferramentas.md`); slice em `extract/serverpak/client/slices/characters/<x>.dynamicslice.json`.

## O que vem do jogo

- Árvore inteira (`<x>_Base.behaviortree` com as subárvores embutidas): leash, alerta, combate por distância, escolha
  de ataque por utilidade com sorteio, recargas nomeadas, timers, reposicionamento.
- Perfil do slice: visão ambiente e de combate (ângulo, distância), aggro de grupo (raio, quantos), perfil de alvo
  (`passive` = só briga se apanhar ou se o grupo brigar), replanejamento (frequência e chaves observadas), blackboard
  inicial (leash, velocidades), navegação por tipo (velocidade, giro, chegada, repath e destino padrão).

Lobo cinza: visão 12 m/75° parado e 30 m/180° em combate, aggro de grupo 7,5 m (até 5), corrida 9,2 m/s, leash de
combate 70 m. Javali: passivo, visão 12 m/85°, aggro de grupo 20 m, corrida 7 m/s.

## O que o port faz no lugar do servidor

| Parte | Como |
|---|---|
| Alvo | jogador mais perto dentro da visão ambiente com linha de visão; quem bate vira alvo; aggro de grupo; perde o alvo 15 s fora da visão de combate |
| Chaves do sensor | `Has_Target`, `Target_Pos`, `Target_Dist` (m), `Target_Angle` (graus, + à esquerda), `Orientation_Delta` (hipótese: frente do alvo − minha frente), `Target_Visible`, `Target_Vitals_State`, `Full_Body_Reaction`, `Last_Damage_Pos`, `Spawn_Pos`, `Waypoint_Pos` |
| Alcançável | `Target_Reachable` e `Target_Directly_Reachable` sempre 1 (sem navmesh) |
| Movimento | linha reta até o destino escolhido, na velocidade e no giro do tipo de navegação; desiste (falha) se demorar |
| Tarefas CAGE | tabela `TASKS` em `make_ai.py`: ataque → ação da criatura; espera; virar para o alvo; passo de esquiva; tag marcada no fim (Alert → `Alerted`) |
| `Difficulty_Tier` | 2 (sem fonte do valor por mob) |
| Tags de variante | `VARIANT_TAGS` (lobo = `Grey`); as do slice saem do TagComponent |

## Adicionar outra criatura

1. Conjunto de animações (`tools/sets/<x>.json`, `make_datatables.py`) como o javali e o lobo.
2. `CREATURES` em `make_ai.py`: conjunto → slice.
3. `TASKS`: cada tarefa CAGE da árvore (`python3 tools/ai_sim.py <x> --trace` e `tools/bt_dump.py` mostram quais) →
   ação, espera, virar ou passo. Tarefa sem entrada vira espera curta.
4. Tags de variante se a árvore testar alguma (`MannequinTagCondition` que nunca passa no simulador).
5. `python3 tools/make_ai.py` e conferir no simulador por faixa de distância.

## Diferenças conhecidas

- Sem navmesh: anda reto; a árvore acha que tudo é alcançável.
- Esquiva do lobo é um passo sem animação (o conjunto não tem a animação ainda).
- Atordoado: o plano pausa; no jogo a árvore tem o ramo de escape (`BreakoutWindow`), que o port não tem.
- Lobos timber usam a árvore do cinza.
