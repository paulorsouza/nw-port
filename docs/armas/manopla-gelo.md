# Manopla de gelo (IceGauntlet): levantamento do jogo original

Sub-esteira 1a (`docs/esteiras.md`). Levantado em 2026-10-15 a partir dos arquivos extraídos. Relatório automático
completo (todas as janelas por fragmento, textos das passivas, feitiços): `extract/research/icegauntlet_report.md`
e `.json`, gerado com

```bash
python3 tools/weapon_report.py icegauntlet --tag Ice --adb player/player_magicgauntlet_ice.adb \
    --base MagicGauntlet --damage IceMagic_ --cost IceMagic --ability icemagic
```

Convenções: tempos em segundos do fragmento (ProcLayer do `.adb`), "fim" = até o fim do fragmento. Tabelas
relativas a `extract/data/sharedassets/springboardentitites/datatables/` (DT). Actionlists em
`extract/serverpak/client/sharedassets/springboardentitites/playerdata/cage/`.

## Fontes usadas

| Dado | Arquivo |
|---|---|
| Item | `javelindata_itemdefinitions_weapons` (`1hElementalGauntlet_IceT2..T5`) |
| Dano | `javelindata_damagetable` (15 linhas `IceMagic_*`) |
| Mana (custos) | `javelindata_manacosts_player` (`IceMagic_*`, `Zero_Mana`) |
| Mana (recurso) | `slicejson/slices/player.dynamicslice.json` (`ManaComponent`), `vitalstables/javelindata_basevitals_player` (`Player`) |
| Recargas | `javelindata_cooldowns_player` (`IceMagic_*`) |
| Feitiços | `javelindata_spelltable_icemagic` (91 linhas) |
| Status | `javelindata_statuseffects_icemagic` (42 linhas) |
| Habilidades e maestria | `weaponabilities/javelindata_ability_icemagic` (40 linhas) |
| Animações e janelas | `adb/player/player_magicgauntlet_ice.adb`, `player_magicgauntlet.adb`, `player.adb` |
| Lógica das ações | `playeractions_ability_icemagic.actionlist` (19 ações), `playeractions_attack.actionlist` (entrada do básico), `player.grid` (L2178–2199) |
| Textos | `extract/loc/localization/pt-br/weaponabilities.loc.xml` |

## 1. Mecânica básica

### Item

`1hElementalGauntlet_IceT5`: BaseDamage 67, CritChance 0,09, CritDamageMultiplier 1,3, BaseStaggerDamage 43,
BlockStaminaDamage 43, BlockStability 30, `CanBlockRanged` (escala 0,5/0,5), escala INT 0,0095 / FOR 0,004,
`MannequinTag=MagicGauntlet,Ice`, `RangedAttackProfile=IceMagic`, `FireJoint=aim_direction`, mastery
`IceMagicAbilityTable`. Malha: `.skin` (`objects/weapons/magic/gauntletcore/1h/icegauntlett5/wep_mag_gauntcore_1h_icegauntlett5.skin`),
malha com pele presa ao esqueleto do jogador, não arma estática no socket.

Escala do port (espada 64 = 100): BaseDamage 67 → **104,7**; StaggerDamage e BlockStaminaDamage 43.

Malha (aparência `Elemental_Gauntlet_T5_Ice`, `javelindata_itemappearancedefinitions`): a luva
`objects/weapons/magic/gauntlets/1h/icegauntlett5/wep_mag_gaunt_1h_icegauntlett5.cdf` tem pele no braço direito
(antebraço, mão e dedos); o cristal da palma (`gauntletcore/.../wep_mag_gauntcore_1h_icegauntlett5.cdf`) é rígido no
`handattach`, como a rapieira. A aparência troca o antebraço direito do corpo (`RightHandOnlySkin`
`male_naked_right_forearm.skin`).

### Grid (player.grid)

