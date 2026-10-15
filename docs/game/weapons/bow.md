# Bow

Survey of the original game's Bow, and the port requirements. Structured data: `extract/research/bow.json` (same
information, with every window per fragment). Shared rules: [../combat.md](../combat.md).

Conventions:

- Times in seconds of the Mannequin fragment (ProcLayer of the `.adb`). "end" = until the end of the fragment.
- "row N" = index in `rows` of the datasheet JSON (base 0).
- Table paths are relative to `extract/data/sharedassets/springboardentitites/datatables/`.
- Actionlists (CAGE logic) are in `extract/serverpak/client/sharedassets/springboardentitites/playerdata/cage/`.

## Sources

| Data | File |
|---|---|
| Bow item | `javelindata_itemdefinitions_weapons` row 111 (`BowT5`) |
| Arrow (ammo) | `javelindata_itemdefinitions_ammo` (`ArrowT5`) |
| Damage | `javelindata_damagetable` rows 172–193 |
| Stamina | `javelindata_staminacosts_player` rows 27–31 and 63–67 |
| Cooldowns | `javelindata_cooldowns_player` rows 30–35 |
| Spells (rain, poison, explosion) | `javelindata_spelltable_bow` rows 0–9 |
| Abilities and mastery | `weaponabilities/javelindata_ability_bow` (40 rows) |
| Status effects | `javelindata_statuseffects_bow` (20 rows) |
| Aim/lock profile | `combatsettings/javelindata_combatprofiles` row 2, `javelindata_combatsettings` rows 6–7 |
| Animations and windows | `extract/models/animations/mannequin/adb/player/player_bow.adb` + `player.adb` |
| Bow string | `extract/models/animations/mannequin/adb/playermaleanims_bowattach.adb` |
| State rules | `playeractions_archery.actionlist`, `playeractions_ability_bow.actionlist`, `playeractions_nav.actionlist`, `playeractions_camera.actionlist`, `playeractions_dodge.actionlist`, `playerinput_combat.actionlist` |
| Keys | `extract/config/libs/config/defaultprofile.xml` lines 407–612 |
| Camera | `extract/camera_gamecamera.xml` |
| Texts | `extract/loc/localization/pt-br/weaponabilities.loc.xml` (and `en-us`) |
| Projectile and AoE (via nwbt) | `sharedassets/genericassets/rangedattackdatabase.radb`, `slices/items/weapons/basic_arrow*.dynamicslice`, `smoke_arrow*`, `spell_aoe_rainofarrowslarge`, `spell_aoe_explosionshot*`, `spell_bow_smoke*`, `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cdf` and `.chrparams` |

The values read from the files fetched with nwbt are in `bow.json`.

## 1. Basic mechanics

### Item and ammo

| Field | Value | Source |
|---|---|---|
| Mannequin tags | `Bow,LongBow` | BowT5 `MannequinTag` |
| Base damage | 84 (same in T1–T5) | `BaseDamage` |
| Critical | chance 0.08, multiplier 1.3 | `CritChance`, `CritDamageMultiplier` |
| Base stagger | 38; block stamina damage 38 | `BaseStaggerDamage`, `BlockStaminaDamage` |
| Base accuracy | 91 (T5) | `BaseAccuracy` |
| Scaling | Dexterity 0.0095, Strength 0.004 | `ScalingDexterity`, `ScalingStrength` |
| Projectile profile | `Bow` | `RangedAttackProfile` |
| Fire joint | `arrowAttach_tip_jnt` | `FireJoint` |
| Ammo | `Arrow`, mesh `jav_wep_gear_arrowa.cgf` | `AmmoType`, `AmmoMesh` |
| Arrow ArrowT5 | `DamageModifier` 1.2, `StaggerDamageModifier` 1 | `javelindata_itemdefinitions_ammo` |

Ammo: each shot consumes 1 arrow from the `arrow-ammo` slot (`UseAmmo`, `ConsumeAmmo`, `AmmoCountToConsume=1` in
`CAGE-RangedAttack`). Without arrows the game plays `Bow_Empty_UpperBody`/`Bow_Empty_HipFire` (empty quiver
animation). The game has the cvar `javelin.enable-ammoless-weapons`, which removes the ammo requirement.

### Controls

| Key | Game action | Effect |
|---|---|---|
| Left, not aiming | `attack_shoot` (mouse1) | Quick unaimed shot (`Bow_Shoot_HipFire`) |
| Right held | `aim_draw` (mouse2) → `aim_draw_active_virtual` | Aim (`Bow_ReadyWeapon` → `Bow_AimChargeShoot_SubState`). There is an option to toggle instead of hold |
| Left while aiming, quick release | `aim_shoot_release` | Aimed light shot (`Bow_Shoot`) |
| Left while aiming, held | `aim_shoot_hold` | Draw (`Bow_Charge_UpperBody`) |
| Release right | `Aim_Exit_Bow` | Lowers the bow (`Bow_Lower_UpperBody`) |

Pressing right with the weapon sheathed draws it and goes straight into aim (`Trigger_From_Sheathed`).

### State flow

1. **Not aiming, left**: `Bow_Shoot_HipFire`. The arrow leaves at 0.4 s with `BowAttack_Hipshot` (60%). Holding the
   button repeats: a new shot is buffered from 0.4 to 1.0 s and fired at 1.0 s (1.0 s cycle).
2. **Right**: `Bow_ReadyWeapon_UpperBody` (0.433 s). Tag `Aim` at 0.07 s; aim camera and walk from the start (tag
   `AimDownSights`). Releasing left between 0.033 and 0.4 s buffers a shot that leaves at 0.4 s.
