# Rapieira (Rapier): levantamento do combate

Levantamento de 2026-10-12. Tudo que o jogo define para a rapieira do jogador, com fonte. Dados estruturados em
`extract/research/rapier.json` (mesmos números, mais as janelas completas de cada fragmento).

## Fontes

| Dado | Arquivo |
|---|---|
| Dano, coeficientes, stun, empurrão | `extract/data/.../datatables/javelindata_damagetable.datasheet.json` (linhas `1H_Rapier_*`) |
| Stamina | `javelindata_staminacosts_player` |
| Recargas | `javelindata_cooldowns_player` (`WeaponTag=Rapier`) |
| Árvore de maestria | `weaponabilities/javelindata_ability_rapier` (38 linhas) |
| Status effects | `javelindata_statuseffects_rapier` (15 linhas) |
| Arma | `javelindata_itemdefinitions_weapons` (`RapierT5`) |
| Câmera / trava de alvo | `javelindata_camerashake`, `combatsettings/javelindata_combatprofiles` e `javelindata_combatsettings` (`RapierGeneric`) |
| Animação e janelas (escopo do personagem) | `extract/models/animations/mannequin/adb/player/player_rapier.adb`, `player_1h_melee.adb`, `player.adb` |
| Janelas de dano dos ataques (escopo da arma) | `extract/models/animations/mannequin/adb/playermaleanims_1h_meleeattach.adb` (`AnimDbPath` do item) |
| Regras das ações (condições, efeitos, movimento) | `sharedassets/springboardentitites/playerdata/cage/playeractions_ability_rapier.actionlist`, `playeractions_attack`, `_dodge`, `_block`, `_reaction`, `_sheathe` (lidos com `nwbt cat`; não estão em `extract/`) |
| Eventos de animação | `animations/gameplay/character/player/male/temp_male.animevents` (`nwbt cat`): a rapieira só tem `audio_trigger` no saque |
| Malha | `objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf` (`nwbt cat`) |
| Textos | `extract/loc/localization/pt-br` e `en-us` |

Tags do Mannequin da arma: `MannequinTag = 1H_Melee,Rapier` (offhand `Melee_OH`, que não tem variantes com a rapieira).

### Dois achados que afetam o pipeline

1. **O dano dos ataques básicos não está no `.adb` do personagem.** `player_rapier.adb` não tem `CAGE-Damage` nem
   `WeaponEffects` nos ataques leves e no pesado. Eles estão no escopo da arma, `playermaleanims_1h_meleeattach.adb`
   (fragmentos só com `ProcLayer`, sem `AnimLayer`). O `adb_windows.best_fragment` exige `AnimLayer` e o
   `build_set.py` só lê os `.adb` do personagem, então hoje essas janelas não entram. As habilidades têm dano nos dois
   escopos.
2. **`tools/adb_windows.py` não soma o `ExitTime` que vem depois de um clipe vazio** (`<Procedural type="">`). Na
   sequência `B(0) vazio, B(0.4) Dano` ele põe o dano em 0 s, quando o certo é 0,4 s (cada `Blend.ExitTime` é relativo ao
   início do clipe anterior, vazio ou não). Na rapieira isso move os 5 golpes da Rajada (ferramenta: 0,000–0,334;
   correto: 0,400–1,212) e o segundo acerto do Light2 (0,400 → 0,533). Também afeta 40 variantes de `player_sword.adb`
   (na espada usada, só `Dodge_Roll_Fast`: `ApplyRotation_PostDodge` 0,067 → 0,167). Os números deste documento usam a
   soma correta. Nada foi alterado em `tools/`.

## A arma

`RapierT5` (as linhas T2–T5 têm os mesmos números; `TierNumber` 2 em todas):

| Campo | Rapieira | Espada (SwordT5), para comparar |
|---|---|---|
| BaseDamage | 61 | 64 |
| CritChance | 0,09 | 0,07 |
| CritDamageMultiplier | 1,4 | 1,3 |
| BaseStaggerDamage | 28 | 36 |
| BlockStaminaDamage / BlockStability | 28 / 17 | 36 / 22 |
| Escala | DES 0,0085, INT 0,0065 | FOR 0,0085, DES 0,0065 |
| Bloqueio | `BLA*` = 1, `CanBlockRanged` = true (alcance 50% vida, 50% stamina) | |
| Peso | 25 (`WeightOverride`) | |
| Tooltip | `1H_Rapier_PrimaryAttack` / `1H_Rapier_HeavyAttack` | |
| EquipType | `RSword_LeftHolster` (guardada no quadril esquerdo), `FireJoint handattach` | |
| Trava de alvo | perfil `RapierGeneric`: inicia até 25 m, mantém até 31 m | |

Malha: `objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf`, socket `right_hand_attach` (igual à
espada). O `.cdf` usa o esqueleto `objects/weapons/weapon_skels/lh_weapon_skel.chr`, a pele
`wep_me1h_sw_rapiera_t5.skin` e o material `wep_me1h_sw_rapiera_mat_0.mtl`. Pontos da lâmina (`CA_BONE` em
`master_root`, eixo Z): `vfx_Base_01` 0 m, `vfx_Mid_01` 0,5 m, `vfx_Melee_01` 0,9 m, `vfx_Tip_01` 0,925 m. Há também
`.cgf` estáticos `jav_wep_me1h_sw_rapiera_t1/t2/t3.cgf` na mesma pasta, caso o `.cdf` dê trabalho.

## Mecânica básica

### Fluxo do ataque básico

Todo ataque básico começa como carga (`attack_primary` → ação `Attack_Start_Charge`, `CanAttack_Heavy` = input
`attack_primary_virtual`). Não existe pesado separado: segurar o mesmo botão vira o pesado.

```
attack_primary ─► HeavyCharge (rapier_hvy_c1_stab, do início)
                   ├─ soltou antes de 0,667 s ──► Light1
                   └─ segurou até 0,667 s ─────► HeavyCharged (mesmo clipe, a partir de 74%)
Light1 ── input 0,333–0,533 (buffer) ou ≥0,533 ─► HeavyCharge_C2 ─ solto <0,667 ► Light2 / segurou ► HeavyCharged
Light2 ── input 0,300–0,733 (buffer) ou ≥0,733 ─► HeavyCharge_C3 ─ solto <0,594 ► Light3 / segurou ► HeavyCharged
Light3 ── input 0,231–0,867 (buffer) ou ≥0,867 ─► HeavyCharge (recomeça)
```

`Attack_Heavy_NotHeld` = `attack_primary` e `attack_primary_hold` soltos (`playeractions_attack`). A carga se
desfaz com `block` (`DeactivationCondition`).

Combo leve: **3 golpes, 4 acertos** (o segundo golpe acerta duas vezes, 66% cada). O passivo Vantagem Leve fala em
"ambos os ataques intermediários" por isso.

`1H_Rapier_HeavyAttack` (133%, clipes `rapier_spc_attack` / `rapier_1h_hvy_atk_01`) só é alcançado por
`Attack_Start_Charge_C4..C6`, que nenhum fragmento da rapieira aciona. O pesado real é o carregado (166%). O tooltip do
item ainda mostra a linha de 133%.

Habilidades interrompem os ataques: quase todo fragmento de ataque tem `CAGE-TransitionBuffered` 0–0,1 s e
`TransitionInstant` desde 0,1 s para Tondo, Brandir, Ripostar, Rajada e Fleche, e `TransitionInstant` desde 0 s para
Evadir.

### Ações básicas

Tempos em segundos do fragmento (já com o `speed` do clipe). `w` = janela no escopo da arma, `c` = escopo do
personagem. Stamina das tabelas: todos os ataques custam 0.

