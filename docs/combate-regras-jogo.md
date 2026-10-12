# Regras gerais de combate do jogo

Levantamento dos números e regras do New World a partir dos dados extraídos. Objetivo: tirar os chutes do
`NWCombatComponent`. Números estruturados em `extract/research/combat_rules.json` (gerado por script, só leitura).

Grau de certeza em cada item:

- **dado**: valor lido direto de tabela, slice ou actionlist.
- **inferido**: deduzido de nome de campo, estrutura ou coerência entre valores. Não há código do jogo lido.

Abreviações de caminho:

| Sigla | Caminho |
|---|---|
| `DT/` | `extract/data/sharedassets/springboardentitites/datatables/` |
| `CAGE/` | `extract/serverpak/client/sharedassets/springboardentitites/playerdata/cage/` |
| `NPC/` | `extract/serverpak/client/sharedassets/springboardentitites/characters/` |
| `SL/` | `extract/serverpak/client/slices/characters/` (NPCs) e `extract/slicejson/slices/player.dynamicslice.json` (jogador) |
| `ADB/` | `extract/stage3/animations/mannequin/adb/` |
| `LY/` | `reference/lumberyard/dev/Code/CryEngine/` |

Fontes novas usadas aqui: as actionlists CAGE (`*.actionlist`, XML com a lógica de reação, bloqueio, esquiva e
exaustão) e as slices dos NPCs em `extract/serverpak/client/`. Também `strings` do `NewWorld.exe` para nomes de
campos e cvars.

## 1. Tremida de câmera

### O que é cada coluna

`DT/javelindata_camerashake.datasheet.json` tem as mesmas colunas de `IView::SShakeParams` do CryEngine
(`LY/CryCommon/IViewSystem.h`, `LY/CrySystem/ViewSystem/View.cpp`). Semântica pelo código do Lumberyard (dado):

| Coluna | Significado |
|---|---|
| `ShakeShiftX/Y/Z` | Deslocamento da câmera em metros, eixos da câmera (X direita, Y frente, Z cima). |
| `ShakeAngleX/Y/Z` | Rotação em radianos (`Ang3`). |
| `FadeInDuration`, `SustainDuration`, `FadeOutDuration` | Envelope em segundos. Total = soma dos três. |
| `Frequency` | **Período**, não Hz: segundos entre dois alvos aleatórios (`nextShake = frequency`). |
| `FlipVec` | Inverte o sinal a cada alvo. Oscilação completa = 2 × `Frequency`. |
| `Randomness` | Ruído extra = `Randomness` × média dos módulos dos eixos. |
| `ShakeChannel` | `shakeID`. Tremida nova no mesmo canal reinicia a anterior. |
| `IsSmooth` | Envelope linear passado por smoothstep `(3 − 2d)·d²`; interpolação cúbica entre alvos. |
| `Permanent` | Ignora `SustainDuration`. |
| `UpdateOnly` | Só atualiza a frequência de uma tremida em curso. |

O cvar `c_shakeMult` multiplica amplitude. O jogo tem `g_cameraShake` (liga/desliga) e opção do jogador
`Hud.LocalPlayer.Options.Controls.CameraShake` (strings do binário).

### IDs usados em combate

| ID | Desloc. (m) | Ângulo (rad) | Fade in / sustain / fade out (s) | Total (s) | Período (s) | Canal |
|---|---|---|---|---|---|---|
| `Small` | 0.01 / 0.01 / 0.01 | 0 / 0.005 / 0 | 0 / 0.1 / 0.1 | 0.2 | 0.015 | 1 |
| `Large` | 0.04 ×3 | 0.01 ×3 | 0.1 / 0.2 / 0.1 | 0.4 | 0.03 | 2 |
| `Small_short` | 0.02 ×3 | 0 | 0.1 / 0.15 / 0.1 | 0.35 | 0.03 | 3 |
| `Large_short` | 0.02 ×3 | 0.005 ×3 | 0.05 / 0.1 / 0.05 | 0.2 | 0.015 | 4 |
| `Large_long` | 0.04 ×3 | 0.01 ×3 | 0.1 / 0.25 / 0.1 | 0.45 | 0.03 | 5 |
| `WH_Hit_L1`..`L4` | Y 0.015–0.03, Z 0.015–0.03 | Y 0.003–0.018 | 0 / 0 / 0.2–1.0 | 0.2–1.0 | 0.03–0.08 | 7 |