3. **Aim idle** (`Bow_Aim_UpperBody`, loop). Left:
   - release before 0.5 s → `Bow_Shoot_UpperBody`: arrow at 0.033 s, `BowAttack1` (100%), `ChargeValue` 0;
   - hold → `Bow_Charge_UpperBody` (clip at 2× speed). At **0.5 s** the `SetChargedAim` marker turns on full charge.
     Release after that → `Bow_Shoot_Charged`: arrow at 0.033 s, `BowAttack2` (150%), `ChargeValue` 100. At 0.667 s,
     if still held, goes to the full-charge loop (`Bow_Aim_UpperBody|Bow+ChargedAim`).
   - Full charge held for **3.0 s** (tag `ChargeTimer`, `DelayTimer=3.00`) → `Bow_Shake_UpperBody` (bow shaking).
     The data has no damage or accuracy penalty for the shake.
   - There is no intermediate damage: the charge is binary (`BowAttack1` or `BowAttack2`).
4. **After the shot**, with right still held: at 0.467 s `Bow_Reload_UpperBody` starts (nocks the next arrow). A new
   light shot is buffered from 0.133 to 0.667 s and leaves at 0.667 s; a new draw from 0.733 s; with no input,
   back to the aim loop at 1.167 s. Minimum aimed light shot cycle: 0.467 + 0.667 ≈ 1.13 s. Without right:
   `Bow_Lower_FromShoot_UpperBody` (or `_Charged_`) at 0.467–0.5 s.
5. **No stamina** (≤ 0): variants `Bow_ReadyWeapon_Exhausted_UpperBody` (clip at 0.5×) and
   `Bow_Reload_Exhausted_UpperBody` (0.55×), with stamina regeneration off.

### Movement and camera

| Item | Value | Source |
|---|---|---|
| Speed while aiming | 1.65 m/s (`Stand_Walk`) | `playeractions_nav.actionlist`: Aim, ChargedAim, HeavyAim, SmokeAim, Shoot, ForceWalk, AimDownSightsAbility |
| Combat speed not aiming | 3.25 m/s (`Combat_Nav_Stand`) | same |
| Body turn while aiming | TurnRate 0.35 (`Aiming_Or_Shooting`); 0.40 in ReadyWeapon; 0.50 in the HipFire `AlignToCamera` | same, `playeractions_archery.actionlist` |
| Accuracy bonus standing/aiming | 12.25 default; 25 aiming while standing | `SetWeaponAccuracyBonus` in `playeractions_nav.actionlist` |
| Combat camera | `MidCamera` (line 1505): FOV 50, Offset (0.5; 0; 1.7), Distance 2.0 | `camera_gamecamera.xml` |
| Aim camera | `AimCamera` (line 211): inherits MidCamera, **Distance 1.0**, MinZoom/MaxZoom 0, `IsADS`, DistanceDampening 0.05; entry transition 0.09 s; back to MidCamera in 0.09 s (to DynamicCamera it would be 0.5 s) | same |
| FOV while aiming | unchanged: FOV 50 inherited with `UseUserDefinedFov=true`. The "zoom" is only the camera moving closer (2.0 → 1.0 m) | same |
| Camera choice | `Aim+Stand` (tag `AimDownSights`) → AimCamera | `playeractions_camera.actionlist` |

Target lock: profile `BowGeneric`/`BowCameraLock`; `BreakLockOnADS` true (aiming releases the lock).

### Dodge, draw and sheathe

| Action | Fragment | Stamina | Windows |
|---|---|---|---|
| Dodge (light load) | `Dodge_Roll_Fast` with `Bow` / `Bow+BackNav` / `+LeftNav` / `+RightNav` | 45 (`Dodge_Roll_Fast`, row 66) | invulnerable 0–0.5; pays stamina at 0.1; `CanAttackCancel` 0.267; `CanTransition` 0.5; chains another dodge 0.267–0.5 |
| Dodge (normal load) | `Dodge_Roll` (quickstep) | 45 (row 65) | same windows |
| Dodge (heavy load) | `Dodge_Roll_Slow` | 50 (row 67) | same windows |
| Draw | `Unsheathe_UpperBody|Bow` (`bow_idle_draw_ub` at 1.2×) | 0 | socket swap at 0.2 (`unsheathe_weapon`); `CanTransition_UpperBody` 0.433 |
| Sheathe | `Sheathe_UpperBody|From_Bow` (`bow_idle_sheath_ub`) in `player.adb` | 0 | socket swap at 0.533 (`sheathe_weapon`); `CanTransition_UpperBody` 1.0 |

The dodge fragment depends on the equip load (`ActionConditionEquipLoadCategory` in `playeractions_dodge.actionlist`).
The port's sword uses `Dodge_Roll_Fast`; the bow uses the same.

The bow has no sprint attack of its own (no `Attack_Sprint` in `player_bow.adb`).

Bow melee hit (`Bow_GunButt|Bow+LongBow`, damage `Bow_Gunbutt` 0.5 Strike, capsule radius 0.3 m on the left hand,
`CAGE-Damage` 0.567–0.667): the action `Bow_Gunbutt` has `ActivationCondition Not True` and is only reachable by a
transition from `Attack_Start_Charge|Bow`, which the current bow does not use. Treat as unused.

### Projectile

Profile `Bow` in `rangedattackdatabase.radb` + slice `basic_arrow.dynamicslice`:

| Field | Value |
|---|---|
| Speed | 100 m/s (`m_basespeed`; also `ProjectileSpeed=100` in each `CAGE-RangedAttack`) |
| Extra speed | 25 m/s, `m_timeformaxspeed` 1 s (likely reading: extra from the charge; not confirmed) |
| Gravity | −9.8 m/s² (`m_accel`) |
| Straight flight before gravity | 5 m (`Basic`/`Basic_Heavy`); 10 m with Arrow Range (`Basic_Plus*`) |
| Lifetime | 10 s flying, 8 s stuck |
| Max range | 100 m |
| Arrow radius | 0.15 m (light), 0.275 m (charged and abilities) |
| Penetration | 0.2 m |
| Damage falloff | start 30 m, max 80 m, `m_projectiledamagemaxfalloff` 1 (no reduction in practice) |
| Origin/aim | joint `aim_direction` (`m_aimjoint`, `AimJoint`) |
| Fly-by sound | `Arrow_WhizzBy` at 15 m; `Play_InFlight_ArrowBys` |

Attacks of the `Bow` profile (radb):

| Name | Used by | Range | Radius | Straight flight before gravity | Note |
|---|---|---|---|---|---|
| Basic | HipFire, AimShot | 100 | 0.15 | 5 | |
| Basic_Plus | same with Arrow Range | 100 | 0.15 | 10 | |
| Basic_Heavy | ChargedShot, EvadeShot | 100 | 0.275 | 5 | |
| Basic_Plus_Heavy | same with Arrow Range | 100 | 0.275 | 10 | |
| TripleShot | RapidShot | 100 | 0.275 | 20 | |
| PenetratingShot (A1, A2) | Penetrating Shot | 50 | 0.275 | 30 (60, 100) | `m_hitrate` 1000; description says 100 m |
| SmokeShot | Poison Shot | 100 | 0.275 | 20 | penetration 0 |
| ExplosionShot | Explosive Arrow | 100 | 0.275 | 10 | penetration 0 |
| SpreadShot | (unused) | 10 | 0.275 | 20 | |

## 2. Basic actions

Layers: the first list is the full body (`unarmed_*`), the second is the weapon layer. `_UpperBody` fragments have a
single layer and play over locomotion.

| Action | Fragment (tags) | Clips | Shot / main window | DamageID (row) | Stamina (CostID) | Cancel |
|---|---|---|---|---|---|---|
| HipFire | `Bow_Shoot_HipFire` (Bow) | `bow_quickfire_ub` @1.25 | `CAGE-RangedAttack` Basic at 0.4; arrow in hand 0.067–0.133; `AlignToCamera` 0–0.4 | `BowAttack_Hipshot` (178): 0.6 Thrust | 0 (`Attack_2H_Bow_Primary`) | dodge 0–0.267 and 1.0+; `CanCancel_UpperBody` 1.0 |
| AimStart | `Bow_ReadyWeapon_UpperBody` (Bow) | `bow_idle_to_aim_halfdrw_ub` | arrow in hand 0.1–0.333; buffered shot 0.033–0.4; to aim at 0.433 | — | 0 | dodge 0.267+ |
| AimIdle | `Bow_Aim_UpperBody` (Bow) | `bow_aim_halfdrw_idle_ub` (loop) | — | — | — | `CanCancel_UpperBody` always |
| AimShot | `Bow_Shoot_UpperBody` (Bow) | `bow_aim_halfdrw_release_ub` | `CAGE-RangedAttack` Basic at 0.033 | `BowAttack1` (172): 1.0 Thrust | 0 (`Attack_2H_Bow_Aim`) | dodge 0.433+; reload/lower 0.467 |
| Charge | `Bow_Charge_UpperBody` (Bow) | `bow_aim_halfdrw_to_fulldrw_walk_ub` @2 | full charge at 0.5 (`SetChargedAim`) | — | 0 (`Attack_2H_Bow_Aim_Charge`) | release before 0.5 = AimShot |
| ChargedIdle | `Bow_Aim_UpperBody` (Bow+ChargedAim) | `bow_aim_fulldrw_idle_ub` (loop) | shake after 3.0 s | — | — | |
| Shake | `Bow_Shake_UpperBody` / `Bow_Shake_End_UpperBody` (Bow+ChargedAim) | `bow_aim_fulldrw_walk_shake_ub`, `..._shake_end_ub` | visual only | — | — | |
| ChargedShot | `Bow_Shoot_Charged` (Bow) | `bow_aim_fulldrw_release_ub` | `CAGE-RangedAttack` Basic_Heavy at 0.033, `ChargeValue` 100 | `BowAttack2` (173): 1.5 Thrust | 0 (`Attack_2H_Bow_Primary_Charge`) | dodge 0.433+; reload 0.467; lower 0.5 |
| Reload | `Bow_Reload_UpperBody` (Bow) | `bow_aim_halfdrw_reload_ub` | arrow in hand 0.2–0.633; shot 0.667 (buffer 0.133); charge 0.733; back to aim 1.167 | — | — | dodge always |
| ReloadCharged | `Bow_Reload_Charged_UpperBody` (Bow) | `bow_aim_fulldrw_reload_ub` | shot 0.933 | — | — | |
| AimEnd | `Bow_Lower_UpperBody` (Bow / Bow+ChargedAim) | `bow_aim_halfdrw_to_idle_ub` / `bow_aim_fulldrw_to_idle_ub` | `CanTransition_UpperBody` 0.067; exits at 0.367 | — | — | |
| AimEndFromShot | `Bow_Lower_FromShoot_UpperBody` / `_Charged_` (Bow) | `bow_aim_halfdrw_release_to_idle_ub` / `bow_aim_fulldrw_release_to_idle_ub` | exits at 0.367 | — | — | |
| Dodge_F/B/L/R | `Dodge_Roll_Fast` (Bow / +BackNav / +LeftNav / +RightNav) | `unarmed_dodgeroll_{f,b,l,r}_light_to_crch` + `bow_dodgeroll_{f,b,l,r}_light_to_crch` | invulnerable 0–0.5 | — | 45 (`Dodge_Roll_Fast`) | `CanTransition` 0.5 |
| Draw | `Unsheathe_UpperBody` (Bow) | `bow_idle_draw_ub` @1.2 | socket swap 0.2 | — | — | 0.433 |
| Sheath | `Sheathe_UpperBody` (From_Bow, player.adb) | `bow_idle_sheath_ub` | socket swap 0.533 | — | — | 1.0 |
| Hit_F/B/L/R | `r_R2_{F,B,L,R}` (Bow) | `unarmed_r2_*` + `bow_r2_*` | — | — | — | |
| Death | `r_Death` (no tag, player.adb) | `unarmed_death_droptokneesfaceplant` | — | — | — | |
| StunLoop | `r_Stun_Loop` (Bow) | `unarmed_r_stun_loop` | — | — | — | |

