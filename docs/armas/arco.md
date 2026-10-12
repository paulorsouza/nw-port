# Arco (Bow): levantamento do jogo original

Levantado em 2026-10-12 a partir dos arquivos extraídos. Dados estruturados em `extract/research/bow.json`
(mesmas informações, com todas as janelas por fragmento).

Convenções:
- Tempos em segundos do fragmento do Mannequin (ProcLayer do `.adb`). "fim" = até o fim do fragmento.
- "linha N" = índice em `rows` do datasheet JSON (base 0).
- Caminhos de tabela relativos a `extract/data/sharedassets/springboardentitites/datatables/`.
- Actionlists (lógica CAGE) em `extract/serverpak/client/sharedassets/springboardentitites/playerdata/cage/`.

## Fontes usadas

| Dado | Arquivo |
|---|---|
| Item do arco | `javelindata_itemdefinitions_weapons` linha 111 (`BowT5`) |
| Flecha (munição) | `javelindata_itemdefinitions_ammo` (`ArrowT5`) |
| Dano | `javelindata_damagetable` linhas 172–193 |
| Stamina | `javelindata_staminacosts_player` linhas 27–31 e 63–67 |
| Recarga | `javelindata_cooldowns_player` linhas 30–35 |
| Feitiços (chuva, veneno, explosão) | `javelindata_spelltable_bow` linhas 0–9 |
| Habilidades e maestria | `weaponabilities/javelindata_ability_bow` (40 linhas) |
| Efeitos de status | `javelindata_statuseffects_bow` (20 linhas) |
| Perfil de mira/trava | `combatsettings/javelindata_combatprofiles` linha 2, `javelindata_combatsettings` linhas 6–7 |
| Animações e janelas | `extract/models/animations/mannequin/adb/player/player_bow.adb` + `player.adb` |
| Corda do arco | `extract/models/animations/mannequin/adb/playermaleanims_bowattach.adb` |
| Regras de estado | `playeractions_archery.actionlist`, `playeractions_ability_bow.actionlist`, `playeractions_nav.actionlist`, `playeractions_camera.actionlist`, `playeractions_dodge.actionlist`, `playerinput_combat.actionlist` |
| Teclas | `extract/config/libs/config/defaultprofile.xml` linhas 407–612 |
| Câmera | `extract/camera_gamecamera.xml` |
| Textos | `extract/loc/localization/pt-br/weaponabilities.loc.xml` |
| Projétil e AoE (via nwbt, 1 chamada) | `sharedassets/genericassets/rangedattackdatabase.radb`, `slices/items/weapons/basic_arrow*.dynamicslice`, `smoke_arrow*`, `spell_aoe_rainofarrowslarge`, `spell_aoe_explosionshot*`, `spell_bow_smoke*`, `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cdf` e `.chrparams` |

Os arquivos baixados pelo nwbt ficaram só no scratchpad da sessão. Os valores usados estão em `bow.json`.

## 1. Mecânica básica

### Item e munição

| Campo | Valor | Fonte |
|---|---|---|
| Tags do Mannequin | `Bow,LongBow` | BowT5 `MannequinTag` |
| Dano base | 84 (igual em T1–T5) | `BaseDamage` |
| Crítico | chance 0,08, multiplicador 1,3 | `CritChance`, `CritDamageMultiplier` |
| Stagger base | 38; dano de stamina no bloqueio 38 | `BaseStaggerDamage`, `BlockStaminaDamage` |
| Precisão base | 91 (T5) | `BaseAccuracy` |
| Escala | Destreza 0,0095, Força 0,004 | `ScalingDexterity`, `ScalingStrength` |
| Perfil de projétil | `Bow` | `RangedAttackProfile` |
| Junta de disparo | `arrowAttach_tip_jnt` | `FireJoint` |
| Munição | `Arrow`, malha `jav_wep_gear_arrowa.cgf` | `AmmoType`, `AmmoMesh` |
| Flecha ArrowT5 | `DamageModifier` 1,2, `StaggerDamageModifier` 1 | `javelindata_itemdefinitions_ammo` |

Munição: cada tiro consome 1 flecha do slot `arrow-ammo` (`UseAmmo`, `ConsumeAmmo`, `AmmoCountToConsume=1` no
`CAGE-RangedAttack`). Sem flecha entram `Bow_Empty_UpperBody`/`Bow_Empty_HipFire` (animação de aljava vazia). O
jogo tem o cvar `javelin.enable-ammoless-weapons`, que dispensa munição. No port: munição infinita; manter o
`DamageModifier` 1,2 no cálculo de dano se quisermos o número igual.

### Controles

| Tecla | Ação do jogo | Efeito |
|---|---|---|
| Esquerdo sem mira | `attack_shoot` (mouse1) | Tiro rápido sem mira (`Bow_Shoot_HipFire`) |
| Direito segurado | `aim_draw` (mouse2) → `aim_draw_active_virtual` | Mira (`Bow_ReadyWeapon` → `Bow_AimChargeShoot_SubState`). Há opção de alternar em vez de segurar |
| Esquerdo em mira, solto rápido | `aim_shoot_release` | Tiro leve mirado (`Bow_Shoot`) |
| Esquerdo em mira, segurado | `aim_shoot_hold` | Puxada (`Bow_Charge_UpperBody`) |
| Soltar o direito | `Aim_Exit_Bow` | Baixa o arco (`Bow_Lower_UpperBody`) |

Apertar o direito com a arma guardada saca e já entra em mira (`Trigger_From_Sheathed`).

### Fluxo de estados

1. **Sem mira, esquerdo**: `Bow_Shoot_HipFire`. Flecha sai em 0,4 s com `BowAttack_Hipshot` (60%). Segurar o
   botão repete: novo tiro aceito em buffer de 0,4 a 1,0 s e disparado em 1,0 s (ciclo de 1,0 s).
2. **Direito**: `Bow_ReadyWeapon_UpperBody` (0,433 s). Tag `Aim` em 0,07 s; câmera de mira e caminhada já no
   início (tag `AimDownSights`). Soltar o esquerdo entre 0,033 e 0,4 s guarda um tiro que sai em 0,4 s.