| Ação | Fragmento (tags) | Clipes | Dano (janela, DamageID) | Rastro | Pode encadear | Esquiva cancela | Outros |
|---|---|---|---|---|---|---|---|
| HeavyCharge | `Attack_Start_Charge` (1H_Melee+Rapier) | `rapier_hvy_c1_stab` | — | — | ver fluxo | 0–fim | afinco 0–0,733 (condição `GRIT_On_Basic_Attacks`) |
| Light1 | `Attack_Primary` | `rapier_prim_c1` @1,4 | w 0,333–0,433 `1H_Rapier_PrimaryAttack` | 0,175–0,319 `rapier_primary_1` | CanTransition 0,767; AnimEnd 1,357 | 0–0,2 e 0,5–fim | homing 0,067–0,433; bloqueio desliga 0,2–0,533 |
| Light2 | `Attack_Primary_Strike2` | `rapier_prim_c2` @1,3 | w 0,300–0,400 e 0,533–0,633 `1H_Rapier_PrimaryTwoThirdsAttack` (chaves atk1/atk2) | 0,275–0,375; 0,533–0,575 | CanTransition 0,9 | 0–0,233 e 0,7–fim | homing 0,067–0,4 |
| Light3 | `Attack_Primary_Strike3` | `rapier_prim_c3` @1,1 (início em 8%) | w 0,231–0,467 `1H_Rapier_PrimaryAttack_Ender` | 0,200–0,306 | CanTransition 0,9 | 0–0,167 e 0,5–fim | homing 0,067–0,4 |
| HeavyCharged | `Attack_Heavy_Charged` | `rapier_hvy_c1_stab` (início em 74%) | w 0,200–0,500 `1H_Rapier_HeavyAttack_Charged` | 0,175–0,5 `thrust_1` | CanTransition 1,2; nova carga 0,1–0,681 (buffer) | 0–0,033 e 0,5–fim | afinco 0–0,567; homing 0,131–0,5 |
| SprintAttack | `Attack_Sprint` → `Attack_Sprint_Land` | `rapier_sprint_downstrike` + `rswrd_sprint_downstrike_fall`; Land: `rapier_sprint_downstrike` (início em 40%) | Land: w 0,167–0,333 `1H_Rapier_SprintAttack` | Land 0–0,333 `melee_1` | Land: CanTransition 1,167 | Land: 1,167–fim | `CanLand_SprintAttack` a partir de 0,4 s; CritWindow 0,033–0,167; stamina `Attack_1H_Rapier_Run` = 0 |
| Dodge_F/B/L/R | `Dodge_Roll` (+`BackNav`/`LeftNav`/`RightNav`) | `cmbt_1h_quickstep_f/b/l/r` + `rapier_pose_add` | — | — | CanTransition 0,5; nova esquiva 0,267 (buffer) | — | invulnerável 0–0,5; paga stamina em 0,1 s |
| DodgeSlow / DodgeFast | `Dodge_Roll_Slow` / `Dodge_Roll_Fast` | `cmbt_1h_quickstep_hvy_f` + pose / `unarmed_dodgeroll_f_light_to_crch` + `rswrd_dodgeroll_f_light_to_crch` | — | — | CanTransition 0,5 | — | invulnerável 0–0,5 |
| BlockStart | `Block_UpperBody_Raise` (1H_Melee) | `rswrd_block_start` @2 | — | — | — | — | `BlockActive` 0–fim |
| BlockIdle | `Block_UpperBody` (1H_Melee) | `rswrd_run_block_ub_layer` | — | — | — | — | |
| BlockEnd | `Block_UpperBody_Lower` (1H_Melee+Rapier+Idle) | `rswrd_block_end` → `cmbt_idle_1h` + `rapier_pose_add` | — | — | — | — | |
| BlockReact / BlockBreak | `r_B1` / `r_B0_Break` (1H_Melee) | `rswrd_block_react_r1` / `rswrd_block_break` @2 | — | — | Break: CanTransition 0,5 | — | |
| Draw | `Unsheathe_UpperBody` | parado: `rapier_idle_draw_v1`; andando: `rapier_idle_draw_v1_ub` (`SetAnimByTag Idle`) | — | — | CanTransition_UpperBody 0,667 | 0,1–fim | `unsheathe_weapon` 0,233 |
| Sheath | `Sheathe_UpperBody` (From_1H_Melee, `player.adb`) | parado: `raxe_idle_sheath`; andando: `raxe_run_sheath_ub` @1,2 (início 15%) | — | — | — | 0,1–fim | `sheathe_weapon` 0,3; sem clipe próprio da rapieira |
| ExhaustedAttack | `Attack_Exhausted` (1H_Melee) | `rswrd_exhausted_atk` | w 0,533–0,733 `1H_Attack_Exhausted` (0,5) | — | CanTransition 1,467 | 1,467–fim | ataque com stamina zerada |
| Hit_F/B/L/R | `r_R1_*` (1H_Melee+Rapier) | `cmbt_idle_1h_r1_*` + `rapier_pose_add` | — | — | — | — | R2 F/L/R genéricos 1H (`unarmed_r2_*` + `rswrd_r2_*`); R2 B da rapieira |
| Stun | `r_Stun` → `r_Stun_Loop` → `r_Stun_End` | `cmbt_idle_r_stun_1h_start/loop/end` | — | — | — | — | |

**Esquiva.** A ação `Dodge` escolhe o fragmento pela carga de equipamento (`ActionConditionEquipLoadCategory`):
leve → `Dodge_Roll_Fast` (rolamento 1H genérico, `Dodge_Roll_Fast` = 45), média → `Dodge_Roll` (passo curto da
rapieira, `Dodge_Roll` = 45), pesada → `Dodge_Roll_Slow` (passo pesado, 50). Multiplicador do deslocamento:
Roll 1,28 (lateral 1,1), Fast 1,15, Slow 1,43. A regen de stamina volta 1,0 s depois. Invulnerável 0–0,5 s nos três.

**Bloqueio.** A rapieira bloqueia com a própria arma (`ActivateBlock ActivateMainHandWeapon`), com os clipes 1H
genéricos da espada (sem versão da rapieira). Levantar não paga custo; durante o bloqueio a regen fica desligada e
volta 1,0 s depois. Cada golpe bloqueado tira stamina pelo `BlockStaminaDmgMod` da linha do atacante e pela
`BlockStability` 17 da rapieira. A linha `Block` = 10 que o port já usa vem de `javelindata_staminacosts_player`.

**Sprint.** `Sprint` = 0 de stamina. Sem clipe próprio: `unarmed_sprint_veerslope_v2_blend` + `rswrd_sprint_veerslope_blend`.

## Habilidades ativas

Árvores: **Sangue** (`TreeId 0`, texto `ui_blood`) e **Graça** (`TreeId 1`, texto `ui_evade`). Maestria: `ui_rapier_mastery`
("Maestria de Rapieira"). Nenhuma habilidade custa stamina. A recarga começa quando termina o marcador
`Ability_Startup` (ação `SetCooldownTimer` em "Activate CoolDown after startup"); sem esse marcador, começa na entrada.

| Habilidade | AbilityID | Árvore (lin/col) | Recarga | DamageID | Fragmentos |
|---|---|---|---:|---|---|
| Tondo | `Ability_Rapier_Slash` | Sangue 1/4 | 6 s | `1H_Rapier_Slash` + sangramento `1H_Rapier_Slash_Bleed` | `Ability_Rapier_Slash` |
| Rajada | `Ability_Rapier_Flurry` | Sangue 0/0 | 16 s | `1H_Rapier_Flurry_Strike_1..5` (`_5_Mod4` com Finalizar) | `Ability_Rapier_Flurry` |
| Brandir e Terminar | `Ability_Rapier_Flourish` | Sangue 1/2 | 20 s | `1H_Rapier_Flourish`, `1H_Rapier_Finish` (`_Consume_Ult` com Fim Sangrento) | `Ability_Rapier_Flourish` → `Ability_Rapier_Finish` / `_Finish_Ultimate` |
| Ripostar | `Ability_Rapier_Riposte` | Graça 1/4 | 18 s | `1H_Rapier_Riposte_Stun` (`_Enhanced` com Mod3); provocação `1H_Rapier_Riposte_Taunt` | `Ability_Rapier_Riposte_Loop` → `_Riposte_Out` ou `_Riposte_Kickout` |
| Fleche | `Ability_Rapier_Aerial_Lunge_Intro` | Graça 0/0 | 20 s | `1H_Rapier_Aerial_Lunge`; continuação `1H_Rapier_Wake` | `_Aerial_Lunge_Intro` → `_Loop` → `_Stop` (ou `_Stop_Instant_Wake` → `Ability_Rapier_Wake`) |
| Evadir | `Ability_Rapier_Evasion` | Graça 1/2 | 12 s | ataque: `1H_Rapier_PrimaryAttack` | `Ability_Rapier_Evasion` (+`ForwardNav`/`BackNav`/`LeftNav`/`RightNav`) → `Ability_Rapier_Evade_Attack` |