- `UpperBodyAttack`: Attack_Primary, Attack_Heavy (tocam no corpo superior, andando).
- `Attack`: Spikes, Entombed, IcePylon.
- `Aim`: IceStorm (+ AimSubState, Casting, Cast), WindChill (+ AimSubState, Outro), IceShower.
- `IsIgnoredByGrid`: Entombed_DodgeExit, IcePylon_Dodge, IcePylon_Cooldown(2), IcePylon_Destroy, Quick_Frost.
- `Attack → Aim` liberado com `IfMannequinTag Ice` (habilidade de mira interrompe as outras).

### Ataque básico: leve e pesado

Entrada: `Attack_Start_Charge_Upperbody` (playeractions_attack) ativa com `UseUpperBodyAttacks` (arma da mão principal
com tag Ice/Fire/Heal/VoidGauntlet). Fragmento `Attack_Start_Charge_UpperBody [MagicGauntlet+Ice]`
(`ice_gnt_windchill_charge_ub`), `CAGE-TransitionInstant` com `TransitionKey=Attack_Branch`:

| t | Condição | Ação |
|---|---|---|
| 0,033 | `Attack_Heavy_NotHeld_Magic` (soltou, ou mana < custo do pesado) | `IceMagic_Attack_Primary` |
| 0,500 | `CanAttack_Heavy_Magic_GreyWizardHatArtifact` | `IceMagic_Attack_Heavy` (artefato, fora do escopo) |
| 0,667 | `CanAttack_Heavy_Magic` (tag Ice e `IfManaCost ≥ IceMagic_Heavy`) | `IceMagic_Attack_Heavy` |
| 0,700 | sempre | `IceMagic_Attack_Primary` |

Mesma regra de carga das armas que já existem: soltar antes de 0,667 = leve, segurar até 0,667 = pesado (no port,
`HeavyMinHold` 0, `HeavyFullCharge` 0,667). Custo de mana dos dois = 0.

| Ação | Fragmento / clipe | Projétil | DamageID | Combo |
|---|---|---|---|---|
| Primary | `Ability_IceMagic_Primary` / `ice_gnt_lt_atk_up` | `CAGE-CastSpell IceMagic_Light_Projectile` em 0,167 | `IceMagic_Primary` (1,0) | `CanAttack_Light_Magic` buffered 0,267–0,933, instant 0,933 → nova carga |
| Heavy | `Ability_IceMagic_Heavy` / `ice_gnt_lt_atk_up` | `IceMagic_Heavy_Projectile` em 0,167 | `IceMagic_Heavy` (1,4) + `IceMagic_HeavyAttackSlow` (−10%, 3 s, recarga 6 s) | buffered 0,1–0,833, instant 0,833 |

Atividades das duas (`playeractions_ability_icemagic` L3, L227): `CameraRelativeFacing` Default 0,
`AlignToCamera` 0,40 exponencial, `AlignToCameraFast` 0,90; `ResetGrit`; `SetHaltMotionOnCollision 170`;
`Sprint_Delay_Timer`. Projéteis (`rangedattackdatabase.radb`, perfil `IceMagic`, e slices
`items/weapons/icemagic_primary_proj` / `_heavy_proj`, extraídos em `extract/weapons_icegauntlet/`): 100 m/s
(`m_basespeed`; `m_extraspeed` 25 sem uso conhecido), vida 0,5 s (`m_flyinglifetime`) = alcance 50 m (`m_maxrange`),
raio 0,15 m (Primary) / 0,33 m (Heavy), reta por 100 m (`m_projectileacceldelaydist`, sem queda no alcance), sai de
(0,3; 0,3; 1,2) m do personagem (`m_characteroffset`), some ao bater ou ser bloqueado. Janelas comuns:
`ForceWalk` (anda ao atacar), `BlockDisabled` 0,1–0,9, `CanDodgeCancel` 0–0,1 e 0,667–fim, `CanBlockCancel`
0,9–fim. Da janela 0–0,1 do leve/pesado sai qualquer habilidade (`CanAttack_<habilidade>` buffered e depois
instant).

### Bloqueio, esquiva, sacar