Todos com `FlipVec=1`, `IsSmooth=1`. Os 40 IDs estão no JSON.

IDs que aparecem nas tabelas de dano e **não existem** na tabela de tremidas: `Medium`, `Smal`, `Large_l`, `1`, `0`,
`0.2`, `Small ` (com espaço). O binário tem a mensagem `GetCameraShakeData(...) failed`. Esperado: sem tremida
(inferido). Minúsculas (`small`, `large`) aparecem em procclips; não confirmado se o lookup ignora caixa.

### Quem treme

Colunas da tabela de dano (nomes no binário; papel inferido pelo nome):

| Coluna | Câmera que treme | Quando |
|---|---|---|
| `CameraShakeID` | quem bate | golpe acertou |
| `TargetCameraShakeID` | quem apanha | golpe acertou |
| `BlockCameraShakeID` | quem bate | golpe bloqueado |
| `BlockTargetCameraShakeID` | quem bloqueia | golpe bloqueado |
| `CritCameraShakeID` | quem bate | crítico |
| `CritTargetCameraShakeID` | quem apanha | crítico |

- A tabela do jogador (`javelindata_damagetable`) só tem as colunas `Crit*`. Espada e escudo: `Large_short` nas duas.
  Golpe normal de espada não treme câmera (dado: coluna ausente).
- IA: `TargetCameraShakeID` e `BlockTargetCameraShakeID` = `Small` na maioria (2200 e 1792 linhas). Damned: `Small`
  nas três colunas de alvo.
- Fora da tabela de dano: procclip `ShakeCamera` no ADB (`CameraShakeID` + `CameraShakeRange` em metros). A espada
  não usa; martelo, mangual e habilidades usam (ex. `WH_Swing_Light` no pesado do martelo).

## 2. Empurrão (knockback)

### Colunas

| Coluna | Significado | Certeza |
|---|---|---|
| `ImpactDistanceY` | Metros na direção do golpe. Positivo afasta do atacante; negativo puxa (`Attack_Force_Grab`, Y = −1). | inferido |
| `ImpactDistanceX` | Metros laterais. Positivo = direita do atacante (`1H_Flail_Ability_Charge_PushNoDmg_Right` = +2, `_Left` = −2). | inferido |
| `ImpactDistanceZ` | Metros para cima (`Damned_Priest_BlastAway_Spell` 0.75; `Attack_Force_Grab` −2). | inferido |
| `ImpactDecayRate` | Fator de decaimento da velocidade do empurrão, 0 a 1. | inferido |
| `Block*`, `Crit*` | Mesmo conjunto para golpe bloqueado e crítico. | dado |
| `UseAttackerPosForReaction` | True: direção = atacante→alvo. False: direção do golpe. | inferido |

Distribuição de `ImpactDecayRate` nas 3534 linhas com `ImpactDistanceY ≠ 0` (todas as tabelas): 0.8 em 3077;
0.85 em 95; 0.75 em 60; 0.7 em 46; 0 em 28; 1 em 5 (dado).

### Escalas

- Alvo: `DamageReceiverComponent.m_impactdistancescale`. Jogador 1.0; `damned_grunt` 1.2; 539 slices com 1.0, 96 com
  1.2, 2 com 0 (dado, `SL/`).
- Reação: `ReactionMotion` no actionlist. Jogador `Scale=1.0`. IA elite promovida `PushBackScale=0.15` (dado,
  `NPC/damned/cage/damned_reaction.actionlist`).
- Golpe que não gera reação: ação `Pushable` com `UsePushNoDecay=true`, dura 0.4 s (dado,
  `CAGE/playeractions_reaction.actionlist`).