### Tondo

Texto: "Corte tão rápido que estende o alcance da lâmina em até 5 m. Este golpe causa 50% do dano de arma e aplica uma
hemorragia que causa 10% do dano de arma adicional por segundo durante 10 s. (Pode se acumular até 3 vezes e renova
aplicações anteriores.)"

- Clipe `rapier_ability_wideslash` @2,2. `Ability_Startup` 0–0,3. Homing 0,133–0,5 (raio 2,75 m, 45°, sem aproximar).
- Dano 0,333–0,500, chave única `atk1` (1 acerto por alvo), três formas somadas:
  caixa 2 × 4,7 × 2,4 m centrada 3 m à frente da raiz (`Xform`, `UseEndAsCenter`, segue a inclinação da câmera);
  caixa 4,5 × 1,6 × 2,4 m centrada 4,5 m à frente; cápsula da arma raio 0,5 m (0,333–0,433).
  Alcance útil: de ~0,65 m a 5,35 m, 2 m de largura perto e 4,5 m no fim.
- `1H_Rapier_Slash`: 0,5, Slash, `StatusEffect Rapier_Bleed_DOT` só se causar dano. A habilidade troca a linha de dano
  do sangramento para `1H_Rapier_Slash_Bleed` (coef 1,0).
- Bloqueio desligado 0–0,856; AnimEnd 0,856; CanTransition 0,9; ataque básico em buffer 0,1–0,856.

### Rajada

Texto: "Use uma postura posada e lance uma série de 5 ataques rápidos e perfurantes, cada um causando 60% do dano de
arma. A Rajada pode ser cancelada a qualquer momento ao se esquivar ou usar outra habilidade. Esta habilidade tem
afinco."

- Clipe `rapier_ability_flurry` @1,75. Afinco (`ActivateGrit`) 0–1,367. `Ability_Startup` 0–0,366.
- Acertos (escopo da arma, cápsula raio 0,6 m a 0,5 m da base da lâmina, cada um com chave própria):
  0,400–0,475 `Strike_1`; 0,625–0,675 `Strike_2`; 0,762–0,800 `Strike_3`; 0,900–0,963 `Strike_4`;
  1,104–1,212 `Strike_5` (ou `Strike_5_Mod4` com Finalizar). Todos 0,6 Thrust; o 5º empurra 1,25.
- Homing raio 4 m em três trechos (0,062–0,733, 0,733–1,067, 1,067–1,367), aproxima até 1 m do alvo.
- Esquiva cancela desde 0,175; Evadir a qualquer momento; CanTransition 1,325; AnimEnd 2,563.

### Brandir e Terminar

Texto: "Brandir realiza um ataque, causando 60% do dano de arma, além de abalar e empurrar os inimigos. Reativar esta
habilidade ou desferir um ataque básico ao final de Brandir continuará a habilidade, realizando automaticamente
'Terminar'. Terminar lança um ataque para a frente, causando 120% do dano de arma direto e consumindo todas as
hemorragias da rapieira em qualquer alvo atingido, causando 110% do dano deles imediatamente."

- Brandir: `rapier_ability_flourish` @1,4. Dano 0,325–0,400 cápsula da arma raio 0,8 m e 0,338–0,362 cápsula raio 1,5 m
  centrada 2 m à frente (`xform`), mesma chave. `1H_Rapier_Flourish`: 0,6 Slash, PowerLevel 2, empurrão 3, HitStun 25.
  Homing 0,188–0,4 (raio 3 m, 60°). Avanço `Attack_Translation` ×1,3. AnimEnd 0,844.
- Continuação: condição `CanAttack_Finish` = `attack_primary` com a habilidade ativa; buffer 0,1–0,65, imediato desde
  0,65. Com as quatro melhorias (Mod1–Mod4) vai para `Ability_Rapier_Finish_Ultimate`. No actionlist só aparece o input
  de ataque básico; a reativação pela tecla da habilidade não tem condição própria.
- Terminar: `rapier_ability_flourish_lunge` @1,8, avanço ×2,6 (×2,9 em `Attack_Translation`). Homing 0,175–0,5
  (raio 7 m, 30°, aproxima). Dano 0,267–0,433, cápsula da arma raio 0,6 m, meia-altura 0,35. `1H_Rapier_Finish` 1,2 Thrust.
  CanTransition 0,967; AnimEnd 1,148. Não tem recarga própria.
- Consumo (linha `Ability_Rapier_Flourish` da árvore): `OnHit`, `RequireReaction`, `DamageTableRow 1H_Rapier_Finish`,
  `TargetStatusEffect Rapier_Bleed_DOT`, `RemoveTargetStatusEffectsList Rapier_Bleed_DOT`,
  `ConsumeTargetStatusEffectMult 1.1`, `ThreatDamage -0.9`. Com Fim Sangrento: `1H_Rapier_Finish_Consume_Ult` e 1,5.
- Afinco em Brandir e Terminar só com Com Talento (condição `Flourish_Finish_Grit`).

### Ripostar

Texto: "Fique em uma posição defensiva durante 1 s. Se sofrer um acerto durante esta habilidade, rebaterá o atacante
causando 50% do dano de arma, abalando-o e atordoando-o por 2 s. Você fica brevemente invulnerável após um Ripostar
bem-sucedido." Com gema Cornalina: provoca por 4 s os inimigos num raio de 5 m.

- Entrada: a ação usa direto o fragmento `Ability_Rapier_Riposte_Loop` (`rapier_ability_riposte_start` →
  `rapier_ability_riposte_loop`). Aplica `Rapier_Riposte_Timer` (1 s; removido ao sair). Movimento ×0,001 (parado).
  Bloqueio desligado. `Spell_Taunt_AOE` 0–0,3 só com `HasTauntGem`.
- `Rapier_Riposte_Timer`: absorção 200% em todos os tipos (não toma dano), resistência 100% a aflições, imune a stun
  (`Stun=-1.0`), afinco, `IgnoreDebuffs`. As ações de reação (`playeractions_reaction`) saem quando o alvo tem esse
  efeito: não há reação de golpe durante a postura.
- Gatilho `Riposte_Attack`: `IfDamagedByPowerLevel >= 0` com `FailOnNoReactionDamage=true`, tag `Rapier` e o timer
  ativo → ação `Ability_Rapier_Riposte_Out` (iniciada pelo servidor).
- Sem acerto: em 1,0 s o Loop troca para `Ability_Rapier_Riposte_Kickout` (`rapier_ability_riposte_cancel`,
  CanTransition 0,6).
- Contra-ataque (`Riposte_Out`): `rapier_ability_riposte_end` @1,6. Invulnerável 0–0,567. Homing 0–0,567 (raio 2,75 m,
  aproxima até 1 m). Dano 0,433–0,567: cápsula raio 0,5 m, meia-altura 0,4 m, 0,7 m à frente e 1 m de altura
  (`Xform`). `1H_Rapier_Riposte_Stun`: 0,5 Strike, PowerLevel 3, stagger 2,4, aplica `Rapier_Riposte_Stun` (2 s,
  quebra no primeiro dano recebido). Com Consequência Duradoura: `_Enhanced` (2,5 s). Tremor `Large_short` em 0,433.
  Aplica `Rapier_Riposte_Uninterruptible_Riposte` (0,875 s). Ataques e habilidades em buffer 0,5–0,9; CanTransition 0,933.

### Fleche

Texto: "Sai do chão, avançando por 10 m em um movimento de perfuração que transpassa os inimigos, causando 145% do
dano de arma."

- Intro: `rapier_ability_aeriallunge_start` @2,8. Alinha à câmera 0–0,267. `Ability_Startup` 0–0,333. Vai para o Loop
  em 0,381. Movimento da animação ×1,3 em `Attack_Translation`, herda até 6 m/s.