Damage rows (all `WeaponCategory` Bow, `IsRanged`, `DmgCoefHead` 1.3, `HitStun` 15, `BlockHitStun` 12):

| DamageID | Row | DmgCoef | Type | Stagger / block | Other |
|---|---|---|---|---|---|
| BowAttack1 | 172 | 1.0 | Thrust, Light | 0.35 / 0.35 | |
| BowAttack2 | 173 | 1.5 | Thrust, Heavy | 1.25 / 1.25 | |
| BowAttack_Hipshot | 178 | 0.6 | Thrust, Light | 0.3 / 0.3 | |

String animations (bow skeleton), from `playermaleanims_bowattach.adb`: ReadyWeapon `bow_idle_to_aim_halfdrw_prop`,
Aim `bow_aim_halfdrw_idle_prop`, Charge `bow_aim_halfdrw_to_fulldrw_prop`, ChargedIdle `bow_aim_fulldrw_idle_prop`,
Shoot `bow_aim_halfdrw_release_prop`, ChargedShot `bow_aim_fulldrw_release_prop`, Reload
`bow_aim_halfdrw_reload_prop`, HipFire `bow_quickfire_prop`, Shake `bow_aim_fulldrw_shake_prop`, abilities
`bow_spc_*_prop`. Files in `animations/gameplay/prop/weapons/bow/base_bow/`.

## 3. Abilities

Trees: 0 = **Skirmisher** (`ui_bow_tree1`), 1 = **Hunter** (`ui_bow_tree2`). Mastery: Bow Mastery
(`ui_bow_mastery`). All abilities cost 0 stamina. The cooldown starts after the `Ability_Startup` marker (comment
"Activate CoolDown after startup" in the actionlist).

### Actives

| AbilityID | Name | Cooldown | DamageID (row) | Hit shape | Fragment(s) and times |
|---|---|---|---|---|---|
| Ability_Bow_RapidShot | Rapid Shot | 14 s | `Bow_RapidAttack_Standard` (176) 1.0 ×2; `Bow_RapidAttack_KnockBack` (177) 1.25, HitStun 30 | 3 `TripleShot` arrows at 0.333 / 0.567 / 0.8; `AccuracyBonus` 10 | `Ability_Bow_RapidShot` (Bow+LongBow), `bow_spc_attack_1` full body; dodge 0.567–0.867, 1.1–1.733, 2.333+; `CanTransition` 2.6 |
| Ability_Bow_EvadeShot | Evade Shot | 15 s | `BowAttackJumpBack` (174) 0.9 | 1 `Basic_Heavy` arrow at 0.733; 5 m jump back | `Ability_Bow_JumpBackShot` (Bow+LongBow), `bow_spc_5_rapidfire_jump`; root motion without gravity until `Enable_Gravity` 0.867; dodge 1.3; `CanTransition` 1.433; camera `AimCameraJumpBack` |
| Ability_Bow_PenetratingShot | Penetrating Shot | 18 s | `Bow_Penetrating_Attack` (187) 1.5, HitStun 25, stagger 1.5 | `PenetratingShot` arrow that passes through targets; range 50 (radb) or 100 m (description) | the key enters its own aim (tag `PenetratingAim`): `Bow_ReadyWeapon_UpperBody` (0.433) → `Bow_Aim_UpperBody` → fire with left or the key again: `Bow_Shoot_UpperBody|Bow+PenetratingAim`, `bow_spc_3_aim_fulldrw_release_ub`, arrow at 0.033; `CanCancel_UpperBody` 0.7 |
| Ability_Bow_PoisonShot | Poison Shot | 20 s | `Bow_PoisonShot` (180) 0.5 NoHeadshot; cloud `Bow_PoisonShotCloud` (182) | `SmokeShot` arrow; on hit or after 2 s creates a cloud, sphere radius 3 m, height 5 m, 6 s | own aim (tag `SmokeAim`): ReadyWeapon → Aim → `Bow_Shoot_UpperBody|Bow+SmokeAim`; marker `ShootPoisonShot` 0.033–0.1 casts `Spell_Ability_Bow_PoisonShot_Projectile` |
| Ability_Bow_RainofArrows | Rain of Arrows | 20 s | `Bow_RainOfArrows_Attack` (186) 1.5, NoHeadshot, NoBackstab | ground area radius 4 m, height 5 m; target up to 50 m; one hit between 0.6 and 0.8 s after impact | high aim (tag `HeavyAim`): `Bow_ReadyWeapon_UpperBody|Bow+HeavyAim` (`bow_idle_to_spc_4_aim_idle` @1.5, 0.4 s) → `Bow_Aim_UpperBody|Bow+HeavyAim` with ground indicator → `Bow_Shoot_RainOfArrows` (intro+outro @1.5), spell at marker `Bow_Spell_RainOfArrows` 0.267–0.567; `CanTransition` 0.567; cancel with right or block |
| Ability_Bow_SplinterShot | Explosive Arrow | 22 s | `Bow_ExplosionShot_Projectile` (190) 0.5; explosion `Bow_ExplosionShot_Explosion` (191) 1.35 Fire, NoHeadshot | `ExplosionShot` arrow; explosion sphere radius 2.5 m on collision | `Ability_Bow_ExplosiveArrow` (Bow+LongBow), `bow_spc_attack_2_ub` (upper body); `CAGE-CastSpell` `Spell_Ability_Bow_Explosive_Base` at 0.467; `CanCancel_UpperBody` 1.167 |