- Bloqueio: `Block_UpperBody_Raise` / `Block_UpperBody` (`ice_gnt_shield_idle_ub`), `BlockActive` o fragmento todo.
  Usa só stamina (`playeractions_block`); `IceMagic_Block` (10 de mana) existe na tabela mas nenhuma actionlist ou
  `.adb` usa.
- Esquiva: `Dodge_Roll` / `Dodge_Roll_Slow` com `cmbt_1h_quickstep_f(_hvy)` + camada `ice_gnt_pose_add`;
  invulnerável 0–0,5; `CanDodgeCancel` 0,267–0,5 buffered.
- Sacar: `Unsheathe_UpperBody` (`ice_gnt_idle_draw_ub`), arma aparece em 0,2 (`unsheathe_weapon`).
- Locomoção: `Strafe` = `melee1h_cmbt_nav_strafe_blend` + camadas `v2_ice_gnt_cmbt_walk_layer_l/r`; `Idle` =
  `cmbt_idle_1h` + `ice_gnt_pose_add` + `ice_gnt_idle_rhand`; parada `ice_gnt_sprint_stop_blend.bspace`.

## 2. Mana

| Peça | Valor | Fonte |
|---|---|---|
| Recurso | mín 0, máx 100, inicial 100 | `ManaComponent` (`m_min`, `m_initmax`); `basevitals_player` (`ManaBaseMax`, `ManaInitial`) |
| Regeneração | **conflito**: `m_regenrate` 1 (slice) × `ManaBaseRegenRate` 2 (tabela); unidade não dita | aguardando manopla-gelo-001 |
| Atraso | `ManaRegenDelay` 0, `ManaRegenDelayOnEmpty` 2 | `basevitals_player`; semântica aguardando manopla-gelo-001 |
| Fora de combate | `Status_OutOfCombat_PassiveRegen` ManaRate 1, 180 s | `statuseffects_common` |
| Foco 100 | `Foc_Bonus_100_1` ManaRate 0,2, MaxMana 0,1 | `ability_attributethreshold` |

Custos (`javelindata_manacosts_player`) e onde cada um é cobrado:

| CostID | Mana | Cobrança | Onde |
|---|---:|---|---|
| IceMagic_Primary / Heavy | 0 | — | — |
| IceMagic_Spikes | 15 | procclip `CAGE-PayManaCost` | `Ability_IceMagic_Spikes` 0,2 |
| IceMagic_WindChill | 20 | procclip | `WindChill_Aim(_Long)` 0,067 |
| IceMagic_Entombed_Break | 10 | procclip | `Entombed_Break` 0,033 |
| IceMagic_IcePylon | 15 | atividade `PayManaCost` | ação `IceMagic_IcePylon_Cooldown`, depois de `Pylon_Spawn` |
| IceMagic_IceStorm | 25 (5 com Invocador da Tempestade e mana cheia) | coluna `ManaCost` do feitiço | `IceMagic_Spell_IceStorm[_Mana]` (aguardando manopla-gelo-002) |
| IceMagic_IceShower | 25 | coluna `ManaCost` do `Shower1` | idem |
| IceMagic_Entombed | 0 | coluna `ManaCost` do `Spell_Entomb` | — |

Condições: `IfManaCost CostID ComparisonType` (habilidade só entra com mana ≥ custo; 96 usos) e `IfManaValue`
(percentual: `Equal 1` = mana cheia). Não há canalização paga por tempo na manopla de gelo (só no cajado de fogo);
o Vento Arrepiante paga uma vez. Não há atividade que desligue a regeneração de mana.

Ganhos de mana: `IceMagic_Entombed_ManaRegen` (ManaRate 6, 10 s), `IceMagic_EmpoweredFrost` (+6 por ataque em área
congelada, passiva Geada Energizada), `IceMagic_UnbrokenWindsPerk` (+10, perk global), `IceTempestPass2` (+10% no
crítico), `_Helper` (+5% no pesado, recarga 0,5 s), `IceTempestPass4` (+15% a cada 3 leves seguidos),
`IceMagic_BlockingStamina` (−5 mana, +10 stamina ao bloquear com mana > 3%).