3. **Mira parada** (`Bow_Aim_UpperBody`, loop). Esquerdo:
   - soltar antes de 0,5 s → `Bow_Shoot_UpperBody`: flecha em 0,033 s, `BowAttack1` (100%), `ChargeValue` 0;
   - segurar → `Bow_Charge_UpperBody` (clipe a 2× de velocidade). Em **0,5 s** o marcador `SetChargedAim` liga a
     carga cheia. Soltar depois disso → `Bow_Shoot_Charged`: flecha em 0,033 s, `BowAttack2` (150%),
     `ChargeValue` 100. Em 0,667 s, se ainda segurando, vai para o loop de carga cheia (`Bow_Aim_UpperBody|Bow+ChargedAim`).
   - Carga cheia segurada por **3,0 s** (tag `ChargeTimer`, `DelayTimer=3.00`) → `Bow_Shake_UpperBody` (arco
     tremendo). Os dados não trazem penalidade de dano ou precisão para o tremor.
   - Não há dano intermediário: a carga é binária (BowAttack1 ou BowAttack2).
4. **Depois do tiro**, com o direito ainda segurado: em 0,467 s entra `Bow_Reload_UpperBody` (encaixa a próxima
   flecha). Novo tiro leve aceito em buffer de 0,133 a 0,667 s, sai em 0,667 s; nova puxada a partir de 0,733 s;
   sem input volta ao loop de mira em 1,167 s. Ciclo mínimo de tiro leve mirado: 0,467 + 0,667 ≈ 1,13 s.
   Sem o direito: `Bow_Lower_FromShoot_UpperBody` (ou `_Charged_`) em 0,467–0,5 s.
5. **Sem stamina** (≤ 0): variantes `Bow_ReadyWeapon_Exhausted_UpperBody` (clipe a 0,5×) e
   `Bow_Reload_Exhausted_UpperBody` (0,55×), com regeneração de stamina desligada.

### Movimento e câmera

| Item | Valor | Fonte |
|---|---|---|
| Velocidade em mira | 1,65 m/s (`Stand_Walk`) | `playeractions_nav.actionlist`: Aim, ChargedAim, HeavyAim, SmokeAim, Shoot, ForceWalk, AimDownSightsAbility |
| Velocidade em combate sem mira | 3,25 m/s (`Combat_Nav_Stand`) | idem |
| Giro do corpo em mira | TurnRate 0,35 (`Aiming_Or_Shooting`); 0,40 no ReadyWeapon; 0,50 no `AlignToCamera` do HipFire | idem, `playeractions_archery.actionlist` |
| Bônus de precisão parado/mirando | 12,25 padrão; 25 mirando de pé | `SetWeaponAccuracyBonus` em `playeractions_nav.actionlist` |
| Câmera de combate | `MidCamera` (l. 1505): FOV 50, Offset (0,5; 0; 1,7), Distance 2,0 | `camera_gamecamera.xml` |
| Câmera de mira | `AimCamera` (l. 211): herda MidCamera, **Distance 1,0**, MinZoom/MaxZoom 0, `IsADS`, DistanceDampening 0,05; transição de entrada 0,09 s; volta para MidCamera em 0,09 s (para DynamicCamera seria 0,5 s) | idem |
| FOV em mira | sem mudança: FOV 50 herdado com `UseUserDefinedFov=true`. O "zoom" é só a câmera chegar mais perto (2,0 → 1,0 m) | idem |
| Escolha da câmera | `Aim+Stand` (tag `AimDownSights`) → AimCamera | `playeractions_camera.actionlist` |

Trava de alvo: perfil `BowGeneric`/`BowCameraLock`; `BreakLockOnADS` verdadeiro (mirar solta a trava).

### Esquiva, sacar e guardar

| Ação | Fragmento | Stamina | Janelas |
|---|---|---|---|
| Esquiva (carga leve) | `Dodge_Roll_Fast` com `Bow` / `Bow+BackNav` / `+LeftNav` / `+RightNav` | 45 (`Dodge_Roll_Fast`, linha 66) | invulnerável 0–0,5; paga stamina 0,1; `CanAttackCancel` 0,267; `CanTransition` 0,5; encadeia outra esquiva 0,267–0,5 |
| Esquiva (carga normal) | `Dodge_Roll` (quickstep) | 45 (linha 65) | mesmas janelas |
| Esquiva (carga pesada) | `Dodge_Roll_Slow` | 50 (linha 67) | mesmas janelas |
| Sacar | `Unsheathe_UpperBody|Bow` (`bow_idle_draw_ub` a 1,2×) | 0 | troca de socket em 0,2 (`unsheathe_weapon`); `CanTransition_UpperBody` 0,433 |
| Guardar | `Sheathe_UpperBody|From_Bow` (`bow_idle_sheath_ub`) em `player.adb` | 0 | troca de socket em 0,533 (`sheathe_weapon`); `CanTransition_UpperBody` 1,0 |

O fragmento da esquiva depende da carga de equipamento (`ActionConditionEquipLoadCategory` em
`playeractions_dodge.actionlist`). A espada do port usa `Dodge_Roll_Fast`; para o arco vale o mesmo.

Não existe sprint attack próprio do arco (sem `Attack_Sprint` em `player_bow.adb`).

Golpe com o arco (`Bow_GunButt|Bow+LongBow`, dano `Bow_Gunbutt` 0,5 Strike, cápsula raio 0,3 m na mão esquerda,
`CAGE-Damage` 0,567–0,667): a ação `Bow_Gunbutt` tem `ActivationCondition Not True` e só entra por transição de
`Attack_Start_Charge|Bow`, que o arco atual não usa. Tratar como não usado.

### Projétil

Perfil `Bow` em `rangedattackdatabase.radb` + slice `basic_arrow.dynamicslice`:

| Campo | Valor |
|---|---|
| Velocidade | 100 m/s (`m_basespeed`; também `ProjectileSpeed=100` em cada `CAGE-RangedAttack`) |
| Velocidade extra | 25 m/s, `m_timeformaxspeed` 1 s (interpretação provável: extra pela carga; não confirmado) |
| Gravidade | −9,8 m/s² (`m_accel`) |
| Reta antes da gravidade | 5 m (`Basic`/`Basic_Heavy`); 10 m com Alcance de Flechas (`Basic_Plus*`) |
| Vida | 10 s voando, 8 s cravada |
| Alcance máximo | 100 m |
| Raio da flecha | 0,15 m (leve), 0,275 m (carregada e habilidades) |
| Penetração | 0,2 m |
| Queda de dano | início 30 m, máximo 80 m, `m_projectiledamagemaxfalloff` 1 (sem redução na prática) |
| Origem/mira | junta `aim_direction` (`m_aimjoint`, `AimJoint`) |
| Som de passagem | `Arrow_WhizzBy` a 15 m; `Play_InFlight_ArrowBys` |

Ataques do perfil `Bow` (radb):

| Nome | Usado por | Alcance | Raio | Reta antes da gravidade | Observação |
|---|---|---|---|---|---|
| Basic | HipFire, AimShot | 100 | 0,15 | 5 | |
| Basic_Plus | idem com Alcance de Flechas | 100 | 0,15 | 10 | |
| Basic_Heavy | ChargedShot, EvadeShot | 100 | 0,275 | 5 | |
| Basic_Plus_Heavy | idem com Alcance de Flechas | 100 | 0,275 | 10 | |
| TripleShot | RapidShot | 100 | 0,275 | 20 | |
| PenetratingShot (A1, A2) | Tiro Penetrante | 50 | 0,275 | 30 (60, 100) | `m_hitrate` 1000; descrição diz 100 m |
| SmokeShot | Disparo Venenoso | 100 | 0,275 | 20 | penetração 0 |
| ExplosionShot | Flecha Explosiva | 100 | 0,275 | 10 | penetração 0 |
| SpreadShot | (não usado) | 10 | 0,275 | 20 | |

## 2. Ações básicas

Camadas: a primeira lista é o corpo inteiro (`unarmed_*`), a segunda a camada da arma. Fragmentos `_UpperBody`
têm uma camada só e tocam por cima da locomoção.

| Ação | Fragmento (tags) | Clipes | Disparo / janela principal | DamageID (linha) | Stamina (CostID) | Cancelar |
|---|---|---|---|---|---|---|
| HipFire | `Bow_Shoot_HipFire` (Bow) | `bow_quickfire_ub` @1,25 | `CAGE-RangedAttack` Basic em 0,4; flecha na mão 0,067–0,133; `AlignToCamera` 0–0,4 | `BowAttack_Hipshot` (178): 0,6 Thrust | 0 (`Attack_2H_Bow_Primary`) | esquiva 0–0,267 e 1,0+; `CanCancel_UpperBody` 1,0 |
| AimStart | `Bow_ReadyWeapon_UpperBody` (Bow) | `bow_idle_to_aim_halfdrw_ub` | flecha na mão 0,1–0,333; tiro guardado 0,033–0,4; vai para mira em 0,433 | — | 0 | esquiva 0,267+ |
| AimIdle | `Bow_Aim_UpperBody` (Bow) | `bow_aim_halfdrw_idle_ub` (loop) | — | — | — | `CanCancel_UpperBody` sempre |
| AimShot | `Bow_Shoot_UpperBody` (Bow) | `bow_aim_halfdrw_release_ub` | `CAGE-RangedAttack` Basic em 0,033 | `BowAttack1` (172): 1,0 Thrust | 0 (`Attack_2H_Bow_Aim`) | esquiva 0,433+; recarga/baixar 0,467 |
| Charge | `Bow_Charge_UpperBody` (Bow) | `bow_aim_halfdrw_to_fulldrw_walk_ub` @2 | carga cheia em 0,5 (`SetChargedAim`) | — | 0 (`Attack_2H_Bow_Aim_Charge`) | soltar antes de 0,5 = AimShot |
| ChargedIdle | `Bow_Aim_UpperBody` (Bow+ChargedAim) | `bow_aim_fulldrw_idle_ub` (loop) | tremor após 3,0 s | — | — | |
| Shake | `Bow_Shake_UpperBody` / `Bow_Shake_End_UpperBody` (Bow+ChargedAim) | `bow_aim_fulldrw_walk_shake_ub`, `..._shake_end_ub` | só visual | — | — | |
| ChargedShot | `Bow_Shoot_Charged` (Bow) | `bow_aim_fulldrw_release_ub` | `CAGE-RangedAttack` Basic_Heavy em 0,033, `ChargeValue` 100 | `BowAttack2` (173): 1,5 Thrust | 0 (`Attack_2H_Bow_Primary_Charge`) | esquiva 0,433+; recarga 0,467; baixar 0,5 |
| Reload | `Bow_Reload_UpperBody` (Bow) | `bow_aim_halfdrw_reload_ub` | flecha na mão 0,2–0,633; tiro 0,667 (buffer 0,133); carga 0,733; volta à mira 1,167 | — | — | esquiva sempre |
| ReloadCharged | `Bow_Reload_Charged_UpperBody` (Bow) | `bow_aim_fulldrw_reload_ub` | tiro 0,933 | — | — | |
| AimEnd | `Bow_Lower_UpperBody` (Bow / Bow+ChargedAim) | `bow_aim_halfdrw_to_idle_ub` / `bow_aim_fulldrw_to_idle_ub` | `CanTransition_UpperBody` 0,067; sai em 0,367 | — | — | |
| AimEndFromShot | `Bow_Lower_FromShoot_UpperBody` / `_Charged_` (Bow) | `bow_aim_halfdrw_release_to_idle_ub` / `bow_aim_fulldrw_release_to_idle_ub` | sai em 0,367 | — | — | |
| Dodge_F/B/L/R | `Dodge_Roll_Fast` (Bow / +BackNav / +LeftNav / +RightNav) | `unarmed_dodgeroll_{f,b,l,r}_light_to_crch` + `bow_dodgeroll_{f,b,l,r}_light_to_crch` | invulnerável 0–0,5 | — | 45 (`Dodge_Roll_Fast`) | `CanTransition` 0,5 |
| Draw | `Unsheathe_UpperBody` (Bow) | `bow_idle_draw_ub` @1,2 | troca de socket 0,2 | — | — | 0,433 |
| Sheath | `Sheathe_UpperBody` (From_Bow, player.adb) | `bow_idle_sheath_ub` | troca de socket 0,533 | — | — | 1,0 |
| Hit_F/B/L/R | `r_R2_{F,B,L,R}` (Bow) | `unarmed_r2_*` + `bow_r2_*` | — | — | — | |
| Death | `r_Death` (sem tag, player.adb) | `unarmed_death_droptokneesfaceplant` | — | — | — | |
| StunLoop | `r_Stun_Loop` (Bow) | `unarmed_r_stun_loop` | — | — | — | |