What the tooltips say (numbers already computed):

- **Rapid Shot**: 3 consecutive arrows: 100%, 100% and 125% of weapon damage.
- **Evade Shot**: jumps 5 m back and fires one arrow for 90% of weapon damage.
- **Penetrating Shot**: one arrow for 150% of weapon damage that passes through targets. Range 100 m.
- **Poison Shot**: one arrow for 50% of weapon damage. On hit or on landing it creates a poison cloud of radius 3 m
  lasting 6 s. Enemies in the cloud are poisoned for 20% of weapon damage per second for 7 s. No headshots.
- **Rain of Arrows**: a barrage covering a 4 m radius for 150% of weapon damage. No headshots.
- **Explosive Arrow**: one arrow for 50% of weapon damage on hit, then an explosion for 135% extra damage to all
  targets within 2.5 m.

Spell details (`javelindata_spelltable_bow`):

| SpellID | Row | Type | Values |
|---|---|---|---|
| Spell_Bow_Spell_RainOfArrows | 0 | AoE | Duration 5000 ms, Height 5, Radius 4, CastDistance 50, CastHeight 60, target Enemy, CheckObstructions; prefab `spell_aoe_rainofarrowslarge` (volume trigger at 0.6 s, off at 0.8 s) |
| Spell_Ability_Bow_PoisonShot_Projectile | 1 | Projectile | Duration 2000 ms, secondary on collision **and** on expiry → `Spell_Ability_Bow_PoisonShot_Aoe`; `FireJointName` aim_direction |
| Spell_Ability_Bow_PoisonShot_Aoe | 3 | AoE sphere | Duration 6000, Radius 3, Height 5, status `Status_Bow_Poison` |
| Spell_Ability_Bow_Explosive_Base | 6 | Projectile | Duration 500 ms, secondary only on collision → `Spell_Ability_Bow_ExplosiveShot_Explosion` |
| Spell_Ability_Bow_ExplosiveShot_Explosion | 8 | AoE sphere | Radius 2.5, Duration 1000; slice uses `Bow_ExplosionShot_Explosion` |
| Spell_Ability_Bow_ExplosiveShot_Explosion_SizeChange | 9 | AoE sphere | Radius 1.25; slice uses `Bow_ExplosionShot_Explosion_Dmg` (1.7) |

### Active upgrades

| AbilityID | Name | Effect (data) |
|---|---|---|
| Upgrade_Bow_RapidAccuracy_CDROnLast | Rapid Accuracy | 3 hits in a row on the same target: cooldown −50% |
| Upgrade_Bow_RapidAccuracy_DmgOnLast | Final Blow | 3rd arrow (`Bow_RapidAttack_KnockBack`) +25% base damage |
| Upgrade_Bow_EvasiveShot_Knockback | Evasive Knockback | switches the row to `BowAttackKnockBack` (175): 0.9, PowerLevel 3, HitStun 30, knockback 2 m |
| Upgrade_Bow_EvasiveShot_Haste | Go the Distance | hit gives `Status_Upgrade_Bow_EvasiveShot_Haste`: +12% speed for 5 s |
| Upgrade_Bow_PenetratingShot_DmgPerHit | Blood Soaked Arrow | switches to `Bow_Penetrating_AttackA1` (188): +10% per hit, max +50% |
| Upgrade_Bow_PenetratingShot_DmgPerDist | Deep Strike | +15% base damage at ≥ 20 m |
| Upgrade_Bow_PoisonShot_PoisonDmg | Infected Arrows | cloud applies `Status_Bow_Poison_Upgrade`: 24%/s for 7 s |
| Upgrade_Bow_PoisonShot_DirectDmg | Direct Hit | direct hit uses `Bow_PoisonShot_Upgrade` (181): 1.0 |
| Upgrade_Bow_RainofArrows_Bleed | Barbed Arrows | `Status_Passive_RainofArrows_Bleed`: 9%/s for 12 s, tick 1 s |
| Upgrade_Bow_RainofArrows_Slow | Hooked Arrows | `Status_Bow_RainOfArrows_Slow`: −15% speed for 4 s |
| Upgrade_Bow_SplinterShot_CDRPerHIt | Flame Arrow | direct hit applies `Status_ExplosiveShot_Burn`: 8%/s Fire for 10 s |
| Upgrade_Bow_SplinterShot_FiveA | Contained Detonation | explosion 170% (`Bow_ExplosionShot_Explosion_Dmg`, 192), radius 1.25 m |

### Status effects of the actives

| StatusID | Row | Duration | Tick | Effect |
|---|---|---|---|---|
| Status_Bow_Poison | 13 | 7 s (max 15) | 1 s | −20% of weapon damage per tick, Nature |
| Status_Bow_Poison_Upgrade | 16 | 7 s (max 20) | 1 s | −24% per tick, Nature |
| Status_Passive_RainofArrows_Bleed | 9 | 12 s (max 24) | 1 s | −9% per tick, Standard |
| Status_Bow_RainOfArrows_Slow | 8 | 4 s (max 8) | — | speed −15% |
| Status_ExplosiveShot_Burn | 18 | 10 s (max 20) | 1 s | −8% per tick, Fire |
| Status_Upgrade_Bow_EvasiveShot_Haste | 0 | 5 s | — | speed +12% |