- Loop: `rapier_ability_aeriallunge_loop` @1,5, movimento ×0,8, gravidade ×3. Dano desde 0 s, cápsula raio 1,1 m,
  meia-altura 0,25, offset (0; 0,5; 0,5) m, `1H_Rapier_Aerial_Lunge` 1,45 Thrust (`WeaponCategory Sword_Fleche`).
  Sai para `Stop` em 0,333 s. As chaves `atk16..atk60` começam em 0,502 s, depois da saída: na prática 1 acerto por alvo.
- Em Intro, Loop e Stop: sem colisão ator-ator e jogador-jogador (atravessa).
- Stop: `rapier_ability_aeriallunge_end` @3, movimento forçado (`CharacterRelativeMovement` Speed Y 5, Decel 50).
  Aplica `Rapier_Fleche_Backstab` (5 s). CanTransition 0,5.
- Continuação (Interrupção): `CanAttack_Wake` = `attack_primary` + Mod1, Mod2 e Mod3 ativos. No Loop vai para
  `Stop_Instant_Wake` e daí para `Ability_Rapier_Wake` em 0,133 s. `Wake`: `rapier_ability_aeriallunge_stab` @1,12,
  dano 0,167–0,333 `1H_Rapier_Wake` (1,15), CanTransition 0,533.
- Os 10 m saem do root motion dos clipes com esses multiplicadores. Não medi (precisa do clipe convertido).

### Evadir

Texto: "Faz um passo lateral pequeno na direção atual do movimento, cancelando qualquer atividade atual e oferecendo uma
invulnerabilidade momentânea. Ataques leves feitos durante Evadir são excepcionalmente rápidos."

- Direção: tag de navegação pelas condições `Evade_Back` (180° ± 50), `Evade_Left` (270° ± 45), `Evade_Right`
  (90° ± 45); senão `ForwardNav`. As laterais e a de trás só valem com trava de câmera, `AttackFacing` ou
  `ForceCameraFacing`; sem isso o passo sai para a frente, na direção do input.
- `rapier_ability_evade_f/b/l/r` @1,5. Invulnerável 0–0,444. Movimento: frente ×1,5, trás ×2,3, lados ×1,7.
  Sem custo de stamina. Zera o afinco. Remove `Rapier_Riposte_Timer`.
- Ataque leve: `attack_primary` em buffer 0–0,067 e imediato desde 0,067 → `Ability_Rapier_Evade_Attack`.
  Pesado (`CanAttack_Heavy`) desde 0,3 → carga. CanTransition 0,333. Esquiva cancela desde 0,2.
- Ataque de Evadir: `rapier_prim_c1` @1,4 começando em 20% do clipe. Dano 0,067–0,200 `1H_Rapier_PrimaryAttack`.
  Homing raio 3,25 m, move até 1,25 m. Encadeia para a carga C2 (→ Light2): buffer 0,067–0,267, imediato desde 0,267.
- Evadir pode ser usado de quase todo fragmento de ataque e habilidade (`CanAttack_Evasion` imediato desde 0 s).

## Passivas, melhorias e definitivas

`kind`: `passive` = melhoria ou passiva; `ultimate` = linha 5 da árvore.