Linhas de dano (todas `WeaponCategory` Bow, `IsRanged`, `DmgCoefHead` 1,3, `HitStun` 15, `BlockHitStun` 12):

| DamageID | Linha | DmgCoef | Tipo | Stagger / bloqueio | Outros |
|---|---|---|---|---|---|
| BowAttack1 | 172 | 1,0 | Thrust, Light | 0,35 / 0,35 | |
| BowAttack2 | 173 | 1,5 | Thrust, Heavy | 1,25 / 1,25 | |
| BowAttack_Hipshot | 178 | 0,6 | Thrust, Light | 0,3 / 0,3 | |

Animações da corda (esqueleto do arco), de `playermaleanims_bowattach.adb`: ReadyWeapon `bow_idle_to_aim_halfdrw_prop`,
Aim `bow_aim_halfdrw_idle_prop`, Charge `bow_aim_halfdrw_to_fulldrw_prop`, ChargedIdle `bow_aim_fulldrw_idle_prop`,
Shoot `bow_aim_halfdrw_release_prop`, ChargedShot `bow_aim_fulldrw_release_prop`, Reload `bow_aim_halfdrw_reload_prop`,
HipFire `bow_quickfire_prop`, Shake `bow_aim_fulldrw_shake_prop`, habilidades `bow_spc_*_prop`. Arquivos em
`animations/gameplay/prop/weapons/bow/base_bow/`.

## 3. Habilidades

Árvores: 0 = **Combatente** (`ui_bow_tree1`), 1 = **Caçador** (`ui_bow_tree2`). Maestria: "Maestria de Arco".
Todas as habilidades custam 0 de stamina. A recarga começa depois do marcador `Ability_Startup`
(comentário "Activate CoolDown after startup" no actionlist).

### Ativas

| AbilityID | Nome pt-BR | Recarga | DamageID (linha) | Forma do acerto | Fragmento(s) e tempos |
|---|---|---|---|---|---|
| Ability_Bow_RapidShot | Tiro Rápido | 14 s | `Bow_RapidAttack_Standard` (176) 1,0 ×2; `Bow_RapidAttack_KnockBack` (177) 1,25, HitStun 30 | 3 flechas `TripleShot` em 0,333 / 0,567 / 0,8; `AccuracyBonus` 10 | `Ability_Bow_RapidShot` (Bow+LongBow), `bow_spc_attack_1` corpo inteiro; esquiva 0,567–0,867, 1,1–1,733, 2,333+; `CanTransition` 2,6 |
| Ability_Bow_EvadeShot | Tiro Evasivo | 15 s | `BowAttackJumpBack` (174) 0,9 | 1 flecha `Basic_Heavy` em 0,733; salto 5 m para trás | `Ability_Bow_JumpBackShot` (Bow+LongBow), `bow_spc_5_rapidfire_jump`; root motion sem gravidade até `Enable_Gravity` 0,867; esquiva 1,3; `CanTransition` 1,433; câmera `AimCameraJumpBack` |
| Ability_Bow_PenetratingShot | Tiro Penetrante | 18 s | `Bow_Penetrating_Attack` (187) 1,5, HitStun 25, stagger 1,5 | flecha `PenetratingShot` que atravessa alvos; alcance 50 (radb) ou 100 m (descrição) | tecla entra em mira própria (tag `PenetratingAim`): `Bow_ReadyWeapon_UpperBody` (0,433) → `Bow_Aim_UpperBody` → disparo com esquerdo ou a tecla de novo: `Bow_Shoot_UpperBody|Bow+PenetratingAim`, `bow_spc_3_aim_fulldrw_release_ub`, flecha em 0,033; `CanCancel_UpperBody` 0,7 |
| Ability_Bow_PoisonShot | Disparo Venenoso | 20 s | `Bow_PoisonShot` (180) 0,5 NoHeadshot; nuvem `Bow_PoisonShotCloud` (182) | flecha `SmokeShot`; ao bater ou após 2 s cria nuvem esfera raio 3 m, altura 5 m, 6 s | mira própria (tag `SmokeAim`): ReadyWeapon → Aim → `Bow_Shoot_UpperBody|Bow+SmokeAim`; marcador `ShootPoisonShot` 0,033–0,1 lança `Spell_Ability_Bow_PoisonShot_Projectile` |
| Ability_Bow_RainofArrows | Chuva de Flechas | 20 s | `Bow_RainOfArrows_Attack` (186) 1,5, NoHeadshot, NoBackstab | área no chão raio 4 m, altura 5 m; alvo até 50 m; um acerto entre 0,6 e 0,8 s depois do impacto | mira alta (tag `HeavyAim`): `Bow_ReadyWeapon_UpperBody|Bow+HeavyAim` (`bow_idle_to_spc_4_aim_idle` @1,5, 0,4 s) → `Bow_Aim_UpperBody|Bow+HeavyAim` com indicador no chão → `Bow_Shoot_RainOfArrows` (intro+outro @1,5), feitiço no marcador `Bow_Spell_RainOfArrows` 0,267–0,567; `CanTransition` 0,567; cancelar com direito ou bloqueio |
| Ability_Bow_SplinterShot | Flecha Explosiva | 22 s | `Bow_ExplosionShot_Projectile` (190) 0,5; explosão `Bow_ExplosionShot_Explosion` (191) 1,35 Fire, NoHeadshot | flecha `ExplosionShot`; explosão esfera raio 2,5 m ao colidir | `Ability_Bow_ExplosiveArrow` (Bow+LongBow), `bow_spc_attack_2_ub` (corpo superior); `CAGE-CastSpell` `Spell_Ability_Bow_Explosive_Base` em 0,467; `CanCancel_UpperBody` 1,167 |