## 3. Habilidades ativas

| Habilidade (tecla) | Ação / fragmentos | Mana | Recarga | DamageID | Mecânica |
|---|---|---:|---:|---|---|
| Estacas de Gelo (`IceMagic_Spikes`) | `Spikes` → `Spikes_Out1..5` | 15 | 10 | `IceMagic_Spikes` 0,61; última `IceMagic_Spikes_End` 1,57 + empurrão 2 m | `ChannelSpell IceMagic_Spell_Spikes`: 7 estacas a cada 65 ms em linha (8 m); ataque básico entre 0,267 e 0,633 (`Spikes_EarlyTrigger`) solta a Estaca Poderosa antes (`Spawn_End1..5` a 2,2/3,3/4,4/5,5/6,6 m). Parado (`PreventNav` 0–1,167) |
| Tempestade de Gelo (`IceMagic_IceStorm`) | `IceStorm_Intro` → `_Aim` (mira) → `_Cast` / `_Outro` | 25 | 20 | `IceMagic_IceStorm` 0,17 a cada 0,33 s | mira de área até 20 m (`CastSpellTargeting`), disparo no marcador `SpellCastIceStorm` (0,2); esfera raio 5, altura 5, 5 s; status `IceMagic_IceStorm` (−25% velocidade) e `_Damage`. `ForceWalk` na mira |
| Vento Arrepiante (`IceMagic_WindChill`) | `WindChill_Intro` → `_Aim` (2 s) → `_Outro` | 20 | 15 | `IceMagic_WindChill` 0,24 + empurrão 15 (Y) e `_End` 0,3 (últimos 2 m), a cada 0,2 s | canalização de 2 s (3 s com Frio Duradouro), coluna de 7 m; anda em strafe (`ForceStrafeNav`, `PreventSprint`); sai cedo com `IceMagic_Heavy_Exit` |
| Torre de Gelo (`IceMagic_IcePylon`) | `Pylon` | 15 (ao nascer) | 10 (conta só depois que a torre morre) | `IceMagic_Pylon` 0,75 por projétil | torre com 20% da vida máxima do jogador, atira em inimigos a 20 m, área congelada 1 m por 15 s; segurar a tecla 30 ticks destrói; recarga infinita até `Pylon_Death` ou 45 s (`PylonCooldownSafety`) |
| Chuva de Gelo (`IceMagic_IceShower`) | `Shower` | 25 | 30 | — | 5 caixas 0,5×0,5×3 m em 0,4/0,467/0,533/0,6/0,667 a 2,8/2,25/2/2,25/2,8 m; 4 s; `IceMagic_Frostbite` |
| Sepultado (`IceMagic_Entombed`) | `Entombed_Intro` → `_Loop` (10 s) → `_Outro` ou `_Break` | 0 (quebrar: 10) | 30 (começa ao sair) | `IceMagic_Entombed_Break` 0,7, esfera 3 m, empurrão 2 m | invulnerável, sem andar, ManaRate 6; tumba com 50% da vida máxima; sai com a tecla, bloqueio ou esquiva; ataque básico quebra (dano em área) |

Texto completo e números de cada habilidade: relatório §4.

## 4. Status de frio

| StatusID | Efeito | Duração |
|---|---|---|
| `IceMagic_IceStorm` | lentidão −25% (andar e correr) | enquanto na área (ClearOnExit) |
| `IceMagic_IceStorm_Damage` | dano 0,17 × dano de arma por tique de 0,33 s | enquanto na área |
| `IceMagic_Frostbite` (Congelamento) | lentidão −50%; tique 10 → `IceMagic_IceShower_Freeze` | enquanto na chuva, depois `_Dur3` 3 s |
| `IceMagic_IceShower_Freeze` | enraizado (`NoNav/NoRun/NoSprint`) | 1 s, depois imune 6 s (`_Freeze_Cooldown`) |
| `IceMagic_Freeze` | enraizado (pesado em alvo com Frost, passiva Congelamento Pesado) | 1 s, depois imune 4 s |
| `IceMagic_HeavyAttackSlow` | lentidão −10% | 3 s, reaplicação a cada 6 s |
| `IceMagic_Entombed` | invulnerável, sem andar, sem regeneração de vida | 10 s |
| `IceMagic_Frost` | marcador "na área congelada" (passivas) | enquanto na área |