`HealthModifierDamageBased` is a fraction of weapon damage per tick.

### Passives

| AbilityID | Tree | Name | Effect |
|---|---|---|---|
| Passive_Bow_Skirmish_HasteWhenFoesNear | Skirmisher | Catch Me If You Can | +20% speed with 3+ enemies near (spell radius 4 m; text says 3 m) |
| Passive_Bow_Skirmish_RendHead | Skirmisher | Rending Headshot | headshot: target armor −10% for 5 s |
| Passive_Bow_Skirmish_SlowFull | Skirmisher | Impale | hit on a target at 100% health: −10% speed for 2 s |
| Passive_Bow_Skirmish_CDRVsLow | Skirmisher | Closing In | hit on a target < 50%: cooldowns −5% |
| Passive_Bow_Skirmish_DodgeHaste | Skirmisher | Dodge and Weave | dodge with full stamina: +7% speed for 2 s |
| Passive_Bow_Skirmish_StaminaVsDebuff | Skirmisher | Hunter's Insight | hit on a debuffed target: +5 stamina |
| Passive_Bow_Skirmish_HasteOnSwap | Skirmisher | Archer's Speed | swapping to the bow: +7% speed for 5 s (cooldown 10 s) |
| Passive_Bow_Skirmish_DmgVsDebuff | Skirmisher | Mark | +10% base damage on a debuffed target |
| Passive_Bow_Skirmish_DebuffDur | Skirmisher | Battle Precision | debuffs +20% duration |
| Ultimate_Bow_Skirmish | Skirmisher | Evasive Tactics | after a dodge: +15% damage for 5 s |
| Passive_Bow_Hunter_DmgVsDist | Hunter | Long Range | +10% base damage at ≥ 10 m |
| Passive_Bow_Hunter_HeavyDmg | Hunter | Aim True | heavy attack +10% base damage and faster arrow |
| Passive_Bow_Hunter_ArmorAimHeavy | Hunter | Unbreakable Focus | +10% armor while aiming |
| Passive_Bow_Hunter_DmgVsLow | Hunter | Finishing Shot | +15% base damage on a target < 50% |
| Passive_Bow_Hunter_HeavyDmgVsFull | Hunter | Opening Strike | heavy +10% base damage on a target at 100% |
| Passive_Bow_Hunter_DmgOnFirst | Hunter | Surprise Attack | text: +15% if no damage dealt in 10 s; table: `BaseDamage` 0.2 |
| Passive_Bow_Hunter_Accuracy | Hunter | Arrow Range | straight flight before gravity ×2 (5 → 10 m: `Basic_Plus`) |
| Passive_Bow_Hunter_HealOnFive | Hunter | Hawkeye | headshot heals 10% of the damage |
| Passive_Bow_Hunter_Crit | Hunter | Bullseye | +10% critical chance |
| Ultimate_Bow_Hunter | Hunter | Concussion | headshot +20% damage |

## 4. Locomotion, loops and aim

| Proposed name | Clips (layers) | Source fragment |
|---|---|---|
| BS_Bw_Strafe | `unarmed_cmbtstrafe_blend` + `bow_ststrafe_blend` | Strafe (Bow) |
| BS_Bw_Sprint | `unarmed_sprint_veerslope_v2_blend` + `bow_sprint_veerslope_blend` | Sprint (Bow) |
| BS_Bw_AimStrafe | `bow_aim_halfdrw_strafe_blend` | Strafe (Bow+Aim); also PenetratingAim, SmokeAim, HipAim |
| BS_Bw_ChargedStrafe | `bow_aim_fulldrw_strafe_blend` | Strafe (Bow+ChargedAim) |
| BS_Bw_RainStrafe | `bow_spc_4_strafe_blend` | Strafe (Bow+HeavyAim) |
| BS_Bw_Turn / AimTurn / ChargedTurn | `bow_actn_idle_turn_blend` / `bow_aim_halfdrw_idle_turn_blend` / `bow_aim_fulldrw_idle_turn_blend` | Turn_InPlace |
| BS_Bw_RunStop / SprintStop | `bow_run_stop_blend` / `bow_sprint_stop_blend` (+ unarmed) | Nav_Stop |
| Bw_Idle | `unarmed_idle` + `bow_idle` | Idle (Bow) |
| Bw_AimIdle | `bow_aim_halfdrw_idle` (full body) | Idle (Bow+Aim) |
| Bw_ChargedIdle | `bow_aim_fulldrw_idle` | Idle (Bow+ChargedAim) |
| Bw_RainAimIdle | `bow_spc_4_aim_idle` | Idle (Bow+HeavyAim) |
| Bw_ExhaustIdle | `unarmed_exhaust_idle` + `bow_exhaust_idle` | Idle (Bow+Exhausted) |
| Bw_FallLoop | `unarmed_fall_flail_loop` + `bow_fall_flail_loop` | Fall (Bow) |

Aim poses 15× (pitch/yaw aim offset, fragment `Aim`, action `AimPose` in `playeractions_aim.actionlist`):
`bow_halfdrw_aim_poses_15x_ub` (Aim), `bow_fullfdrw_aim_poses_15x_ub` (ChargedAim), `bow_spc_4_aim_poses_15x_ub`
(HeavyAim), `bow_quickfire_aim_poses_15x_ub` (HipAim). Versions without `_ub` for standing still (`+Idle`).

Jump and landing follow the sword pattern with `v2_bow_jump_walk_*`, `v2_bow_jump_walk_f_to_land_walk_*`,
`v2_bow_jump_inplace`, `bow_fall_land_to_idle`, `bow_fall_hardland_to_idle` (list in `bow.json` → `montages_extra`).