Descrições pt-BR (números já calculados):

- **Tiro Rápido**: Dispare 3 flechas consecutivas. Primeira Flecha: 100% do dano de arma. Segunda Flecha: 100% do dano de arma. Terceira Flecha: 125% do dano de arma.
- **Tiro Evasivo**: Salta 5 m para trás e dispara uma flecha, causando 90% do dano de arma ao seu alvo.
- **Tiro Penetrante**: Dispara uma flecha que causa 150% do dano de arma e atravessa os alvos. Esse ataque tem um alcance de 100 m.
- **Disparo Venenoso**: Dispara uma flecha que causa 50% do dano de arma. Ao acertar ou aterrissar, essa flecha cria uma nuvem de veneno com raio de 3 m e duração de 6 s. Os inimigos que entram na nuvem são envenenados, recebendo 20% do dano de arma por segundo durante 7 s. (Essa habilidade não causa acertos na cabeça.)
- **Chuva de Flechas**: Atira uma barragem de flechas que cobre uma área de 4 m de raio e causa 150% do dano de arma. Essa habilidade não causa acertos na cabeça.
- **Flecha Explosiva**: Dispara uma flecha que causa 50% do dano de arma ao acertar, depois explode ao atingir o alvo, causando 135% de dano adicional a todos os alvos dentro de um raio de 2,5 m.

Detalhes dos feitiços (`javelindata_spelltable_bow`):

| SpellID | Linha | Tipo | Valores |
|---|---|---|---|
| Spell_Bow_Spell_RainOfArrows | 0 | AoE | Duration 5000 ms, Height 5, Radius 4, CastDistance 50, CastHeight 60, alvo Enemy, CheckObstructions; prefab `spell_aoe_rainofarrowslarge` (gatilho do volume em 0,6 s, desliga em 0,8 s) |
| Spell_Ability_Bow_PoisonShot_Projectile | 1 | Projectile | Duration 2000 ms, secundário ao colidir **e** ao expirar → `Spell_Ability_Bow_PoisonShot_Aoe`; `FireJointName` aim_direction |
| Spell_Ability_Bow_PoisonShot_Aoe | 3 | AoE esfera | Duration 6000, Radius 3, Height 5, status `Status_Bow_Poison` |
| Spell_Ability_Bow_Explosive_Base | 6 | Projectile | Duration 500 ms, secundário só ao colidir → `Spell_Ability_Bow_ExplosiveShot_Explosion` |
| Spell_Ability_Bow_ExplosiveShot_Explosion | 8 | AoE esfera | Radius 2,5, Duration 1000; slice usa `Bow_ExplosionShot_Explosion` |
| Spell_Ability_Bow_ExplosiveShot_Explosion_SizeChange | 9 | AoE esfera | Radius 1,25; slice usa `Bow_ExplosionShot_Explosion_Dmg` (1,7) |

### Melhorias das ativas

| AbilityID | Nome | Efeito (dados) |
|---|---|---|
| Upgrade_Bow_RapidAccuracy_CDROnLast | Precisão Rápida | 3 acertos seguidos no mesmo alvo: recarga −50% |
| Upgrade_Bow_RapidAccuracy_DmgOnLast | Último Golpe | 3ª flecha (`Bow_RapidAttack_KnockBack`) +25% dano base |
| Upgrade_Bow_EvasiveShot_Knockback | Empurrão Evasivo | troca a linha para `BowAttackKnockBack` (175): 0,9, PowerLevel 3, HitStun 30, empurrão 2 m |
| Upgrade_Bow_EvasiveShot_Haste | Longa Distância | acerto dá `Status_Upgrade_Bow_EvasiveShot_Haste`: +12% velocidade por 5 s |
| Upgrade_Bow_PenetratingShot_DmgPerHit | Flecha Coberta de Sangue | troca para `Bow_Penetrating_AttackA1` (188): +10% por acerto, máx. +50% |
| Upgrade_Bow_PenetratingShot_DmgPerDist | Golpe Profundo | +15% dano base a ≥ 20 m |
| Upgrade_Bow_PoisonShot_PoisonDmg | Flechas Infectadas | nuvem aplica `Status_Bow_Poison_Upgrade`: 24%/s por 7 s |
| Upgrade_Bow_PoisonShot_DirectDmg | Acerto Direto | acerto direto usa `Bow_PoisonShot_Upgrade` (181): 1,0 |
| Upgrade_Bow_RainofArrows_Bleed | Flechas Farpadas | `Status_Passive_RainofArrows_Bleed`: 9%/s por 12 s, tick 1 s |
| Upgrade_Bow_RainofArrows_Slow | Flechas de Gancho | `Status_Bow_RainOfArrows_Slow`: −15% velocidade por 4 s |
| Upgrade_Bow_SplinterShot_CDRPerHIt | Flecha em Chamas | acerto direto aplica `Status_ExplosiveShot_Burn`: 8%/s Fire por 10 s |
| Upgrade_Bow_SplinterShot_FiveA | Detonação Contida | explosão 170% (`Bow_ExplosionShot_Explosion_Dmg`, 192), raio 1,25 m |

### Efeitos de status das ativas