Categorias que o port precisa entender: `Slow`, `Root`, `Frost`, `FrostBuff`, `Haste`, `ArmorRend`,
`ArmorFortify`, `Weaken`. Variantes por passiva (`_Rend`, `_DeadlyFrost`, `_UltimateChill`, `_Long`, `_Quick`):
`javelindata_statuseffects_icemagic`.

## 5. Passivas e árvores

Duas árvores (`TreeId` 0 Tempestade, 1 Conjurador), 6 ativas e 34 passivas com posição (`TreeRowPosition`,
`TreeColumnPosition`). Lista com texto: relatório §4. As que mexem em mana, frio ou recarga dependem do sistema de
gatilhos de passiva (`OnHit`, `OnCrit`, `OnBlockedHitTaken`, `NumConsecutiveHits`, `TargetStatusEffectCategory`), que o
port ainda não tem de forma geral.

## 6. Sons e efeitos

Sons nos procclips `Audio` (`play_gauntlet_ice_*`), lidos pelo `make_sound_map.py`. Prefabs de efeito e de feitiço
(`items/weapons/icemagic_*_proj`, `spell_aoe_blizzard`, `spell_aoe_shower_*`, `Ice_Entomb`, `Ice_Pylon*`,
`Ice_Spike_*`) e materiais `materials/vfx/weapon/icegauntlet_*` estão no pak, não extraídos.

## Sem fonte ou incerto

- Regeneração de mana: taxa (1 ou 2), unidade, atraso depois de gastar e com mana zerada. **Aguardando manopla-gelo-001.**
- Cobrança pela coluna `ManaCost` do feitiço (`CastSpell`, `CastSpellRaycast`) e fórmula do `PayManaCost`
  (modificadores como na stamina?). **Aguardando manopla-gelo-002.**
- Torre, tumba e estacas: vida, alvo da torre, cadência, quem aplica `IceMagic_Entombed_ManaRegen` e `IceMagic_Frost`:
  prefabs `Ice_Pylon*`, `Ice_Entomb`, `Ice_Spike_*` (extrair; se a lógica não estiver no prefab, perguntar).
- `ChannelSpell` / `CastingTypes=IceSpikes|PathOfDestiny` (espaçamento e tempo das estacas): `SpawnRate 65` e
  `NumToSpawn 7` lidos; o resto aguardando manopla-gelo-003.
- Duração dos `.caf` (janelas "fim").
- `IceMagic_Windchill_Shatter` é sempre falsa no actionlist (`And(X, Not X)`): no jogo a variante Shatter nunca sai.
  Seguir os dados.
- `IceMagic_WindChill_SW` citada na habilidade e ausente da damagetable.

## Proposta para o port

Prefixo de montagem `AM_Ig_`; parte do conjunto em `tools/sets/player/armas_manopla-gelo.json` (entra em `parts`);
`window_overrides` com tags `MagicGauntlet,Ice` e adbs `player/player_magicgauntlet_ice.adb`,
`player/player_magicgauntlet.adb`, `player/player.adb`; `attach_adbs` = `PlayerMaleAnims_1H_RangedAttach.adb`.
Montagens de corpo superior (slot `UpperBody`, já existe pelo arco) para leve, pesado, carga, mira e casts `_ub`.