| Árvore | Lin/Col | AbilityID | Nome | Tipo | Efeito (texto do jogo) | Dados-chave |
|---|---|---|---|---|---|---|
| Graça | 1/0 | `Rapier_Aerial_Lunge_Mod1` | Investida Rápida | passive | Abater com esta habilidade reduz o tempo de recarga dela em 80%. | OnKill=True; RequireReaction=True; DamageTableRow=1H_Rapier_Aerial_Lunge; CooldownTimer=-0.8; CDRImmediatelyOptions=AbilitySpecific |
| Graça | 2/0 | `Rapier_Aerial_Lunge_Mod2` | Parte de Trás | passive | Depois de executar Fleche, seu próximo acerto crítico dentro de 5 s causará mais 15% de dano base. | OnCrit=True; AttackType=Light,Heavy,Ability; CritDamage=0.15; StatusEffect=Rapier_Fleche_Backstab |
| Graça | 3/0 | `Rapier_Aerial_Lunge_Mod3` | Interrupção | passive | Reativar esta habilidade ou pressionar o ataque básico a qualquer momento durante Fleche interromperá o golpe e realizará um ataque estático como continuação, causando 115% de dano . | OnKill=True; RequireReaction=True; DamageTableRow=1H_Rapier_Wake; CooldownTimer=-0.8; CDRImmediatelyOptions=AbilitySpecific |
| Graça | 0/1 | `Rapier_BonusDmgLowStamina` | Desespero | passive | Causa mais 10% de dano base quando seu Vigor está abaixo de 40%. | OnHit=True; BaseDamage=0.1; MyStaminaPercent=40 |
| Graça | 2/1 | `Rapier_BonusCDOnCrit` | Cortinas Vermelhas | passive | Os acertos críticos reduzem todos os tempos de recarga em 5%. (Recarga de 0,26.) | OnCrit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; CooldownTimer=-0.05; CDRImmediatelyOptions=ActiveWeapon; ActivationCooldown=0.26 |
| Graça | 4/1 | `Rapier_Evasion_Mod4` | Inspirar | passive | Ganhe 20 de vigor após uma esquiva bem-sucedida de um ataque inimigo. | OnHitTakenWhileInvulnerable=True; RequireReaction=True; SelfApplyStatusEffect=Rapier_Evasion_Mod1 |
| Graça | 2/2 | `Rapier_Evasion_Mod1` | Crescendo | passive | Cada Ataque Leve individual bem-sucedido reduz o tempo de recarga desta habilidade em 25%. (Tempo de recarga: 0,1 s) | OnHit=True; RequireReaction=True; AttackType=Light; CooldownTimer=-0.25; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.1 |
| Graça | 3/2 | `Rapier_Evasion_Mod2` | Allegro | passive | Ganhe 12% de Pressa durante 3 s após uma esquiva bem-sucedida de um ataque inimigo. (Pressa aumenta a velocidade de movimento.) | OnHitTakenWhileInvulnerable=True; RequireReaction=True; SelfApplyStatusEffect=Rapier_Evasion_Mod2 |
| Graça | 4/2 | `Rapier_Evasion_Mod3` | Adagio | passive | Esquivar para a frente concede 10% de aumento do dano base em seu próximo Ataque Leve. Termina ao atingir ou após 1 s. | OnHit=True; AttackType=Light; BaseDamage=0.1; StatusEffect=Rapier_Evade_Forward_Damage |
| Graça | 5/2 | `Rapier_BonusDMGAfterAbility` | Impulso | ultimate | Causa mais 25% de dano base em seu próximo ataque leve ou pesado após executar uma habilidade. Termina ao atingir ou após 3 s. | OnHit=True; AttackType=Light,Heavy; BaseDamage=0.25; StatusEffect=Rapier_BonusDMGAfterAbility |
| Graça | 0/3 | `Rapier_BonusStamRegenOnKill` | Respiração Controlada | passive | Recebe 3 de vigor com qualquer acerto. | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; SelfApplyStatusEffect=Rapier_BonusStamOnHit |
| Graça | 1/3 | `Rapier_BonusDMGMaxHP` | Perfeccionista | passive | Causa mais 10% de dano base quando sua Saúde está cheia. | OnHit=True; BaseDamage=0.1; MyHealthPercent=99 |
| Graça | 3/3 | `Rapier_BonusSpeedOnKill` | Rapidez | passive | Recebe 3% de pressa durante 4 s em acertos de rapieira. Pode se acumular até 5 vezes. (Pressa aumenta a velocidade de movimento.) | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; SelfApplyStatusEffect=Rapier_BonusSpeedOnKill |
| Graça | 2/4 | `Rapier_Riposte_Mod1` | Sal na Ferida | passive | Se ripostar acertar um alvo com sucesso, receberá energizado, aumentando o dano causado em 10% durante 3 s. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Riposte_Stun,1H_Rapier_Riposte_Stun_Enhanced; SelfApplyStatusEffect=Rapier_Riposte_Empower |
| Graça | 3/4 | `Rapier_Riposte_Mod2` | Prioridade | passive | Reduz em 20% os tempos de recarga de outras habilidades de rapieira ao acertar um contra-ataque com Ripostar. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Riposte_Stun,1H_Rapier_Riposte_Stun_Enhanced; CooldownTimer=-0.2; CDRImmediatelyOptions=AllExcept |
| Graça | 4/4 | `Rapier_Riposte_Mod3` | Consequência Duradoura | passive | Aumenta o atordoamento de Ripostar para 2,5 s. | OnHit=True |
| Sangue | 1/0 | `Rapier_Flurry_All_In` | Subjugar | passive | Cada acerto de Rajada causa 25% a mais de dano no bloqueio. | OnBlockedHit=True; BlockDamage=0.25 |
| Sangue | 2/0 | `Rapier_Flurry_Haste` | Golpes Fugazes | passive | Cada acerto de Rajada reduz seu tempo de recarga em 5%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Flurry_Strike_1,1H_Rapier_Flurry_Strike_2,1H_Rapier_Flurry_Strike_3,1H_Rapier_Flurry_Strike_4,1H_Rapier_Flurry_Strike_5,1H_Rapier_Flurry_Strike_5_Mod4; CooldownTimer=-0.05; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.1 |
| Sangue | 3/0 | `Rapier_Flurry_Weakness` | Até os Ossos | passive | Cada acerto de Rajada estende a hemorragia da rapieira em 1 s. (Acúmulos futuros desta hemorragia estendida também serão prolongados à sua maior duração.) | OnHit=True; TargetStatusEffectDurationList=Rapier_Bleed_DOT; TargetStatusEffectDurationMod=1 |
| Sangue | 4/0 | `Rapier_Flurry_Mod4` | Finalizar | passive | O último acerto de Rajada aplicará um acúmulo de Hemorragia ao inimigo, causando 10% do dano de arma por segundo durante 10 s. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Flurry_Strike_5_Mod4; OtherApplyStatusEffect=Rapier_Bleed_DOT |
| Sangue | 0/1 | `PassiveRapier02` | Golpes Renovadores | passive | Reduz todos os tempos de recarga da rapieira em 1% com qualquer acerto. (Máximo de 5 reduções por ataque.) | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; CooldownTimer=-0.01; CDRImmediatelyOptions=ActiveWeapon; NumberOfTrackedHits=5 |
| Sangue | 1/1 | `PassiveRapier04` | Infalível | passive | Causa mais 5% de dano base contra alvos com Hemorragia de Rapieira. | OnHit=True; TargetStatusEffect=Rapier_Bleed_DOT; BaseDamage=0.05 |
| Sangue | 3/1 | `PassiveRapier03` | Punção Pesada | passive | Qualquer ataque pesado contra um inimigo com hemorragia estende as hemorragias de rapieira em 2 s. (Acúmulos futuros desta hemorragia estendida também serão prolongados à sua maior duração.) | OnHit=True; AttackType=Heavy; TargetStatusEffectDurationList=Rapier_Bleed_DOT; TargetStatusEffectDurationMod=2 |
| Sangue | 2/2 | `Rapier_Flourish_Mod1` | Com Talento | passive | Ganhe afinco para Brandir e Terminar.   (Afinco impede que você sofra abalo ao receber ataques.) |  |
| Sangue | 3/2 | `Rapier_Flourish_Mod2` | Arrogância | passive | Ganha 20 de vigor ao acertar com sucesso um alvo com Finalizar. | OnHit=True; DamageTableRow=1H_Rapier_Finish,1H_Rapier_Finish_Consume_Ult; SelfApplyStatusEffect=Rapier_Finish_Stam_Mod2 |
| Sangue | 4/2 | `Rapier_Flourish_Mod3` | Combustível | passive | Quando a Hemorragia de Tondo aplica dano, o tempo de recarga dessa habilidade é reduzido em 9%. (Máximo de 1 redução por ativação.) | OnHit=True; RemoteDamageTableRow=1H_Rapier_Slash_Bleed; CooldownTimer=-0.09; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.5 |
| Sangue | 5/2 | `Rapier_Flourish_Mod4` | Fim Sangrento | ultimate | Terminar causa 150% do dano de hemorragia da rapieira em vez de apenas 110%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Finish_Consume_Ult; TargetStatusEffect=Rapier_Bleed_DOT; ConsumeTargetStatusEffectMult=1.5 |
| Sangue | 0/3 | `PassiveRapier05` | En Garde | passive | Causa mais 10% de dano base quando seu alvo tem mais de 50% de Saúde. | OnHit=True; BaseDamage=0.1; TargetHealthPercent=50 |
| Sangue | 2/3 | `PassiveRapier01` | Vantagem Leve | passive | Aumenta em 8% o dano base de ambos os ataques de corte intermediários na sequência de ataques leves. | OnHit=True; DamageTableRow=1H_Rapier_PrimaryTwoThirdsAttack; BaseDamage=0.08 |
| Sangue | 2/4 | `Rapier_Slash_Mod1` | Sede de Sangue | passive | Ao aplicar o primeiro acúmulo de hemorragia a um adversário, o tempo de recarga de Tondo é reduzido em 10%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Slash; TargetStatusEffect=Rapier_Bleed_DOT; CooldownTimer=-0.1; CDRImmediatelyOptions=AbilitySpecific |
| Sangue | 3/4 | `Rapier_Slash_Mod2` | E De Novo | passive | Se você acertar apenas 1 alvo, mesmo se o ataque for bloqueado, o tempo de recarga de Tondo será reduzido em 25%. | RequireReaction=True; DamageTableRow=1H_Rapier_Slash,1H_Rapier_Slash_Bleed; CooldownTimer=-0.25; CDRImmediatelyOptions=AbilitySpecific; NumberOfTrackedHits=1; Duration=2; AfterAction=Ability_Rapier_Slash |
| Sangue | 4/4 | `Rapier_Slash_Mod3` | Espaçamento Adequado | passive | Aumenta o dano base do acerto inicial de Tondo em 100% quando o alvo já está infligido com Hemorragia da Rapieira. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Slash; TargetStatusEffect=Rapier_Bleed_DOT; BaseDamage=1 |

Observações sobre a árvore:
- `Rapier_Evasion_Mod1` mostra o texto de "Crescendo" (`@Rapier_06_Mod4`) e `Rapier_Evasion_Mod4` o de "Inspirar"
  (`@Rapier_06_Mod1`). Os dados batem com o texto mostrado, não com o número do ID.
- `Rapier_Aerial_Lunge_Mod3` (Interrupção) tem os campos `OnKill`/`CooldownTimer -0.8` copiados de Mod1; a regra real
  está no actionlist (`CanAttack_Wake`).
- "Golpes Renovadores" e "Cortinas Vermelhas" contam acertos por ação: toda ação de ataque da rapieira faz
  `ClearAbilityHits` desses dois ao sair (máx. 5 reduções por ataque no primeiro).

## Status effects