### Curva (código, `docs/analise-re/combate-cliente.md` §1)

- Deslocamento até o tempo t da reação: `s(t) = D·(1 − r^(30·t))`, t limitado à duração da reação. Se a reação acaba
  antes, anda `D·(1 − r^(30·T))`, não `D` inteiro.
- `r ≈ 0`, `r ≈ 1` ou reação sem duração: tudo no primeiro quadro.
- `ImpactDecayRate ≤ 0`: vai até uma posição que o servidor grava no golpe (sem fonte para a posição).
- Eixos: Y na direção do golpe, X à direita dela, Z para cima; `PushBackScale` só reduz o que aponta para a frente.
- No port: `combate.empurrao_curva` em `UNWCombatComponent::Push`. Faltam Z e `PushBackScale` (`tools/re_check.py`).

## 3. HitStun

### Unidade

**Quadros a 30 fps** (1 = 1/30 s). Confirmado no código do `CHitStunContext` (`docs/analise-re/combate-cliente.md` §2):
a reação dura o início do procclip `HitStun` + `HitStun/30`, e o resto do fragmento toca na velocidade
`(duração − início)/(HitStun/30)`, limitada a (0.001, 3]. O port lê o início do procclip do `.adb` (coluna
`HitStunStart` das ações de reação).

Motivo:

- Nas reações `r_R1`..`r_R3` o procclip `HitStun` começa a 0.2 s (0.183–0.2 em 1254 de 1290 ocorrências, todas
  as ADBs; dado, `ADB/`).
- Os marcadores `CanStun` caem em R1 0.433 s, R2 0.767 s, R3 0.967 s (`ADB/player/player_1h_melee.adb`).
- Com quadros a 30 fps, `0.2 + HitStun/30` passa desses marcadores para os valores típicos (R1 15 → 0.70 s;
  R2 25 → 1.03 s; R3 30 → 1.20 s). Com centésimos (0.35 s; 0.45 s) a reação acabaria antes dos próprios marcadores.
- Todos os tempos do Mannequin são múltiplos de 1/30 s.

### Colunas `_AI`

Valem quando o **alvo é IA**. Valor 0 = sem override, usa a coluna normal. Inferido:

- Só a tabela do jogador tem `_AI`. Tabelas de IA não têm.
- `1H_Sword_Attack1`: `PowerLevel` 0, `PowerLevel_AI` 1; `HitStun` 15, `HitStun_AI` 18.
- `OH_Shield_Attack_Bash`: `PowerLevel` 3, `PowerLevel_AI` 0. Se 0 fosse override, o escudo não derrubaria IA.
- `DT/javelindata_aileveldisparity`: com a IA 10+ níveis acima, o golpe do jogador vira `AdjustedPowerLevel` 1 e
  `AdjustedHitStun` 20 (dado). Mostra que PL e HitStun do jogador contra IA são ajustáveis por alvo.

### Hit stop

`g_hitStopTime` = 0 por padrão e nenhum `.cfg` muda (código): **o jogo não tem hit stop**. O port também não
(`combate.hit_stop_s`).

## 4. PowerLevel e resistência

### PowerLevel é categoria de reação

Não é força contra poise. Cada valor escolhe uma animação na tabela de reação.

Alvo jogador (`CAGE/playerreactiontableharness.actionlist`, dado):

| PL | Reação |
|---|---|
| 0 | `r_R0` aditivo, não interrompe. Vira `r_R2` se `GRIT_Broken`. |
| 1 / 2 / 3 | `r_R1` / `r_R2` / `r_R3` direcional (F/B/L/R) |
| 4 | `r_R4` (derruba) |
| 5 | `r_R4_F_Overhead` |
| 6 | bloqueado: `r_B0_Break`; sem bloqueio: `r_R3` |
| 7 / 8 | `r_R4_F_Crumple` / `r_R4_F_Stumble` |
| 9 | `r_R2_*_Disarm` |
| 10 | `r_R1` com saída de stun (`StunBreakoutIncrement`) |