| StatusID | Linha | Duração | Tick | Efeito |
|---|---|---|---|---|
| Status_Bow_Poison | 13 | 7 s (máx. 15) | 1 s | −20% do dano de arma por tick, Nature |
| Status_Bow_Poison_Upgrade | 16 | 7 s (máx. 20) | 1 s | −24% por tick, Nature |
| Status_Passive_RainofArrows_Bleed | 9 | 12 s (máx. 24) | 1 s | −9% por tick, Standard |
| Status_Bow_RainOfArrows_Slow | 8 | 4 s (máx. 8) | — | velocidade −15% |
| Status_ExplosiveShot_Burn | 18 | 10 s (máx. 20) | 1 s | −8% por tick, Fire |
| Status_Upgrade_Bow_EvasiveShot_Haste | 0 | 5 s | — | velocidade +12% |

`HealthModifierDamageBased` é fração do dano de arma por tick.

### Passivas

| AbilityID | Árvore | Nome | Efeito |
|---|---|---|---|
| Passive_Bow_Skirmish_HasteWhenFoesNear | Combatente | Alcance-me se For Capaz | +20% velocidade com 3+ inimigos perto (feitiço raio 4 m; texto diz 3 m) |
| Passive_Bow_Skirmish_RendHead | Combatente | Tiro na Cabeça Vulnerabilizante | tiro na cabeça: armadura do alvo −10% por 5 s |
| Passive_Bow_Skirmish_SlowFull | Combatente | Empalar | acerto em alvo com 100% de vida: −10% velocidade por 2 s |
| Passive_Bow_Skirmish_CDRVsLow | Combatente | Aproximando-se | acerto em alvo < 50%: recargas −5% |
| Passive_Bow_Skirmish_DodgeHaste | Combatente | Esquivar e Escapar | esquiva com stamina cheia: +7% velocidade por 2 s |
| Passive_Bow_Skirmish_StaminaVsDebuff | Combatente | Percepção do Caçador | acerto em alvo com debuff: +5 stamina |
| Passive_Bow_Skirmish_HasteOnSwap | Combatente | Velocidade do Arqueiro | trocar para o arco: +7% velocidade por 5 s (recarga 10 s) |
| Passive_Bow_Skirmish_DmgVsDebuff | Combatente | Marca | +10% dano base em alvo com debuff |
| Passive_Bow_Skirmish_DebuffDur | Combatente | Precisão de Batalha | debuffs +20% de duração |
| Ultimate_Bow_Skirmish | Combatente | Táticas Evasivas | após esquiva: +15% dano por 5 s |
| Passive_Bow_Hunter_DmgVsDist | Caçador | Alcance Extenso | +10% dano base a ≥ 10 m |
| Passive_Bow_Hunter_HeavyDmg | Caçador | Mira Verdadeira | ataque pesado +10% dano base e flecha mais rápida |
| Passive_Bow_Hunter_ArmorAimHeavy | Caçador | Foco Inquebrável | +10% armadura enquanto mira |
| Passive_Bow_Hunter_DmgVsLow | Caçador | Disparo Final | +15% dano base em alvo < 50% |
| Passive_Bow_Hunter_HeavyDmgVsFull | Caçador | Golpe de Abertura | pesado +10% dano base em alvo com 100% |
| Passive_Bow_Hunter_DmgOnFirst | Caçador | Ataque Surpresa | texto: +15% se não causou dano em 10 s; tabela: `BaseDamage` 0,2 |
| Passive_Bow_Hunter_Accuracy | Caçador | Alcance de Flechas | reta antes da gravidade ×2 (5 → 10 m: `Basic_Plus`) |
| Passive_Bow_Hunter_HealOnFive | Caçador | Olho de Gavião | tiro na cabeça cura 10% do dano |
| Passive_Bow_Hunter_Crit | Caçador | Na Mosca | +10% chance de crítico |
| Ultimate_Bow_Hunter | Caçador | Concussão | tiro na cabeça +20% dano |

## 4. Locomoção, loops e mira

| Nome proposto | Clipes (camadas) | Fragmento de origem |
|---|---|---|
| BS_Bw_Strafe | `unarmed_cmbtstrafe_blend` + `bow_ststrafe_blend` | Strafe (Bow) |
| BS_Bw_Sprint | `unarmed_sprint_veerslope_v2_blend` + `bow_sprint_veerslope_blend` | Sprint (Bow) |
| BS_Bw_AimStrafe | `bow_aim_halfdrw_strafe_blend` | Strafe (Bow+Aim); também PenetratingAim, SmokeAim, HipAim |
| BS_Bw_ChargedStrafe | `bow_aim_fulldrw_strafe_blend` | Strafe (Bow+ChargedAim) |
| BS_Bw_RainStrafe | `bow_spc_4_strafe_blend` | Strafe (Bow+HeavyAim) |
| BS_Bw_Turn / AimTurn / ChargedTurn | `bow_actn_idle_turn_blend` / `bow_aim_halfdrw_idle_turn_blend` / `bow_aim_fulldrw_idle_turn_blend` | Turn_InPlace |
| BS_Bw_RunStop / SprintStop | `bow_run_stop_blend` / `bow_sprint_stop_blend` (+ unarmed) | Nav_Stop |
| Bw_Idle | `unarmed_idle` + `bow_idle` | Idle (Bow) |
| Bw_AimIdle | `bow_aim_halfdrw_idle` (corpo inteiro) | Idle (Bow+Aim) |
| Bw_ChargedIdle | `bow_aim_fulldrw_idle` | Idle (Bow+ChargedAim) |
| Bw_RainAimIdle | `bow_spc_4_aim_idle` | Idle (Bow+HeavyAim) |
| Bw_ExhaustIdle | `unarmed_exhaust_idle` + `bow_exhaust_idle` | Idle (Bow+Exhausted) |
| Bw_FallLoop | `unarmed_fall_flail_loop` + `bow_fall_flail_loop` | Fall (Bow) |

Poses de mira 15× (aim offset de pitch/yaw, fragmento `Aim`, ação `AimPose` em `playeractions_aim.actionlist`):
`bow_halfdrw_aim_poses_15x_ub` (Aim), `bow_fullfdrw_aim_poses_15x_ub` (ChargedAim), `bow_spc_4_aim_poses_15x_ub`
(HeavyAim), `bow_quickfire_aim_poses_15x_ub` (HipAim). Versões sem `_ub` para parado (`+Idle`).