| Ação | Montagem | Fragmento | DamageID | Mana | Recarga | Forma |
|---|---|---|---|---:|---:|---|
| Ig_Draw / Ig_Sheath | AM_Ig_Draw / _Sheath | Unsheathe/Sheathe_UpperBody | — | — | — | — |
| HeavyCharge | AM_Ig_Charge (hold, ub) | Attack_Start_Charge_UpperBody | — | — | — | carga 0,667 |
| Light1 | AM_Ig_Primary (ub) | Ability_IceMagic_Primary | IceMagic_Primary | 0 | — | projétil em 0,167 |
| Heavy | AM_Ig_Heavy (ub) | Ability_IceMagic_Heavy | IceMagic_Heavy | 0 | — | projétil em 0,167 + lentidão |
| BlockStart/BlockIdle | AM_Ig_Block* (ub) | Block_UpperBody(_Raise) | — | — | — | — |
| Dodge_F/B/L/R | AM_Ig_Dodge_* | Dodge_Roll | — | — | — | invulnerável 0–0,5 |
| Spikes | AM_Ig_Spikes + AM_Ig_SpikesOut | Ability_IceMagic_Spikes(_Out1..5) | IceMagic_Spikes / _End | 15 | 10 | linha de 7 áreas + estaca final |
| IceStorm | AM_Ig_StormIntro/Aim(loop)/Cast/Outro (ub) | IceStorm_* | IceMagic_IceStorm | 25 | 20 | área mirada 5 m, 5 s, tique 0,33 |
| WindChill | AM_Ig_WindIntro/Aim/Outro (ub) | WindChill_* | IceMagic_WindChill / _End | 20 | 15 | cone/coluna 7 m, tique 0,2, 2 s |
| IceShower | AM_Ig_Shower (ub) | Ability_IceMagic_Shower | — | 25 | 30 | 5 caixas, Frostbite |
| IcePylon | AM_Ig_Pylon | Ability_IceMagic_Pylon | IceMagic_Pylon | 15 | 10 após morrer | ator torre |
| Entombed | AM_Ig_TombIntro/Loop/Outro/Break | Entombed_* | IceMagic_Entombed_Break | 0 / 10 | 30 ao sair | ator tumba |
| Hit_*, Death | as do conjunto | r_R2_*, r_Death | — | — | — | — |

Ordem: (1) mana + leve/pesado/bloqueio/esquiva com projéteis; (2) Vento Arrepiante e Estacas (sem ator novo);
(3) Tempestade e Chuva (área persistente com status de frio); (4) Torre e Sepultado (atores com vida);
(5) passivas, sons, efeitos.

### Código novo (arquivos da 1a)

1. `UNWManaComponent` (`NWManaComponent.h/.cpp`, novo): Mana/MaxMana replicados, regeneração e atraso
   (manopla-gelo-001), `CanPay(CostID)`, `Pay(CostID)`, ganho por status (`ManaRate`, `Mana`). Custos lidos de
   `javelindata_manacosts_player` (tabela gerada por `make_datatables.py`).
2. `NWArmaManopla.cpp/.h` (novo): registro da arma, ações de mira de área (Tempestade), canalização (Vento),
   estacas em linha, chuva, torre (`ANWIcePylon`) e tumba (`ANWIceTomb`), botões F10 pelo `NW_REGISTER_DEV`.
3. Status de frio: categorias `Slow`/`Root`/`Frost` e imunidade pós-enraizar (`*_Freeze_Cooldown`).

### Mudança em arquivo comum (mínima, avisar a esteira 1 / dono)

| Arquivo | Dono | Mudança |
|---|---|---|
| `NWCharacter.h/.cpp` | compartilhado | criar o `UNWManaComponent`; `FNWWeaponSet` aceitar malha com pele (`USkeletalMesh`, seguindo a pose do corpo) além da estática |
| `NWCombatTypes.h` | compartilhado | coluna `ManaCost` (CostID) em `FNWActionRow`; categoria do status (`Root`) |
| `NWCombatComponent.cpp` | esteira 1 | `CheckAction` recusa se a mana não cobre (`IfManaCost`); cobrar no procclip; enraizado bloqueia andar |
| `NWCharacterArmas.cpp` | esteira 1 | uma linha chamando o registro da manopla em `AddPlayerWeaponSets` |
| `make_datatables.py`, `make_abilities.py` | esteira 1 | `ICEGAUNTLET_*` e `WEAPONS["IceGauntlet"]` (blocos novos, sem mexer nas outras armas) |
| `tools/sets/sword_shield.json` | compartilhado | `parts` += `player/armas_manopla-gelo.json` |
| `NWHUD.cpp` | esteira 4 | a barra `ManaMeter` já existe no layout; ligar ao `UNWManaComponent` (pedir à esteira 4 ou mudança de 1 linha) |