| StatusID | Nome | Duração | Regra |
|---|---|---|---|
| `Rapier_Bleed_DOT` | Hemorragia | 10 s (máx. 18) | Tick 1,01 s; cada tick causa 10% do dano de arma do atacante (`HealthModifierDamageBased -0.1`) × coef da linha de override (Tondo: `1H_Rapier_Slash_Bleed` 1,0; Finalizar da Rajada: `Common_DamageOverTime_Override` 1,0, sem reação). Até 3 acúmulos; nova aplicação renova a duração dos anteriores (`RefreshDuration`, `InheritTotalDuration`); mínimo 0,5 s entre aplicações. Extensões: +1 s por acerto de Rajada (Até os Ossos), +2 s por pesado (Punção Pesada), até 18 s. Sai na morte. |
| `Rapier_Riposte_Timer` | (timer) | 1 s | Absorção 200% (sem dano), 100% contra aflições, imune a stun, afinco, `IgnoreDebuffs`. |
| `Rapier_Riposte_Stun` | Ripostar | 2 s | `Stunned`; termina no 1º dano recebido (`StopOnHitCount 1`, `OnHitTaken`). |
| `Rapier_Riposte_Stun_Enhanced` | Ripostar | 2,5 s | Igual, com Consequência Duradoura. |
| `Rapier_Riposte_Uninterruptible_Riposte` | (afinco) | 0,875 s | Afinco e imune a stun durante o contra-ataque. |
| `Rapier_Riposte_Uninterruptible` | (afinco) | 3 s | Não é aplicado pelas ações da rapieira; só citado em `playeractions_reaction`. |
| `Rapier_Riposte_Empower` | Energizado Ripostando | 3 s | +10% de dano em todos os tipos (Sal na Ferida). |
| `Rapier_Evasion_Mod1` | Desvio | instantâneo | +20 de stamina (Inspirar). |
| `Rapier_Evasion_Mod2` | Desvio | 3 s | +12% de velocidade (Allegro). |
| `Rapier_Finish_Stam_Mod2` | Desvio | instantâneo | +20 de stamina (Arrogância). |
| `Rapier_Evade_Forward_Damage` | Energizado | 1 s | Marca para o +10% do próximo leve (Adagio). |
| `Rapier_Fleche_Backstab` | — | 5 s | Marca para o +15% de dano crítico (Parte de Trás). |
| `Rapier_BonusDMGAfterAbility` | — | 3 s | Marca para o +25% do próximo leve/pesado (Impulso). Removido ao guardar a arma (ação `Rapier_Momentum_Active`). |
| `Rapier_BonusStamOnHit` | — | instantâneo | +3 de stamina (Respiração Controlada). |
| `Rapier_BonusSpeedOnKill` | Pressa | 4 s | +3% de velocidade por acúmulo, até 5, mínimo 0,1 s entre aplicações (Rapidez). |

## Linhas de dano

`WeaponCategory` Sword em todas (Fleche: `Sword_Fleche`). Todas com `CanCrit` (menos a de provocação).
`PowerLevel` vazio na tabela = 0 (escala ainda não confirmada, ver `plano-mecanicas.md`).

| DamageID | Tipo | Coef | Cabeça | PowerLevel (bloq) | HitStun / Bloq / Crit | Empurrão Y | Stagger | Stamina no bloqueio | Extra |
|---|---|---:|---:|---|---|---:|---:|---:|---|
| `1H_Rapier_PrimaryAttack` | Thrust | 1 | 1.2 | 0 (0) | 20 / 15 / 20 | 0.6 | 0.4 | 0.4 | AttackType=Light |
| `1H_Rapier_PrimaryTwoThirdsAttack` | Thrust | 0.66 | 0.86 | 0 (0) | 20 / 15 / 20 | 0.6 | 0.35 | 0.35 | AttackType=Light |
| `1H_Rapier_PrimaryAttack_Ender` | Thrust | 1 | 1.2 | 0 (0) | 20 / 15 / 20 | 0.6 | 0.4 | 0.4 | AttackType=Light |
| `1H_Rapier_HeavyAttack_Charged` | Thrust | 1.66 | 1.3 | 0 (0) | 25 / 25 / 30 | 2 | 2.4 | 2.4 | IsTaunt=True; TauntDuration=0.5; AttackType=Heavy |
| `1H_Rapier_HeavyAttack` | Thrust | 1.33 | 1.3 | 0 (0) | 25 / 25 / 30 | 2 | 1.5 | 1.5 | IsTaunt=True; TauntDuration=0.5; AttackType=Heavy |
| `1H_Rapier_SprintAttack` | Thrust | 1.1 | 1.3 | 3 (2) | 30 / 10 / 30 | 3 | 0.75 | 0.75 | AttackType=Ability |
| `1H_Rapier_Slash` | Slash | 0.5 | 1.3 | 0 (0) | 20 / 15 / 20 | 0.3 | 0.5 | 0.5 | StatusEffect=Rapier_Bleed_DOT; AttackType=Ability |
| `1H_Rapier_Slash_Bleed` | Slash | 1 | 1 | 0 (0) | 20 / 15 / 20 | 0.3 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_1` | Thrust | 0.6 | 0.65 | 0 (0) | 20 / 15 / 20 | 0 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_2` | Thrust | 0.6 | 0.69 | 0 (0) | 20 / 15 / 20 | 0 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_3` | Thrust | 0.6 | 0.75 | 0 (0) | 20 / 15 / 20 | 0 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_4` | Thrust | 0.6 | 0.8 | 0 (0) | 20 / 15 / 20 | 0 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_5` | Thrust | 0.6 | 1 | 0 (0) | 20 / 15 / 20 | 1.25 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flurry_Strike_5_Mod4` | Thrust | 0.6 | 1 | 0 (0) | 15 / 10 / 20 | 1.25 | 0.3 | 0.3 | AttackType=Ability |
| `1H_Rapier_Flourish` | Slash | 0.6 | 1.2 | 2 (2) | 25 / 25 / 25 | 3 | 0.4 | 0.4 | AttackType=Ability |
| `1H_Rapier_Finish` | Thrust | 1.2 | 1.3 | 0 (0) | 25 / 25 / 30 | 0 | 0.4 | 0.4 | AttackType=Ability |
| `1H_Rapier_Finish_Consume_Ult` | Thrust | 1.2 | 1.3 | 0 (0) | 25 / 25 / 30 | 0 | 0.4 | 0.4 | AttackType=Ability |
| `1H_Rapier_Riposte_Stun` | Strike | 0.5 | 0.7 | 3 (3) | 25 / 25 / 25 | 1.5 | 2.4 | 2.4 | StatusEffect=Rapier_Riposte_Stun; AttackType=Ability |
| `1H_Rapier_Riposte_Stun_Enhanced` | Strike | 0.5 | 0.7 | 3 (3) | 25 / 25 / 25 | 1.5 | 2.4 | 2.4 | StatusEffect=Rapier_Riposte_Stun_Enhanced; AttackType=Ability |
| `1H_Rapier_Riposte_Taunt` | Strike | 0 | 1 | 0 (0) | 0 / 0 / 0 | 0 | 0 | 0 | IsTaunt=True; TauntDuration=4; NoReaction=1; IgnoreInvulnerable=1; AttackType=Ability |
| `1H_Rapier_Aerial_Lunge` | Thrust | 1.45 | 1.45 | 0 (0) | 25 / 20 / 25 | 0 | 0.4 | 0.4 | AttackType=Ability |
| `1H_Rapier_Wake` | Thrust | 1.15 | 1.4 | 0 (0) | 25 / 20 / 30 | 1 | 0.4 | 0.4 | AttackType=Ability |

`Common_DamageOverTime_Override` (sangramento da Rajada com Finalizar): coef 1,0, Standard, `NoReaction`, `AttackType DOT`.
`1H_Attack_Exhausted` (ataque com stamina zerada): 0,5, Standard, PowerLevel 2.

Tremores de câmera citados: `Small` (deslocamento 0,01, 0,1 s) nos acertos normais; `Large_short` (0,02, 0,1 s) nos
críticos e no contra-ataque do Ripostar (`javelindata_camerashake`).

## Locomoção e loops

| Item | Fragmento | Clipes |
|---|---|---|
| Idle | `Idle` (1H_Melee+Rapier) | `cmbt_idle_1h` + `rapier_pose_add` (aditivo, loop) |
| Idle exausto | `Idle` (+Exhausted) | `Unarmed_Exhaust_Idle` + `rswrd_exhaust_idle` |
| Strafe | `Strafe` (1H_Melee+Rapier) | `melee1h_cmbt_nav_strafe_blend` (começa em 50%) + `v2_rapier_cmbt_walk_layer_l` + `v2_rapier_cmbt_walk_layer_r` (weight 0,5) |
| Sprint | `Sprint` (1H_Melee) | `unarmed_sprint_veerslope_v2_blend` + `rswrd_sprint_veerslope_blend` (sem versão da rapieira) |
| Início do sprint | `Sprint_Start` (1H_Melee) | `unarmed_sprint_start_strafe_turn_v2_blend` + `rswrd_sprint_start_strafe_turn` |
| Parada do sprint | `Nav_Stop` (+Stand+Sprint) | `rswrd_sprint_stop_blend` + `rapier_sprint_stop_blend` |
| Parada andando/correndo | `Nav_Stop` (+Stand+Walk/Run) | `cmbt_walk_stop_1h_f` + `rapier_pose_add` |
| Bloqueio parado | `Block_UpperBody` (1H_Melee) | `rswrd_run_block_ub_layer`; a espada e a machadinha do port usam `rswrd_block_idle` |
| Bloqueio andando | — | `rswrd_walk_block` (o mesmo da espada) |
| Queda | `Fall` (1H_Melee) | `unarmed_fall_flail_loop` + `rswrd_fall_flail_loop` |
| Pouso | `Land` (+Normal/Soft / +Hard) | `rapier_fall_softland_to_idle` / `rapier_fall_hardland_to_idle` |
| Atordoado | `r_Stun_Loop` (1H_Melee) | `cmbt_idle_r_stun_1h_loop` |