Pulo e aterrissagem seguem o padrão da espada com `v2_bow_jump_walk_*`, `v2_bow_jump_walk_f_to_land_walk_*`,
`v2_bow_jump_inplace`, `bow_fall_land_to_idle`, `bow_fall_hardland_to_idle` (lista em `bow.json` → `montages_extra`).

Agachado (`Crouch`) tem variantes de quase tudo; o port não tem agachar e ignora.

## 5. Malhas e sockets

| Item | Caminho |
|---|---|
| Arco T5 (personagem com esqueleto) | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cdf` → `wep_ran2h_bowa.chr` + `wep_ran2h_bowa_t5.skin` |
| Corda | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t1_string.skin` (`SkinOverride2` do item, igual em todos os tiers) |
| Arco T5 estático | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cgf` (sem corda animada) |
| Material | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_mat.mtl` |
| Ossos do arco | `bow_root_jnt`, `bow_localRoot_jnt`, `bowStringPull_jnt`, `upperBow_1/2_jnt`, `upperBowString(_end)_jnt`, `lowerBow_1/2_jnt`, `lowerBowString(_end)_jnt`, `arrowAttach_tip_jnt` |
| Flecha | `objects/weapons/ranged/projectiles/arrowa/jav_wep_gear_arrowa.cgf` (mesma no projétil e na mão) |
| Aljava (opcional) | `objects/props/weapons/quiver/jav_quiver.cgf` |

Sockets (attachments do `player_male.cdf`):

| Uso | Attachment | Osso | Fonte |
|---|---|---|---|
| Arco na mão | `lefthandattach` | `left_hand_attach` | inferido (nenhuma tabela liga EquipType Bow a um socket; a flecha vai para a mão direita) |
| Flecha na mão | `righthandattach` | `right_hand_attach` | `CAGE-Attachment` `AttachmentSource=ActiveWeapon` nos fragmentos de puxar/recarregar |
| Arco guardado | `bow` | `Spine1`, RelPosition (−0,10805; 0,073166; −0,169168), RelRotation (0,384488; 0,599706; −0,374635; 0,593439) | `player_male.cdf` |
| Aljava | `quiver` | `Spine2` | idem |

## 6. Regras de dano

- Dano esperado: `84 × DmgCoef × 1,2 (flecha) × 1,3 se cabeça`. O 1,2 da flecha é provável pelo nome do campo;
  não há fórmula explícita nos dados.
- Cabeça: `DmgCoefHead` 1,3 em todas as linhas do arco. `NoHeadshot` em `Bow_PoisonShot`, `Bow_PoisonShot_Upgrade`,
  `Bow_PoisonShotCloud_Vortex`, `Bow_RainOfArrows_Attack`, `Bow_ExplosionShot_Explosion`, `_Explosion_Dmg`,
  `Bow_Spread_Shot`.
- Costas: `NoBackstab` em `Bow_RainOfArrows_Attack` e `Bow_ExplosionShot_*`.
- Crítico: vem da arma (0,08 de chance, ×1,3). Nas linhas `DmgCoefCrit` = 0 e `CanCrit` = falso.
- Reação: `PowerLevel` 0 em todas as linhas comuns (não interrompe); só `BowAttackKnockBack`, `Bow_Gunbutt*` têm 3.
- Bloqueio: flecha bloqueada é destruída (`m_destroyprojectileonblock`). `BlockStaminaDmgMod` = stagger da linha.

## Sem fonte ou incerto

- Duração dos clipes `.caf` (os `.caf` não estão extraídos; as janelas com "fim" dependem dela).
- Efeito real de `m_extraspeed` 25 e de `ChargeValue` 100 na velocidade da flecha.
- Socket do arco na mão (inferido como mão esquerda).
- Penalidade do tremor (`Shake`): nenhuma nos dados.
- Alcance do Tiro Penetrante: 50 m no radb, 100 m no texto.
- `Spell_Ability_Bow_Explosive_Base` com Duration 500 ms: não está claro se limita o alcance da flecha.
- Tempo de segurar para `aim_shoot_hold` virar carga: o perfil de input não define atraso; tratar como imediato.

## Proposta para o port

Prefixo de montagem `AM_Bw_`. Tags de janela `Bow,LongBow`; adbs `player/player_bow.adb` + `player/player.adb`
(entrada em `window_overrides` como a da machadinha). Shapes: `("Projectile", 10000, 0)` = 100 m/s em cm/s,
como a machadinha usa 2500.