Regras do jogador (`CAGE/playeractions_reaction.actionlist`, dado):

- Bloqueado e stamina > 0: `UpperBodyReaction` (`r_B0`), só o tronco, não interrompe.
- Bloqueado e a stamina zerou nesse golpe: `r_B0_Break` (quebra de guarda).
- 1 ≤ PL < 4 sem bloqueio: `Reaction` completa, interrompe, limpa inputs de ataque.
- Grit ativo: só status `Stun` interrompe. O golpe vira `r_R0`.
- `Stagger_Resisted`: `r_R0`.
- Armas do jogador contra jogador: espada leve e pesado têm PL 0. Não interrompem outro jogador (dado,
  `javelindata_damagetable`).

Alvo IA, família damned (`NPC/damned/cage/damnedreactiontableharness.actionlist`, dado):

| PL | Sem bloqueio | Bloqueado |
|---|---|---|
| 0 | `r_R1` | `r_B1` |
| 1 | `r_R1` | `r_B1` |
| 2 | `r_R2` | `r_B2` |
| 3 | `r_R3` | `r_B3` |
| 4 | `r_R4` | `r_B3` |
| 5 / 8 | `r_R4_F_Overhead` / `r_Stun` | — |

Regras da IA damned (`NPC/damned/cage/damned_reaction.actionlist`, dado):

- `Solo`, `Solo-`: golpe `Light` (coluna `AttackType`) dá reação de corpo inteiro.
- `Heavy`: reação em `Solo`, `Solo-`, `Solo+`, `Named_Solo`, `Named_Solo+`.
- `Ability` com PL ≥ 1: reação sempre.
- Depois de uma reação completa: status `AI_Reaction_Limiter` por 2 s. Nesse tempo golpes leves só dão reação
  aditiva (anti-stunlock).
- `Boss`, `Raid*`, `EliteBoss`: não reagem.
- Grit ativo na IA bloqueia reação, exceto stun.

### Resistência (stagger)

Fonte: `GritComponent.m_defaultstaggerresistrating` nas slices (dado).

| Quem | Rating |
|---|---|
| Jogador | 0 |
| `damned_grunt`, `damned_axeman` | 20 |
| `boar`, `wolf_*` | 10 |
| Todas as slices de NPC | 20 em 398, 0 em 113, 10 em 76, 30 em 8 |

O binário tem os campos `staggerResist`, `staggerResistMod`, `totalStaggerDamage`, `ELSRM` no `GritComponent`.
Regra inferida:

- Dano de stagger = `BaseStaggerDamage` (arma) × `StaggerDmgModifier` (linha) × `CritStaggerDamageMultiplier` 1.5 no
  crítico (arma).
- Resistência = rating × `StaggerResistModifier` da linha do ataque **que o alvo está executando** × multiplicador de
  equip load (hoje todos 1).
- Dano < resistência → `Stagger_Resisted` → só `r_R0`.

Leitura das colunas por isso:

- `StaggerDmgModifier`: escala o dano de stagger do golpe. Espada leve 0.4, pesado 1.5, pesado carregado 2.4.
- `StaggerResistModifier`: nas linhas de IA, a armadura de stagger durante aquele ataque. Damned leve 0, pesado 1;
  `Reaction_Breakout` 10; lobo `Attack_02_Lunge` 1.6.
- Exemplo: espada leve contra damned parado = 36 × 0.4 = 14.4 < 20 → resistido. Pesado = 54 > 20 → reage. Contra
  javali/lobo (10) o leve passa.

### Grit do jogador

`GritComponent` na slice do jogador (dado): `m_min` 1000, `m_initmax` 10000, `m_maxnohittime` 30,
`m_setnoreactionwhenactive` true. Liga pelo procclip `ActivateGrit` nos fragmentos (pesados, algumas habilidades;
condição `GRIT_On_Basic_Attacks` para leves com perk). Quebra por dano acumulado (texto de lore). Unidade dos valores
1000/10000 sem fonte.

## 5. Crítico