`rapier_pose_add` é uma pose aditiva (posição do braço com a rapieira) aplicada por cima dos clipes genéricos 1H
(idle, passo da esquiva, reações, paradas). Montada como camada comum, ela substitui a pose em vez de somar.

## Crítico, costas e regras próprias

- Crítico: `CritChance` 9% e `CritDamageMultiplier` 1,4 na arma (espada: 7% e 1,3). Todas as linhas de ataque têm
  `CanCrit`. `CritWindow` aparece em `Attack_Sprint_Land` (0,033–0,167); o efeito desse marcador não está nos dados lidos.
- Costas: nenhuma linha da rapieira liga `NoBackstab`. A regra geral de acerto pelas costas não está nas tabelas lidas.
  `Rapier_Fleche_Backstab` não é bônus pelas costas: marca os 5 s em que o próximo crítico ganha +15% (Parte de Trás).
  O perk de arma "Fleche Ladino" (+20% pelas costas com Fleche) é item, fora da árvore.
- Cabeça: `DmgCoefHead` existe nas linhas (1,2 no leve), sem regra de cabeça para corpo a corpo nos dados lidos.
- Afinco nos ataques básicos: `ActivateGrit` com condição `GRIT_On_Basic_Attacks` (definida fora dos actionlists lidos;
  provavelmente perk/gema).
- Todo uso de habilidade remove `PotionHaste` (poção de pressa) na entrada.
- Provocação: o pesado (`1H_Rapier_HeavyAttack*`) tem `IsTaunt` 0,5 s (só afeta IA).

## Fora de uso (legado nos arquivos)

- Fragmentos: `Attack_Heavy`, `Attack_Heavy_C2/C3`, `Attack_Start_Charge_C4..C6`, `Attack_Special`, `_Special_Charge`,
  `_Special_Charge_Exit`, `Attack_Special3` (sistema antigo com mana `SpecialAttack`), `Ability_Rapier_Aerial_Lunge`
  (usa clipe do cajado de vida), `Attack_Primary_Strike4..6` (só no escopo da arma).
- Linhas de dano: `1H_Rapier_SpecialAttack*`, `_Disarm`, `_Rupture`, `_Add_Bleed_Only`, `_Slash_Damage_Mod`,
  `_HeavyAttack` (sem caminho).
- Recarga `Ability_Rapier_Lunge` (16 s); condições `Lunge_Mod*` e `Evasion02_Mod*`; spells `Rapier_Evasion_Spell*` e
  `Rapier_Evasion_AllVision_Mod3` (`javelindata_spelltable_rapier`, aura antiga da Evasão).

## Sem fonte

- Distância real dos avanços (Fleche "10 m", Terminar, Evadir): depende do root motion dos clipes, não medido.
- Duração dos clipes (fim dos fragmentos sem `AnimEnd`): sai do `.caf` convertido.
- Semântica exata de `PowerLevel`/`HitStun` (pendente desde `plano-mecanicas.md`), de `CritWindow` e da regra de costas.
- Como "consumir" o sangramento conta o dano (restante dos ticks × 1,1 é a leitura mais provável do texto; o campo é só
  `ConsumeTargetStatusEffectMult`).
- `GRIT_On_Basic_Attacks` e `HasTauntGem`: condições globais não lidas.

## Proposta para o port

### Montagens (prefixo `AM_Rp_`)

Formato do `tools/sets/sword_shield.json`. Na rapieira as duas camadas do `.adb` usam o mesmo clipe, então cada seção
tem um clipe só (fora as poses aditivas). Clipes de bloqueio e de guardar são os mesmos da machadinha: dá para reusar
`AM_Hx_Block*` e `AM_Hx_Sheath`.

| Montagem | Seções (clipes) | Fragmentos (janelas) | Tags |
|---|---|---|---|
| `AM_Rp_LightCombo` | `rapier_prim_c1`, `rapier_prim_c2`, `rapier_prim_c3` | `Attack_Primary`, `Attack_Primary_Strike2`, `Attack_Primary_Strike3` | 1H_Melee,Rapier |
| `AM_Rp_HeavyCharge` (hold) | `rapier_hvy_c1_stab` (até 0,667 s) | `Attack_Start_Charge` | 1H_Melee,Rapier |
| `AM_Rp_HeavyCharged` | `rapier_hvy_c1_stab` a partir de 74% | `Attack_Heavy_Charged` | 1H_Melee,Rapier |
| `AM_Rp_SprintAttack` | `rapier_sprint_downstrike` | `Attack_Sprint_Land` (o Land começa em 40% do clipe: somar o deslocamento) | 1H_Melee,Rapier |
| `AM_Rp_Dodge_F/B/L/R` | `cmbt_1h_quickstep_f/b/l/r` (+ `rapier_pose_add` aditivo) | `Dodge_Roll` | +ForwardNav/BackNav/LeftNav/RightNav |
| `AM_Rp_Draw` | `rapier_idle_draw_v1` | `Unsheathe_UpperBody` | 1H_Melee,Rapier |
| `AM_Rp_Tondo` | `rapier_ability_wideslash` | `Ability_Rapier_Slash` | 1H_Melee,Rapier |
| `AM_Rp_Flurry` | `rapier_ability_flurry` | `Ability_Rapier_Flurry` | 1H_Melee,Rapier |
| `AM_Rp_Flourish` | `rapier_ability_flourish` | `Ability_Rapier_Flourish` | 1H_Melee,Rapier |
| `AM_Rp_Finish` | `rapier_ability_flourish_lunge` | `Ability_Rapier_Finish` | 1H_Melee,Rapier |
| `AM_Rp_Riposte` | `rapier_ability_riposte_start`, `rapier_ability_riposte_loop` (loop) | `Ability_Rapier_Riposte_Loop` | 1H_Melee,Rapier |
| `AM_Rp_RiposteOut` | `rapier_ability_riposte_end` | `Ability_Rapier_Riposte_Out` | 1H_Melee,Rapier |
| `AM_Rp_RiposteCancel` | `rapier_ability_riposte_cancel` | `Ability_Rapier_Riposte_Kickout` | 1H_Melee,Rapier |
| `AM_Rp_Fleche` | `rapier_ability_aeriallunge_start`, `_loop`, `_end` | `_Aerial_Lunge_Intro`, `_Loop`, `_Stop` | 1H_Melee,Rapier |
| `AM_Rp_Wake` | `rapier_ability_aeriallunge_stab` | `Ability_Rapier_Wake` | 1H_Melee,Rapier |
| `AM_Rp_Evade_F/B/L/R` | `rapier_ability_evade_f/b/l/r` | `Ability_Rapier_Evasion` | +ForwardNav/BackNav/LeftNav/RightNav |
| `AM_Rp_EvadeAttack` | `rapier_prim_c1` a partir de 20% | `Ability_Rapier_Evade_Attack` | 1H_Melee,Rapier |
| reuso | `AM_Hx_BlockStart/End/Idle/React/Break`, `AM_Hx_Sheath`, `AM_Hit_*`, `AM_StunLoop`, `AM_Death` | | |

Blend spaces e loops: `BS_Rp_Strafe` = `melee1h_cmbt_nav_strafe_blend` (+ `v2_rapier_cmbt_walk_layer_l/r` quando houver
camada aditiva); `BS_Rp_Sprint` = igual a `BS_Hatchet_Sprint`; `Rp_Idle` = `cmbt_idle_1h` (+ `rapier_pose_add`).
Arma: `"rapier": {"file": "objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf", "socket": "right_hand_attach"}`.