| Ação | Montagem | Fragmentos (tags) | DamageID | Stamina | Recarga | Forma |
|---|---|---|---|---|---|---|
| Bw_Draw | AM_Bw_Draw | Unsheathe_UpperBody (Bow) | — | — | — | — |
| Bw_Sheath | AM_Bw_Sheath | Sheathe_UpperBody (From_Bow) | — | — | — | — |
| Bw_HipFire | AM_Bw_HipFire | Bow_Shoot_HipFire (Bow) | BowAttack_Hipshot | Attack_2H_Bow_Primary (0) | — | Projétil 100 m/s, disparo 0,4 |
| Bw_AimStart | AM_Bw_AimStart | Bow_ReadyWeapon_UpperBody (Bow) | — | — | — | — |
| Bw_AimIdle | AM_Bw_AimIdle (loop) | Bow_Aim_UpperBody (Bow) | — | — | — | — |
| Bw_AimShot | AM_Bw_AimShot | Bow_Shoot_UpperBody (Bow) | BowAttack1 | Attack_2H_Bow_Aim (0) | — | Projétil, disparo 0,033 |
| Bw_Charge | AM_Bw_Charge (hold) | Bow_Charge_UpperBody (Bow) | — | Attack_2H_Bow_Aim_Charge (0) | — | carga cheia em 0,5 |
| Bw_ChargedIdle | AM_Bw_ChargedIdle (loop) | Bow_Aim_UpperBody (Bow+ChargedAim) | — | — | — | — |
| Bw_Shake | AM_Bw_Shake (loop) | Bow_Shake_UpperBody (Bow+ChargedAim) | — | — | — | após 3 s |
| Bw_ChargedShot | AM_Bw_ChargedShot | Bow_Shoot_Charged (Bow) | BowAttack2 | Attack_2H_Bow_Primary_Charge (0) | — | Projétil raio 0,275 |
| Bw_Reload | AM_Bw_Reload | Bow_Reload_UpperBody (Bow) | — | — | — | — |
| Bw_AimEnd / Bw_AimEndCharged | AM_Bw_AimEnd / AM_Bw_AimEndCharged | Bow_Lower_UpperBody (Bow / Bow+ChargedAim) | — | — | — | — |
| Bw_AimEndFromShot / ...Charged | AM_Bw_AimEndFromShot / AM_Bw_AimEndFromShotCharged | Bow_Lower_FromShoot(_Charged)_UpperBody (Bow) | — | — | — | — |
| Dodge_F/B/L/R | AM_Bw_Dodge_F/B/L/R | Dodge_Roll_Fast (Bow, +BackNav, +LeftNav, +RightNav) | — | Dodge_Roll_Fast (45) | — | invulnerável 0–0,5 |
| Hit_F/B/L/R | AM_Bw_Hit_F/B/L/R | r_R2_* (Bow) | — | — | — | — |
| Death | AM_Bw_Death | r_Death (player.adb) | — | — | — | — |
| RapidShot | AM_Bw_RapidShot | Ability_Bow_RapidShot (Bow+LongBow) | Bow_RapidAttack_Standard ×2, Bow_RapidAttack_KnockBack | — | 14 | 3 projéteis em 0,333/0,567/0,8 |
| EvadeShot | AM_Bw_EvadeShot | Ability_Bow_JumpBackShot (Bow+LongBow) | BowAttackJumpBack | — | 15 | salto 5 m + projétil em 0,733 |
| PenetratingShot | AM_Bw_AimStart + AM_Bw_AimIdle + AM_Bw_PenetratingShot | Bow_Shoot_UpperBody (Bow+PenetratingAim) | Bow_Penetrating_Attack | — | 18 | projétil que atravessa, 50 m |
| PoisonShot | AM_Bw_AimStart + AM_Bw_AimIdle + AM_Bw_AimShot | Bow_Shoot_UpperBody (Bow+SmokeAim) | Bow_PoisonShot + nuvem | — | 20 | projétil → AoE 3 m, 6 s, veneno 20%/s 7 s |
| RainOfArrows | AM_Bw_RainReady + AM_Bw_RainIdle (loop) + AM_Bw_RainShot + AM_Bw_RainEnd | Bow_*\|Bow+HeavyAim, Bow_Shoot_RainOfArrows, Bow_Lower_FromRain_UpperBody | Bow_RainOfArrows_Attack | — | 20 | AoE no chão raio 4 m, até 50 m, acerto 0,6–0,8 s |
| ExplosiveArrow | AM_Bw_ExplosiveArrow | Ability_Bow_ExplosiveArrow (Bow+LongBow) | Bow_ExplosionShot_Projectile + _Explosion | — | 22 | projétil → explosão 2,5 m ao colidir |

Ordem sugerida: HipFire, mira, AimShot, carga e ChargedShot, Reload, esquiva; depois RapidShot, EvadeShot e
PenetratingShot (só projétil); por último PoisonShot, RainOfArrows e ExplosiveArrow (precisam de AoE e status).
Proposta de habilidades padrão: RapidShot, EvadeShot, PenetratingShot.

### O que exige código novo no C++

1. **Modo mira**: estado segurando o direito; troca de câmera (AimCamera: distância 1,0, transição 0,09 s nos dois sentidos
   com a MidCamera); velocidade 1,65 m/s; personagem vira com a câmera (TurnRate 0,35); retícula.
2. **Montagens de corpo superior**: os fragmentos `_UpperBody` tocam num slot de corpo superior por cima da
   locomoção (BS_Bw_AimStrafe / ChargedStrafe / RainStrafe). O AnimBP precisa de blend por osso a partir da coluna.
3. **Aim offset**: poses `*_aim_poses_15x*` para apontar pitch/yaw.
4. **Máquina de estados do arco**: ReadyWeapon → mira → carga (0,5 s) → tiro leve ou carregado → recarga
   automática (tiro em 0,667 s) → volta à mira; tremor após 3 s; HipFire com repetição a cada 1,0 s; buffers
   das janelas `CAGE-TransitionBuffered`.
5. **Projétil com gravidade**: ator replicado criado no servidor; 100 m/s, reta de 5 m e depois −9,8 m/s²,
   raio 0,15/0,275 m, vida 10 s, alcance 100 m; mira pelo raio da câmera; flecha crava (8 s). Detectar cabeça
   para `DmgCoefHead`. Penetração para o Tiro Penetrante (vários alvos, um acerto por alvo).
6. **Flecha na mão**: mostrar `jav_wep_gear_arrowa` no `right_hand_attach` nas janelas `CAGE-Attachment`.
7. **Disparo múltiplo**: Tiro Rápido com 3 eventos de disparo e linhas de dano diferentes.
8. **Root motion sem gravidade**: Tiro Evasivo até `Enable_Gravity` (0,867 s).
9. **Área no chão com mira**: Chuva de Flechas com indicador por raycast (até 50 m), volume raio 4 m × 5 m,
   um acerto entre 0,6 e 0,8 s, sem cabeça e sem costas.
10. **Área persistente e explosão**: nuvem de veneno (3 m, 6 s, aplica status a quem entra) e explosão ao
    colidir (2,5 m).
11. **Efeitos de status com tick**: veneno, sangramento, queimadura (fração do dano de arma por segundo), lentidão
    e pressa (multiplicador de velocidade). Hoje o port só tem buffs fixos (Berserk, Último Recurso).
12. **Malha do arco**: começar com `wep_ran2h_bowa_t5.cgf` estático no `left_hand_attach`; corda animada
    (`.cdf` + animações `_prop`) fica para depois. Arco guardado no attachment `bow` (Spine1).
13. **Munição**: infinita; sem `Bow_Empty`.