| Item | Valor | Fonte |
|---|---|---|
| Chance base | Espada 7%, machadinha 8%, escudos 7%, IA típica 3% | `DT/javelindata_itemdefinitions_weapons` `CritChance` |
| Multiplicador | Espada 1.3, machadinha 1.4 | `CritDamageMultiplier` |
| Override por golpe | `DmgCoefCrit` 0 = usa a arma; 1.15–1.3 em 11 linhas | damagetable |
| Stagger no crítico | ×1.5 | `CritStaggerDamageMultiplier` |
| Backstab | Atacante a ≥ 135° da frente do alvo (cone traseiro de 90°) | `m_ishitbycritfrombehindminangle` = 2.3562 rad, jogador e 561 de 638 slices |
| Efeito do backstab | Conta como crítico (inferido pelo nome) | — |
| `NoBackstab` | Desliga o backstab do golpe | damagetable |
| "Atingido por trás" | ≥ 90° no jogador, ≥ 120° no damned | `m_ishitfrombehindminangle` |
| Headshot | `DmgCoefHead`: espada 1.3, machadinha 1.4 | damagetable |
| Volume da cabeça | Esfera r = 0.12 m no osso `hat` | `HitVolumeComponent` na slice do jogador |
| `NoHeadshot` | Desliga headshot | damagetable |

No crítico (dado, colunas): `CritHitStun` substitui `HitStun` (espada leve 15 → 20); `CritPowerLevel` substitui
`PowerLevel`; `CritImpactDistance*`/`CritImpactDecayRate` substituem o empurrão; tremidas `Crit*`.

Sem fonte:

- `CanCrit`: célula vazia = pode critar; só `False` desliga (código do parser). O JSON do nwbt grava vazio como
  `False`, então a coluna exportada não diz quem crita; o port não a usa.
- `CriticalHitsComponent.m_inactivitytime` = 5000 ms na slice do jogador.
- Se golpe corpo a corpo conta headshot.

## 6. Bloqueio

### Absorção e arco

- Absorção: `BLA*` = 1 em todas as armas e escudos do jogador. Bloqueio frontal absorve 100% do dano (dado na
  coluna; uso inferido). `BlockAbsorptionModifier` por golpe: 1, ou 1.2 nos leves de espada.
- Arco: 180°. O jogador tem `m_ishitfrombehindminangle` = 90°; golpe de trás não é bloqueado (inferido).
- `Unblockable` = True: não pode ser bloqueado (11 linhas na tabela do jogador).

### Stamina perdida (inferido)

```
perda = BlockStaminaDamage(arma de quem bate) × BlockStaminaDmgMod(linha) × max(1 − BlockStability/100, 0.1)
```

- `BlockStaminaDamage` e `BlockStability`: `DT/javelindata_itemdefinitions_weapons` (dado). Nomes no tooltip:
  "Block Stamina Damage" e "Blocking Stability".
- 0.1 = `DamageReceiverComponent.m_blockstabilitymin` (dado).
- Arma sem requisito de atributo: `BlockStability` −50% (`CAGE/playeractions_block.actionlist`, dado).

| Arma | BlockStaminaDamage | BlockStability |
|---|---|---|
| `SwordT2` | 36 | 22 |
| `HatchetT2` | 30 | 21 |
| Escudo redondo `1hShieldAT2` | 54 | 30 |
| Escudo pipa `1hShieldBT3` | 48 | 40 |
| Escudo torre `1hShieldDT4` | 42 | 50 |
| IA `1hSwordT2Damned` | 100 | 15 |

Exemplos contra escudo redondo (30): espada leve 36 × 0.4 × 0.7 = 10.1; pesado 37.8; pesado carregado 60.5.
Damned leve (0.1) 7.0; `Damned_1H_Melee_Primary` (0.35) 24.5; pesado (0.6) 42.

Sem confirmação: o custo `Block` = 10 de `javelindata_staminacosts_player` e `BlockStaminaCost` = 0.8 do vitals do
jogador. Nenhuma actionlist cobra `Block`.

### Quebra de guarda