`ABP_NW`: nada novo (slot `UpperBody` já existe).

## Estado

Parte 1 (2026-10-15): arma registrada (`NWArmaManopla.cpp`, `Id` `IceGauntlet`), luva com pele (`SK_icegauntlet`, segue a
pose do corpo) e cristal na palma (`SM_icegauntlet_core`), `UNWManaComponent` (barra na HUD, F10 > Encher mana / Mana
infinita), coluna `ManaCost` nas ações (`IfManaCost`), leve e pesado com projétil do perfil `IceMagic`, bloqueio,
esquiva e sacar/guardar com as animações da manopla (`AM_Ig_*`), árvore de maestria nos dados (`abilities.*.json`,
habilidades ainda sem ação). Par de teste: F10 > Manopla de gelo + espada. Testes: `docs/testes/manopla-gelo.md` §Parte 1.

Habilidades (2026-10-15): as seis ativas com animação do jogo (`AM_Ig_*`) e mecânica no `UNWIceGauntletComponent`
(`NWArmaManopla.cpp`, números com fonte em `namespace NWIce`): Estacas (7 estacas a cada 65 ms, Estaca Poderosa no fim
ou antes com ataque básico), Vento Arrepiante (11 tiques em 2 s, cápsulas do `.adb`), Tempestade (área de 5 m por 5 s,
tique 0,33 s, −25%), Chuva (5 caixas por 4 s, −50%, enraizar 1 s e imune 6 s, −50% por 3 s ao sair), Torre (15 s, tiro
a cada 1 s em 20 m, recarga ao sumir), Sepultado (10 s, invulnerável, +6 de mana/s, quebrar com ataque básico; recarga
ao sair). Padrão em Q/R/F: Tempestade, Vento, Estacas. Passivas: ainda não.

Correções do teste de 2026-10-15: clique correndo pedia `SprintAttack`, que a manopla não tem, e o leve não saía (agora
o golpe de corrida só entra se a tabela da arma tiver a linha, `NWCharacterArmas.cpp`); ações que encadeiam nelas
mesmas (`WindChillAim`, `EntombedLoop`) tocam a montagem em loop (`MulticastPlayLoop`) em vez de recomeçar a cada
volta, porque recomeçar a mesma montagem interrompia a anterior e encerrava a tumba logo depois da entrada.

Efeitos do jogo (2026-10-15): 28 efeitos das bibliotecas `wfx_icegaunt_base`, `wfxo_icegaunt_tempest` e
`wfxo_iceguant_builder` convertidos para Niagara (`/Game/NW/FX/NS_<Biblioteca>_<Efeito>`, receita em
`docs/particulas-estudo.md` §Pipeline). Projéteis com rastro (`ProjectileFx`), mão e corpo pelas montagens
(`tools/ue/fx_notifies.py`), áreas/estacas/torre/tumba pelo `UNWIceGauntletComponent`.

**Primeira versão fechada em 2026-10-15** com os bugs abaixo conhecidos (decisão do dono: seguir assim).

## Pendências

- **Bugs conhecidos no fechamento da v1 (teste do dono, 2026-10-15):** ataque básico ainda com muitos defeitos (leve
  às vezes não sai, cliques seguidos cortam a animação, às vezes não dá dano); ataque correndo funciona só em parte;
  algumas habilidades ainda falham; efeitos de algumas habilidades ainda longe do original. Pedir prints por
  habilidade antes de mexer.