Crouch (`Crouch`) has variants of almost everything; the port has no crouch and ignores them.

## 5. Meshes and sockets

| Item | Path |
|---|---|
| Bow T5 (skinned, with skeleton) | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cdf` → `wep_ran2h_bowa.chr` + `wep_ran2h_bowa_t5.skin` |
| String | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t1_string.skin` (item `SkinOverride2`, same in every tier) |
| Bow T5 static | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_t5.cgf` (no animated string) |
| Material | `objects/weapons/ranged/bows/bowa/wep_ran2h_bowa_mat.mtl` |
| Bow bones | `bow_root_jnt`, `bow_localRoot_jnt`, `bowStringPull_jnt`, `upperBow_1/2_jnt`, `upperBowString(_end)_jnt`, `lowerBow_1/2_jnt`, `lowerBowString(_end)_jnt`, `arrowAttach_tip_jnt` |
| Arrow | `objects/weapons/ranged/projectiles/arrowa/jav_wep_gear_arrowa.cgf` (same for the projectile and the hand) |
| Quiver (optional) | `objects/props/weapons/quiver/jav_quiver.cgf` |

Sockets (attachments of `player_male.cdf`):

| Use | Attachment | Bone | Source |
|---|---|---|---|
| Bow in hand | `lefthandattach` | `left_hand_attach` | inferred (no table links EquipType Bow to a socket; the arrow goes to the right hand) |
| Arrow in hand | `righthandattach` | `right_hand_attach` | `CAGE-Attachment` `AttachmentSource=ActiveWeapon` in the draw/reload fragments |
| Bow sheathed | `bow` | `Spine1`, RelPosition (−0.10805; 0.073166; −0.169168), RelRotation (0.384488; 0.599706; −0.374635; 0.593439) | `player_male.cdf` |
| Quiver | `quiver` | `Spine2` | same |

## 6. Damage rules

- Expected damage: `84 × DmgCoef × 1.2 (arrow) × 1.3 if head`. The arrow's 1.2 is likely from the field name; the data
  has no explicit formula.
- Head: `DmgCoefHead` 1.3 on every bow row. `NoHeadshot` on `Bow_PoisonShot`, `Bow_PoisonShot_Upgrade`,
  `Bow_PoisonShotCloud_Vortex`, `Bow_RainOfArrows_Attack`, `Bow_ExplosionShot_Explosion`, `_Explosion_Dmg`,
  `Bow_Spread_Shot`.
- Back: `NoBackstab` on `Bow_RainOfArrows_Attack` and `Bow_ExplosionShot_*`.
- Critical: from the weapon (0.08 chance, ×1.3). In the rows `DmgCoefCrit` = 0 and `CanCrit` = false.
- Reaction: `PowerLevel` 0 on all common rows (does not interrupt); only `BowAttackKnockBack` and `Bow_Gunbutt*`
  have 3.
- Block: a blocked arrow is destroyed (`m_destroyprojectileonblock`). `BlockStaminaDmgMod` = the row's stagger.

## Unknowns

- Duration of the `.caf` clips (the `.caf` files are not extracted; windows marked "end" depend on it).
- Real effect of `m_extraspeed` 25 and of `ChargeValue` 100 on the arrow speed.
- Socket of the bow in hand (inferred as left hand).
- Shake penalty (`Shake`): none in the data.
- Penetrating Shot range: 50 m in the radb, 100 m in the text.
- `Spell_Ability_Bow_Explosive_Base` with Duration 500 ms: unclear whether it limits the arrow range.
- Hold time for `aim_shoot_hold` to become a draw: the input profile defines no delay; treat as immediate.

## Proposal for the port

Montage prefix `AM_Bw_`. Window tags `Bow,LongBow`; adbs `player/player_bow.adb` + `player/player.adb` (an entry in
`window_overrides` like the Hatchet's). Shapes: `("Projectile", 10000, 0)` = 100 m/s in cm/s (the Hatchet throw
uses 2500).

| Action | Montage | Fragments (tags) | DamageID | Stamina | Cooldown | Shape |
|---|---|---|---|---|---|---|
| Bw_Draw | AM_Bw_Draw | Unsheathe_UpperBody (Bow) | — | — | — | — |
| Bw_Sheath | AM_Bw_Sheath | Sheathe_UpperBody (From_Bow) | — | — | — | — |
| Bw_HipFire | AM_Bw_HipFire | Bow_Shoot_HipFire (Bow) | BowAttack_Hipshot | Attack_2H_Bow_Primary (0) | — | Projectile 100 m/s, fired at 0.4 |
| Bw_AimStart | AM_Bw_AimStart | Bow_ReadyWeapon_UpperBody (Bow) | — | — | — | — |
| Bw_AimIdle | AM_Bw_AimIdle (loop) | Bow_Aim_UpperBody (Bow) | — | — | — | — |
| Bw_AimShot | AM_Bw_AimShot | Bow_Shoot_UpperBody (Bow) | BowAttack1 | Attack_2H_Bow_Aim (0) | — | Projectile, fired at 0.033 |
| Bw_Charge | AM_Bw_Charge (hold) | Bow_Charge_UpperBody (Bow) | — | Attack_2H_Bow_Aim_Charge (0) | — | full charge at 0.5 |
| Bw_ChargedIdle | AM_Bw_ChargedIdle (loop) | Bow_Aim_UpperBody (Bow+ChargedAim) | — | — | — | — |
| Bw_Shake | AM_Bw_Shake (loop) | Bow_Shake_UpperBody (Bow+ChargedAim) | — | — | — | after 3 s |
| Bw_ChargedShot | AM_Bw_ChargedShot | Bow_Shoot_Charged (Bow) | BowAttack2 | Attack_2H_Bow_Primary_Charge (0) | — | Projectile radius 0.275 |
| Bw_Reload | AM_Bw_Reload | Bow_Reload_UpperBody (Bow) | — | — | — | — |
| Bw_AimEnd / Bw_AimEndCharged | AM_Bw_AimEnd / AM_Bw_AimEndCharged | Bow_Lower_UpperBody (Bow / Bow+ChargedAim) | — | — | — | — |
| Bw_AimEndFromShot / ...Charged | AM_Bw_AimEndFromShot / AM_Bw_AimEndFromShotCharged | Bow_Lower_FromShoot(_Charged)_UpperBody (Bow) | — | — | — | — |
| Dodge_F/B/L/R | AM_Bw_Dodge_F/B/L/R | Dodge_Roll_Fast (Bow, +BackNav, +LeftNav, +RightNav) | — | Dodge_Roll_Fast (45) | — | invulnerable 0–0.5 |
| Hit_F/B/L/R | AM_Bw_Hit_F/B/L/R | r_R2_* (Bow) | — | — | — | — |
| Death | AM_Bw_Death | r_Death (player.adb) | — | — | — | — |
| RapidShot | AM_Bw_RapidShot | Ability_Bow_RapidShot (Bow+LongBow) | Bow_RapidAttack_Standard ×2, Bow_RapidAttack_KnockBack | — | 14 | 3 projectiles at 0.333/0.567/0.8 |
| EvadeShot | AM_Bw_EvadeShot | Ability_Bow_JumpBackShot (Bow+LongBow) | BowAttackJumpBack | — | 15 | 5 m jump + projectile at 0.733 |
| PenetratingShot | AM_Bw_AimStart + AM_Bw_AimIdle + AM_Bw_PenetratingShot | Bow_Shoot_UpperBody (Bow+PenetratingAim) | Bow_Penetrating_Attack | — | 18 | projectile that passes through, 50 m |
| PoisonShot | AM_Bw_AimStart + AM_Bw_AimIdle + AM_Bw_AimShot | Bow_Shoot_UpperBody (Bow+SmokeAim) | Bow_PoisonShot + cloud | — | 20 | projectile → AoE 3 m, 6 s, poison 20%/s 7 s |
| RainOfArrows | AM_Bw_RainReady + AM_Bw_RainIdle (loop) + AM_Bw_RainShot + AM_Bw_RainEnd | Bow_*\|Bow+HeavyAim, Bow_Shoot_RainOfArrows, Bow_Lower_FromRain_UpperBody | Bow_RainOfArrows_Attack | — | 20 | ground AoE radius 4 m, up to 50 m, hit 0.6–0.8 s |
| ExplosiveArrow | AM_Bw_ExplosiveArrow | Ability_Bow_ExplosiveArrow (Bow+LongBow) | Bow_ExplosionShot_Projectile + _Explosion | — | 22 | projectile → 2.5 m explosion on collision |

Order: HipFire, aim, AimShot, charge and ChargedShot, Reload, dodge; then RapidShot, EvadeShot and PenetratingShot
(projectile only); last PoisonShot, RainOfArrows and ExplosiveArrow (they need AoE and statuses). Default abilities:
RapidShot, EvadeShot, PenetratingShot.

Damage: keep the arrow's `DamageModifier` 1.2 in the damage calculation to get the same numbers. Ammo is infinite;
no `Bow_Empty`.

### Requirements for the C++

1. **Aim mode**: state while right is held; camera switch (AimCamera: distance 1.0, 0.09 s transition both ways with
   MidCamera); speed 1.65 m/s; character turns with the camera (TurnRate 0.35); reticle.
2. **Upper-body montages**: `_UpperBody` fragments play in an upper-body slot over locomotion
   (BS_Bw_AimStrafe / ChargedStrafe / RainStrafe). The AnimBP needs a per-bone blend from the spine.
3. **Aim offset**: `*_aim_poses_15x*` poses for pitch/yaw.
4. **Bow state machine**: ReadyWeapon → aim → charge (0.5 s) → light or charged shot → automatic reload (shot at
   0.667 s) → back to aim; shake after 3 s; HipFire repeating every 1.0 s; buffers from the
   `CAGE-TransitionBuffered` windows.
5. **Projectile with gravity**: replicated actor spawned on the server; 100 m/s, 5 m straight then −9.8 m/s², radius
   0.15/0.275 m, lifetime 10 s, range 100 m; aim along the camera ray; the arrow sticks (8 s). Detect the head for
   `DmgCoefHead`. Penetration for Penetrating Shot (several targets, one hit per target).
6. **Arrow in hand**: show `jav_wep_gear_arrowa` on `right_hand_attach` during the `CAGE-Attachment` windows.
7. **Multiple shots**: Rapid Shot with 3 fire events and different damage rows.
8. **Root motion without gravity**: Evade Shot until `Enable_Gravity` (0.867 s).
9. **Aimed ground area**: Rain of Arrows with an indicator by raycast (up to 50 m), volume radius 4 m × 5 m, one hit
   between 0.6 and 0.8 s, no head and no back.
10. **Persistent area and explosion**: poison cloud (3 m, 6 s, applies status to whoever enters) and explosion on
    collision (2.5 m).
11. **Status effects with tick**: poison, bleed, burn (fraction of weapon damage per second), slow and haste (speed
    multiplier).
12. **Bow mesh**: start with the static `wep_ran2h_bowa_t5.cgf` on `left_hand_attach`; the animated string (`.cdf` +
    `_prop` animations) comes later. Sheathed bow on the `bow` attachment (Spine1).
13. **Ammo**: infinite; no `Bow_Empty`.

## Known differences

Missing in the port: poison cloud, Rain of Arrows indicator, aim offset, animated string, full-charge shake.