- Stamina zera num golpe bloqueado → `r_B0_Break` + status `ExhaustedState_BlockBreak` (dado).
- `BlockPowerLevel` 6 quebra pela tabela de reação (dado).
- Sem bloqueio com stamina 0 ou exausto (dado, condições do `Block_UpperBody`).

### Stamina durante o bloqueio (dado, `CAGE/playeractions_block.actionlist`)

- Ao levantar a guarda (`Block_UpperBody_Raise`): regeneração desligada.
- Guarda ativa: regenera na taxa de bloqueio, 10/s (`m_blockregenrate`).
- Ao soltar: delay de 1 s antes de voltar a regenerar.

## 7. Stamina do jogador

Fonte principal: `StaminaComponent` na slice do jogador, com `m_usevitalsstaminavalues = false` (dado). Os valores do
vitals (`StaminaBaseTickRate` 40, `StaminaTickDelay` 3) não são usados.

| Item | Valor | Fonte |
|---|---|---|
| Máximo | 100 | `m_initmax` |
| Regeneração | 33.33/s | `m_regenrate` |
| Regeneração bloqueando | 10/s | `m_blockregenrate` |
| Delay após esquiva e após soltar bloqueio | 1 s | `SetStaminaRegenDelay RegenDelayTime="1.00"` |
| Durante esquiva | Regen desligada | `DisableStaminaRegen` em `Dodge` |
| Durante a quebra de guarda | Regen desligada | `Reaction` com `BlockingReaction`; procclip no `r_B0_Break` |
| Esquiva | 45 (pesado: 50) | `Dodge_Roll`, `Dodge_Roll_Slow` |
| Esquiva com passiva da machadinha | ×0.75 | `PayStaminaCost` em `playeractions_dodge` |
| Requisito da esquiva | stamina > 0 e não exausto | `IfStaminaValue > 0`, `Exhausted` |
| Sprint | 0 | `staminacosts_player` |
| Ataques de espada | 0 | `staminacosts_player` |
| `m_maxwindedtime` | 2 s | slice; uso sem fonte |

Exaustão (dado, `CAGE/playeractions_global.actionlist`, ação `Exhausted`):

- Entra com stamina ≤ 0%.
- Sai com stamina = 100%.
- Status `ExhaustedState` / `_DodgeDeplete`: `NoDodge`, sprint −15%. `_BlockBreak`: `NoDodge`.
- Exausto: não bloqueia, não esquiva. Ataca normal: a ação `Attack_Exhausted` (linhas `1H_Attack_Exhausted`,
  DmgCoef 0.5) está desligada no `playeractions_attack.actionlist` (`<Not><True/></Not>`).

Equip load (`DT/javelindata_encumbrancelimits`, linha `Player`): faixas 0/26/46/100% de 50 kg. Multiplicadores de
stamina e grit = 1 em todas. Dano: leve +15%, pesado −15%.

Clipe `PayStaminaCost` com `CostID=Stamina_Delay` (custo 0, `payOverTime`) cobre os ataques. Efeito não confirmado.

## 8. Dano e mitigação (essencial)

### IA

- Mitigação física 0.35 e elemental 0.35 em 6041 de 7408 linhas de vitals. Teto 0.9 (`MaxDefenseMitigationPercent`).
  Lost em geral 0.35 (dado, `DT/vitalstables/`).
- Dano base: `vitalsleveldata[Level].BaseDamage × DamageMod (vitals) × CategoryDamageMod` (inferido pela estrutura).
  Nível 5: 230.9; nível 10: 337.6. Categoria: `Solo` 1.0, `Solo-` 0.6, `Solo+` 1.2, `Named_Solo+` 1.25, `Elite` 1.4.
- IA 6+ níveis acima do jogador: +20% a +150% de dano e armadura (`javelindata_aileveldisparity`, dado).

### Jogador

- Base da arma: espada 64, machadinha 67 (`BaseDamage`).
- Atributos: `1 + ScalingStrength × ModifierValueSum(FOR) + ScalingDexterity × ModifierValueSum(DES)`. Espada 0.0085 FOR,
  0.0065 DES (inferido; `ModifierValueSum` em `javelindata_attribute*.json`).