- **Duração mínima da animação (relato do dono, 2026-10-15):** leve e pesado às vezes não dão dano; o dono suspeita
  que as armas mágicas têm uma animação mínima que o cálculo do port não respeita. Investigar depois: tempo do
  disparo (0,167 s) contra a troca de ação, `Ability_Startup`, `g_attackSlowTime`/`g_attackRecoveryTime` (armas-001).

## Diferenças conhecidas

- Tempestade sem a fase de mira: a área cai no ponto da câmera ao apertar (o jogo mostra o alvo e espera o clique).
- Chuva: as 5 caixas aparecem juntas em 0,4 s (no jogo, uma a cada ~0,067 s); posição = distância e giro de cada
  `Shower1..5` a partir do personagem (`CharRelPosOffsetY` 3 não usado, H).
- Estacas: sem o clipe de laço entre a intro e a saída; espaçamento 1,1 m = `CastDistance` (H); a Estaca Poderosa
  antecipada vai para a próxima posição `Spawn_End1..5` depois da última estaca.
- Torre e tumba não recebem dano (no jogo: 20% e 50% da vida máxima do jogador); a torre atira a cada 1 s (H,
  `m_cooldown` 1 do slice); sem a área congelada de 1 m da torre.
- Enraizar = lentidão máxima do port (−90%): ainda não há "sem andar" separado do atordoamento.
- Vento Arrepiante sem empurrão (aguardando manopla-gelo-004) e sem saída antecipada.
- Sair da tumba: só pelo fim (10 s) ou quebrando. A esquiva não sai: o clipe de laço `ice_gnt_entombed_loop` tem 2
  quadros e a janela `CanDodgeCancel` (0,2 s do laço) não cabe na montagem. Apertar a habilidade de novo também não.
- Efeitos Niagara sem luz (`LightSource`), sem fitas (`Connection`), sem curva de tamanho de malha e sem curvas de cor
  e tamanho (só alpha); decalque sem curva de alpha; `EmitterStrength`, `MotionBlur`, `SoftParticle` e a timeline de
  PostFX de fora.

- `ice_gnt_pose_add` (pose aditiva da manopla nas esquivas, parado e andando) fica de fora: o compositor de clipes
  substitui curvas em vez de somar. Locomoção = `melee1h_cmbt_nav_strafe_blend` + `rswrd_ststrafe_blend` +
  `ice_gnt_idle_rhand` (variante `Strafe [MagicGauntlet+Ice+AttackAction]` do jogo); esquivas com `cmbt_1h_quickstep_*`.
- Leve e pesado usam o mesmo clipe (`ice_gnt_lt_atk_up`): o import soma as janelas dos dois fragmentos na mesma
  sequência. No leve, o próximo clique abre em 0,833 s (janela do pesado) em vez de 0,933 s.
- Reação ao bloqueio e quebra de guarda: as da machadinha (a manopla não tem fragmento próprio).
- `IceMagic_HeavyAttackSlow`: sem o `ApplicationCooldown` de 6 s (o port reaplica a cada pesado).
- Projétil sai do ponto padrão do port (centro do corpo + 70 cm), não de (0,3; 0,3; 1,2) m; malha do projétil = cristal
  da palma (prefab `IceMagic_Primary_proj` não convertido).
- Mana sem regeneração até manopla-gelo-001.
- Antebraço direito da armadura não é escondido sob a luva (`RightHandOnlySkin`): pode atravessar.
- Manopla guardada (outra arma ativa): só o cristal aparece no quadril.
- Sons `play_gauntlet_ice_*` sem áudio: o banco da manopla não está em `tools/extract_audio.py` (`SFX_GROUPS`); entra
  na parte de som.
- O export calculou janela de dano pela mão em `AM_Ig_HeavyCharge` e `AM_Ig_Heavy` (nome com "heavy"); a carga não tem
  DamageID e o pesado usa projétil, então a janela não acerta nada. Conferir no teste 3.