### Ações (`DT_RapierActions`, colunas atuais de `make_datatables.py`)

Stamina das tabelas: ataques e habilidades 0; esquiva `Dodge_Roll` 45. Formas em cm; "Blade" = varredura da lâmina
(base 0 → ponta 92,5 cm; o jogo usa cápsula raio 60 cm a 50 cm da base nos leves e 40 cm no pesado).

| Ação | Montagem | DamageID | Stamina | ComboNext | Forma | Recarga | Efeito |
|---|---|---|---:|---|---|---:|---|
| HeavyCharge | AM_Rp_HeavyCharge | — | 0 | Light1 (solto) / HeavyCharged (0,667 s) | — | 0 | |
| Light1 | AM_Rp_LightCombo_1 | 1H_Rapier_PrimaryAttack | 0 | Light2 (via carga C2) | Blade | 0 | |
| Light2 | AM_Rp_LightCombo_2 | 1H_Rapier_PrimaryTwoThirdsAttack ×2 | 0 | Light3 (via carga C3, 0,594 s) | Blade, 2 janelas | 0 | |
| Light3 | AM_Rp_LightCombo_3 | 1H_Rapier_PrimaryAttack_Ender | 0 | Light1 | Blade | 0 | |
| HeavyCharged | AM_Rp_HeavyCharged | 1H_Rapier_HeavyAttack_Charged | 0 | — | Blade | 0 | |
| SprintAttack | AM_Rp_SprintAttack | 1H_Rapier_SprintAttack | 0 | — | Blade | 0 | |
| Dodge_F/B/L/R | AM_Rp_Dodge_* | — | 45 | — | — | 0 | invulnerável 0–0,5 |
| Block* / Sheath | AM_Hx_* | — | (Block 10 como hoje) | — | — | 0 | |
| Draw | AM_Rp_Draw | — | 0 | — | — | 0 | |
| Tondo | AM_Rp_Tondo | 1H_Rapier_Slash | 0 | — | Box 535 × 200→450 (ou Front 535/60) | 6 | sangramento |
| Flurry | AM_Rp_Flurry | 1H_Rapier_Flurry_Strike_1..5 | 0 | — | Blade, 5 janelas | 16 | afinco |
| Flourish | AM_Rp_Flourish | 1H_Rapier_Flourish | 0 | Finish (input 0,1–0,65 buffer, ≥0,65) | Front 350/90 | 20 | empurrão 3 |
| Finish | AM_Rp_Finish | 1H_Rapier_Finish | 0 | — | Blade | 0 | consome sangramento ×1,1 |
| Riposte | AM_Rp_Riposte | — | 0 | RiposteOut (se atingido) / RiposteCancel (1 s) | — | 18 | sem dano 1 s |
| RiposteOut | AM_Rp_RiposteOut | 1H_Rapier_Riposte_Stun | 0 | — | Front 120/90 (no atacante) | 0 | TargetStun 2,0; invulnerável 0–0,567 |
| Fleche | AM_Rp_Fleche | 1H_Rapier_Aerial_Lunge | 0 | Wake (com passivas) | Around 110 acompanhando o corpo | 20 | atravessa |
| Wake | AM_Rp_Wake | 1H_Rapier_Wake | 0 | — | Blade | 0 | |
| Evade_F/B/L/R | AM_Rp_Evade_* | — | 0 | EvadeAttack | — | 12 | invulnerável 0–0,444 |
| EvadeAttack | AM_Rp_EvadeAttack | 1H_Rapier_PrimaryAttack | 0 | Light2 (via carga C2) | Blade | 0 | |

`ABILITY_IDS`: Tondo → `Ability_Rapier_Slash`, Flurry → `Ability_Rapier_Flurry`, Flourish → `Ability_Rapier_Flourish`,
Riposte → `Ability_Rapier_Riposte`, Fleche → `Ability_Rapier_Aerial_Lunge_Intro`, Evade_* → `Ability_Rapier_Evasion`.
`make_abilities.py`: `"rapier": {"name_key": "ui_rapier_mastery", "trees": ["ui_blood", "ui_evade"],
"hud_icon": "lyshineui/images/icons/items/drawing/1hrapier.dds"}`.

### Pipeline (scripts)

1. `build_set.py`/`adb_windows.py`: ler também `playermaleanims_1h_meleeattach.adb` (escopo da arma, fragmentos sem
   `AnimLayer`) e juntar as janelas. Sem isso os leves, o pesado e o sprint ficam sem `DamageWindow`/`WeaponTrail`.
2. `adb_windows.py`: somar o `ExitTime` depois de clipe vazio (correção de uma linha; muda 40 variantes da espada,
   na prática só `ApplyRotation_PostDodge` do rolamento rápido).
3. `window_overrides` por montagem com tags extras (`ForwardNav`/`BackNav`/`LeftNav`/`RightNav`) para esquiva e Evadir.
4. Início do clipe dentro da seção (`StartTime` do `Blend`): HeavyCharged em 74%, Light3 em 8%, EvadeAttack em 20%,
   SprintAttack Land em 40%.
5. Pose aditiva `rapier_pose_add` na montagem do Blender/Unreal (camada aditiva, não substituição).

### O que exige código novo no C++

Já existe em `NWCombatComponent` (2026-10-12): carga com `HeavyMinHold`/`HeavyFullCharge` e combo pelo `ComboNext`,
status com acúmulo e tick em fração do dano de arma (`ApplyStatus`, `StatusID`...), consumo por acúmulo
(`ConsumeStatusID`/`ConsumeDamage`), velocidade em si (`SelfMoveSpeed`), atordoar o alvo (`TargetStun`) e as formas
`Blade`/`Front`/`Around`. O que falta para ficar igual ao jogo:

1. **Carga da rapieira.** Não há pesado intermediário: na rapieira `HeavyMinHold` = `HeavyFullCharge` = 0,667 s
   (0,594 s na carga que vem do Light2) e não existe linha `Heavy`. Os dois números por arma (ou por passo do combo)
   precisam vir da tabela, não de propriedade fixa do componente.
2. **Várias janelas de dano por montagem com chave própria** (Light2 com 2 acertos, Rajada com 5): reiniciar a lista de
   alvos já atingidos a cada `DamageKey`.
3. **Formas novas:** caixa à frente presa na raiz (Tondo) e cápsula que acompanha o corpo durante o avanço (Fleche).
   `Front`/`Around` aproximam, mas Tondo perde a largura de 4,5 m no fim e Fleche precisa testar a cada quadro.
4. **Sangramento como no jogo:** hoje `ApplyStatus` renova a duração e soma acúmulo (igual). Faltam o teto de 18 s com
   extensões, o intervalo mínimo de 0,5 s entre aplicações e o consumo pelo dano restante (110% de
   acúmulos × 10% × dano de arma × ticks restantes), em vez de valor fixo por acúmulo.
5. **Continuação condicionada** (Brandir → Terminar; Fleche → Wake): input de ataque numa janela do fragmento anterior
   leva a outra montagem, sem recarga própria.
6. **Ripostar:** estado de 1 s sem dano, sem reação e imune a stun; ao ser atingido (servidor), contra-ataque com
   invulnerabilidade 0–0,567 e homing até o atacante; atordoamento de 2 s que **termina no primeiro dano** (o
   `ApplyStun` atual só termina pelo tempo); sem acerto, saída em 1 s.
7. **Evadir como habilidade:** passo curto direcional sem stamina, recarga 12 s, invulnerável 0–0,444, que cancela
   qualquer ação (inclusive habilidades) e libera o ataque rápido que entra no combo no Light2.
8. **Fleche:** avanço com root motion multiplicado (×1,3 / ×0,8), gravidade ×3, sem colisão com personagens, um acerto
   por alvo durante o movimento, frenagem forçada no fim.
9. **Afinco (grit) por janela:** Rajada inteira (0–1,367) e o começo dos ataques básicos não são interrompidos.
10. **Homing com aproximação** (`MoveToTarget`: até 1 m do alvo, no máximo 4,25 m nos leves).
11. Opcional (maestria): redução de recarga por acerto/crítico, buffs (Impulso, Sal na Ferida, Adagio), stamina por
    acerto, pressa acumulável.