- Fórmula de gear score e de mitigação por armadura do jogador: sem fonte nos dados.
- Vida base 856 (`HealthBaseMax` do vitals `Player`); +40 por ponto de Constituição acima de 5.

Ordem sugerida: `base × atributos × GS × DmgCoef × (crítico | headshot) × (1 + bônus) × (1 − mitigação)`.

## O que mudar no port

Constantes do `UNWCombatComponent` hoje chutadas (`Public/NWCombatComponent.h`):

| Constante | Hoje | Jogo | Fórmula / regra |
|---|---|---|---|
| `SecondsPerHitStun` | 0.01 | 1/30 = 0.0333 | Feito: `combate.quadros_por_s` e `HitStunStart` (código). |
| `Poise` + teste `PowerLevel >= Poise` | 0 | Sem poise | PL é categoria. Alvo jogador: PL 0 só flinch, 1–3 interrompe (sem grit), ≥4 derruba. Alvo IA: usar colunas `_AI` (0 = sem override); rating de stagger 20 (damned) / 10 (bichos) contra `BaseStaggerDamage × StaggerDmgModifier`; limiter de 2 s. |
| `ArmorMitigation` | 0.3 | 0.35 para IA | Vitals `PhysicalMitigation`; teto 0.9. Jogador: sem fórmula. |
| `BlockStaminaBase` + `BlockStaminaPerPower` | 15 + 8 × BPL | — | `BlockStaminaDamage × BlockStaminaDmgMod × max(1 − BlockStability/100, 0.1)`. Espada 36/22, escudo redondo 54/30, damned 100. |
| `StaminaRegenPerSecond` | 35 | 33.33 | Slice do jogador. |
| Regen bloqueando | 0 (para) | 10/s | Desligada só ao levantar a guarda. |
| `StaminaRegenDelay` | 1 | 1 | Ok. Aplicar após esquiva e ao soltar bloqueio. |
| Saída da exaustão | 50% | 100% | Ação `Exhausted`. |
| Esquiva | bloqueada se exausto | idem + exige stamina > 0 | Custo 45 (50 em carga pesada). Pode esquivar com menos de 45. |
| `BlockArcDegrees` | 180 | 180 | Ok (inferido). |
| `MaxStamina` | 100 | 100 | Ok. |
| `WeaponBaseDamage` | 100 | 64 (espada), 67 (machadinha) | Mais escala de atributo e GS. IA: `BaseDamage` do nível × modificadores. |
| `MaxHealth` | 1000 | 856 + 40/ponto de CON acima de 5 | Vitals `Player` e `javelindata_attributeconstitution`. |
| `HitStopSeconds` | 0.07 | 0 | Feito: `combate.hit_stop_s` (código). |
| Empurrão: força constante por 0.25 s | `ImpactDistanceY` | decaimento | Ver seção 2: total `D`, `v_k = v0 · r^k` por tick de 1/30 s, escala do alvo (damned 1.2). Usar `ImpactDistanceX` e `UseAttackerPosForReaction`. |
| Crítico | ausente | 7% espada, ×1.3 | Backstab a ≥ 135° = crítico. `CritHitStun`, `CritImpactDistanceY`, tremida `Large_short`. |
| Tremida de câmera | ausente | tabela | Só no crítico para golpes de espada; IA → alvo `Small`. |

## Sem fonte

Dano, crítico, backstab, grit e mitigação são calculados no servidor e não estão no cliente
(`docs/analise-re/combate-cliente.md`); ficam as regras inferidas acima.

- Posição do empurrão com `ImpactDecayRate ≤ 0`.
- Fórmula de gear score (só a faixa está no cliente: `itens.gs_faixa`) e de mitigação por armadura do jogador.
- Uso do custo `Block` (10), de `BlockStaminaCost` 0.8 e de `m_maxwindedtime` 2.
- Consumo do grit (pool 1000/10000).
- `m_inactivitytime`.
