# Rapier

Everything the game defines for the player's Rapier, with sources, and the port requirements. Structured data:
`extract/research/rapier.json` (same numbers, plus the full windows of each fragment). Shared rules:
[../combat.md](../combat.md).

## Sources

| Data | File |
|---|---|
| Damage, coefficients, stun, knockback | `extract/data/.../datatables/javelindata_damagetable.datasheet.json` (rows `1H_Rapier_*`) |
| Stamina | `javelindata_staminacosts_player` |
| Cooldowns | `javelindata_cooldowns_player` (`WeaponTag=Rapier`) |
| Mastery tree | `weaponabilities/javelindata_ability_rapier` (38 rows) |
| Status effects | `javelindata_statuseffects_rapier` (15 rows) |
| Weapon | `javelindata_itemdefinitions_weapons` (`RapierT5`) |
| Camera / target lock | `javelindata_camerashake`, `combatsettings/javelindata_combatprofiles` and `javelindata_combatsettings` (`RapierGeneric`) |
| Animation and windows (character scope) | `extract/models/animations/mannequin/adb/player/player_rapier.adb`, `player_1h_melee.adb`, `player.adb` |
| Damage windows of the attacks (weapon scope) | `extract/models/animations/mannequin/adb/playermaleanims_1h_meleeattach.adb` (item `AnimDbPath`) |
| Action rules (conditions, effects, movement) | `sharedassets/springboardentitites/playerdata/cage/playeractions_ability_rapier.actionlist`, `playeractions_attack`, `_dodge`, `_block`, `_reaction`, `_sheathe` (read with `nwbt cat`; not in `extract/`) |
| Animation events | `animations/gameplay/character/player/male/temp_male.animevents` (`nwbt cat`): the rapier only has an `audio_trigger` on draw |
| Mesh | `objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf` (`nwbt cat`) |
| Texts | `extract/loc/localization/pt-br` and `en-us` |

Mannequin tags of the weapon: `MannequinTag = 1H_Melee,Rapier` (offhand `Melee_OH`, which has no rapier variants).

### Where the windows are

1. **The damage of the basic attacks is not in the character `.adb`.** `player_rapier.adb` has no `CAGE-Damage` and
   no `WeaponEffects` on the light attacks and the heavy. They are in the weapon scope,
   `playermaleanims_1h_meleeattach.adb` (fragments with `ProcLayer` only, no `AnimLayer`). Abilities have damage in
   both scopes. The pipeline must read the weapon-scope `.adb` too (`window_attach_adbs`, see
   [../../tech/weapon-porting.md](../../tech/weapon-porting.md)).
2. **Timing after an empty clip.** Each `Blend.ExitTime` is relative to the start of the previous clip, empty
   (`<Procedural type="">`) or not. In the sequence `B(0) empty, B(0.4) Damage` the damage starts at 0.4 s, not 0 s.
   For the rapier this places the 5 Flurry hits at 0.400–1.212 (not 0.000–0.334) and the second Light2 hit at
   0.533 (not 0.400). It also affects 40 variants of `player_sword.adb` (on the sword in use, only `Dodge_Roll_Fast`:
   `ApplyRotation_PostDodge` 0.067 → 0.167). The numbers in this document use the correct sum.

## The weapon

`RapierT5` (rows T2–T5 have the same numbers; `TierNumber` 2 in all):

| Field | Rapier | Sword (SwordT5), for comparison |
|---|---|---|
| BaseDamage | 61 | 64 |
| CritChance | 0.09 | 0.07 |
| CritDamageMultiplier | 1.4 | 1.3 |
| BaseStaggerDamage | 28 | 36 |
| BlockStaminaDamage / BlockStability | 28 / 17 | 36 / 22 |
| Scaling | DEX 0.0085, INT 0.0065 | STR 0.0085, DEX 0.0065 |
| Block | `BLA*` = 1, `CanBlockRanged` = true (range 50% health, 50% stamina) | |
| Weight | 25 (`WeightOverride`) | |
| Tooltip | `1H_Rapier_PrimaryAttack` / `1H_Rapier_HeavyAttack` | |
| EquipType | `RSword_LeftHolster` (sheathed on the left hip), `FireJoint handattach` | |
| Target lock | profile `RapierGeneric`: starts up to 25 m, holds up to 31 m | |

Mesh: `objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf`, socket `right_hand_attach` (same as the
sword). The `.cdf` uses the skeleton `objects/weapons/weapon_skels/lh_weapon_skel.chr`, the skin
`wep_me1h_sw_rapiera_t5.skin` and the material `wep_me1h_sw_rapiera_mat_0.mtl`. Blade points (`CA_BONE` on
`master_root`, Z axis): `vfx_Base_01` 0 m, `vfx_Mid_01` 0.5 m, `vfx_Melee_01` 0.9 m, `vfx_Tip_01` 0.925 m. There are
also static `.cgf` files `jav_wep_me1h_sw_rapiera_t1/t2/t3.cgf` in the same folder, in case the `.cdf` is a problem.

## Basic mechanics

### Basic attack flow

Every basic attack starts as a charge (`attack_primary` → action `Attack_Start_Charge`, `CanAttack_Heavy` = input
`attack_primary_virtual`). There is no separate heavy: holding the same button becomes the heavy.

```
attack_primary ─► HeavyCharge (rapier_hvy_c1_stab, from the start)
                   ├─ released before 0.667 s ──► Light1
                   └─ held until 0.667 s ───────► HeavyCharged (same clip, from 74%)
Light1 ── input 0.333–0.533 (buffer) or ≥0.533 ─► HeavyCharge_C2 ─ released <0.667 ► Light2 / held ► HeavyCharged
Light2 ── input 0.300–0.733 (buffer) or ≥0.733 ─► HeavyCharge_C3 ─ released <0.594 ► Light3 / held ► HeavyCharged
Light3 ── input 0.231–0.867 (buffer) or ≥0.867 ─► HeavyCharge (starts over)
```

`Attack_Heavy_NotHeld` = `attack_primary` and `attack_primary_hold` released (`playeractions_attack`). The charge is
cancelled by `block` (`DeactivationCondition`).

Light combo: **3 swings, 4 hits** (the second swing hits twice, 66% each). The passive Light Edge talks about "both
middle attacks" for this reason.

`1H_Rapier_HeavyAttack` (133%, clips `rapier_spc_attack` / `rapier_1h_hvy_atk_01`) is only reachable through
`Attack_Start_Charge_C4..C6`, which no rapier fragment triggers. The real heavy is the charged one (166%). The item
tooltip still shows the 133% row.

Abilities interrupt attacks: almost every attack fragment has `CAGE-TransitionBuffered` 0–0.1 s and
`TransitionInstant` from 0.1 s for Tondo, Flourish, Riposte, Flurry and Fleche, and `TransitionInstant` from 0 s for
Evade.

### Basic actions

Times in seconds of the fragment (already with the clip `speed`). `w` = window in the weapon scope, `c` = character
scope. Stamina from the tables: all attacks cost 0.

| Action | Fragment (tags) | Clips | Damage (window, DamageID) | Trail | Can chain | Dodge cancels | Other |
|---|---|---|---|---|---|---|---|
| HeavyCharge | `Attack_Start_Charge` (1H_Melee+Rapier) | `rapier_hvy_c1_stab` | — | — | see flow | 0–end | grit 0–0.733 (condition `GRIT_On_Basic_Attacks`) |
| Light1 | `Attack_Primary` | `rapier_prim_c1` @1.4 | w 0.333–0.433 `1H_Rapier_PrimaryAttack` | 0.175–0.319 `rapier_primary_1` | CanTransition 0.767; AnimEnd 1.357 | 0–0.2 and 0.5–end | homing 0.067–0.433; block off 0.2–0.533 |
| Light2 | `Attack_Primary_Strike2` | `rapier_prim_c2` @1.3 | w 0.300–0.400 and 0.533–0.633 `1H_Rapier_PrimaryTwoThirdsAttack` (keys atk1/atk2) | 0.275–0.375; 0.533–0.575 | CanTransition 0.9 | 0–0.233 and 0.7–end | homing 0.067–0.4 |
| Light3 | `Attack_Primary_Strike3` | `rapier_prim_c3` @1.1 (starts at 8%) | w 0.231–0.467 `1H_Rapier_PrimaryAttack_Ender` | 0.200–0.306 | CanTransition 0.9 | 0–0.167 and 0.5–end | homing 0.067–0.4 |
| HeavyCharged | `Attack_Heavy_Charged` | `rapier_hvy_c1_stab` (starts at 74%) | w 0.200–0.500 `1H_Rapier_HeavyAttack_Charged` | 0.175–0.5 `thrust_1` | CanTransition 1.2; new charge 0.1–0.681 (buffer) | 0–0.033 and 0.5–end | grit 0–0.567; homing 0.131–0.5 |
| SprintAttack | `Attack_Sprint` → `Attack_Sprint_Land` | `rapier_sprint_downstrike` + `rswrd_sprint_downstrike_fall`; Land: `rapier_sprint_downstrike` (starts at 40%) | Land: w 0.167–0.333 `1H_Rapier_SprintAttack` | Land 0–0.333 `melee_1` | Land: CanTransition 1.167 | Land: 1.167–end | `CanLand_SprintAttack` from 0.4 s; CritWindow 0.033–0.167; stamina `Attack_1H_Rapier_Run` = 0 |
| Dodge_F/B/L/R | `Dodge_Roll` (+`BackNav`/`LeftNav`/`RightNav`) | `cmbt_1h_quickstep_f/b/l/r` + `rapier_pose_add` | — | — | CanTransition 0.5; new dodge 0.267 (buffer) | — | invulnerable 0–0.5; pays stamina at 0.1 s |
| DodgeSlow / DodgeFast | `Dodge_Roll_Slow` / `Dodge_Roll_Fast` | `cmbt_1h_quickstep_hvy_f` + pose / `unarmed_dodgeroll_f_light_to_crch` + `rswrd_dodgeroll_f_light_to_crch` | — | — | CanTransition 0.5 | — | invulnerable 0–0.5 |
| BlockStart | `Block_UpperBody_Raise` (1H_Melee) | `rswrd_block_start` @2 | — | — | — | — | `BlockActive` 0–end |
| BlockIdle | `Block_UpperBody` (1H_Melee) | `rswrd_run_block_ub_layer` | — | — | — | — | |
| BlockEnd | `Block_UpperBody_Lower` (1H_Melee+Rapier+Idle) | `rswrd_block_end` → `cmbt_idle_1h` + `rapier_pose_add` | — | — | — | — | |
| BlockReact / BlockBreak | `r_B1` / `r_B0_Break` (1H_Melee) | `rswrd_block_react_r1` / `rswrd_block_break` @2 | — | — | Break: CanTransition 0.5 | — | |
| Draw | `Unsheathe_UpperBody` | standing: `rapier_idle_draw_v1`; moving: `rapier_idle_draw_v1_ub` (`SetAnimByTag Idle`) | — | — | CanTransition_UpperBody 0.667 | 0.1–end | `unsheathe_weapon` 0.233 |
| Sheath | `Sheathe_UpperBody` (From_1H_Melee, `player.adb`) | standing: `raxe_idle_sheath`; moving: `raxe_run_sheath_ub` @1.2 (starts at 15%) | — | — | — | 0.1–end | `sheathe_weapon` 0.3; no rapier clip of its own |
| ExhaustedAttack | `Attack_Exhausted` (1H_Melee) | `rswrd_exhausted_atk` | w 0.533–0.733 `1H_Attack_Exhausted` (0.5) | — | CanTransition 1.467 | 1.467–end | attack with zero stamina |
| Hit_F/B/L/R | `r_R1_*` (1H_Melee+Rapier) | `cmbt_idle_1h_r1_*` + `rapier_pose_add` | — | — | — | — | R2 F/L/R generic 1H (`unarmed_r2_*` + `rswrd_r2_*`); R2 B from the rapier |
| Stun | `r_Stun` → `r_Stun_Loop` → `r_Stun_End` | `cmbt_idle_r_stun_1h_start/loop/end` | — | — | — | — | |

**Dodge.** The `Dodge` action picks the fragment by equip load (`ActionConditionEquipLoadCategory`): light →
`Dodge_Roll_Fast` (generic 1H roll, `Dodge_Roll_Fast` = 45), medium → `Dodge_Roll` (rapier short step, `Dodge_Roll` =
45), heavy → `Dodge_Roll_Slow` (heavy step, 50). Displacement multiplier: Roll 1.28 (lateral 1.1), Fast 1.15, Slow
1.43. Stamina regeneration resumes 1.0 s later. Invulnerable 0–0.5 s in all three.

**Block.** The rapier blocks with the weapon itself (`ActivateBlock ActivateMainHandWeapon`), using the generic 1H
sword clips (no rapier version). Raising costs nothing; during the block regeneration is off and resumes 1.0 s
later. Each blocked hit removes stamina by the attacker row's `BlockStaminaDmgMod` and the rapier's
`BlockStability` 17. The `Block` = 10 row the port uses comes from `javelindata_staminacosts_player`.

**Sprint.** `Sprint` = 0 stamina. No clip of its own: `unarmed_sprint_veerslope_v2_blend` +
`rswrd_sprint_veerslope_blend`.

## Active abilities

Trees: **Blood** (`TreeId 0`, text `ui_blood`) and **Grace** (`TreeId 1`, text `ui_evade`). Mastery:
`ui_rapier_mastery` (Rapier Mastery). No ability costs stamina. The cooldown starts when the `Ability_Startup`
marker ends (action `SetCooldownTimer` in "Activate CoolDown after startup"); without that marker it starts on
entry.

| Ability | AbilityID | Tree (row/col) | Cooldown | DamageID | Fragments |
|---|---|---|---:|---|---|
| Tondo | `Ability_Rapier_Slash` | Blood 1/4 | 6 s | `1H_Rapier_Slash` + bleed `1H_Rapier_Slash_Bleed` | `Ability_Rapier_Slash` |
| Flurry | `Ability_Rapier_Flurry` | Blood 0/0 | 16 s | `1H_Rapier_Flurry_Strike_1..5` (`_5_Mod4` with Finalize) | `Ability_Rapier_Flurry` |
| Flourish and Finish | `Ability_Rapier_Flourish` | Blood 1/2 | 20 s | `1H_Rapier_Flourish`, `1H_Rapier_Finish` (`_Consume_Ult` with Bloody End) | `Ability_Rapier_Flourish` → `Ability_Rapier_Finish` / `_Finish_Ultimate` |
| Riposte | `Ability_Rapier_Riposte` | Grace 1/4 | 18 s | `1H_Rapier_Riposte_Stun` (`_Enhanced` with Mod3); taunt `1H_Rapier_Riposte_Taunt` | `Ability_Rapier_Riposte_Loop` → `_Riposte_Out` or `_Riposte_Kickout` |
| Fleche | `Ability_Rapier_Aerial_Lunge_Intro` | Grace 0/0 | 20 s | `1H_Rapier_Aerial_Lunge`; follow-up `1H_Rapier_Wake` | `_Aerial_Lunge_Intro` → `_Loop` → `_Stop` (or `_Stop_Instant_Wake` → `Ability_Rapier_Wake`) |
| Evade | `Ability_Rapier_Evasion` | Grace 1/2 | 12 s | attack: `1H_Rapier_PrimaryAttack` | `Ability_Rapier_Evasion` (+`ForwardNav`/`BackNav`/`LeftNav`/`RightNav`) → `Ability_Rapier_Evade_Attack` |

### Tondo

Tooltip: a slash that extends the blade's reach up to 5 m, for 50% of weapon damage, applying a bleed of 10% of
weapon damage per second for 10 s (stacks up to 3 times and refreshes earlier stacks).

- Clip `rapier_ability_wideslash` @2.2. `Ability_Startup` 0–0.3. Homing 0.133–0.5 (radius 2.75 m, 45°, no
  approach).
- Damage 0.333–0.500, single key `atk1` (1 hit per target), three shapes combined: box 2 × 4.7 × 2.4 m centered 3 m
  in front of the root (`Xform`, `UseEndAsCenter`, follows the camera pitch); box 4.5 × 1.6 × 2.4 m centered 4.5 m
  ahead; weapon capsule radius 0.5 m (0.333–0.433). Useful reach: from ~0.65 m to 5.35 m, 2 m wide near and 4.5 m at
  the end.
- `1H_Rapier_Slash`: 0.5, Slash, `StatusEffect Rapier_Bleed_DOT` only if it deals damage. The ability switches the
  bleed's damage row to `1H_Rapier_Slash_Bleed` (coef 1.0).
- Block off 0–0.856; AnimEnd 0.856; CanTransition 0.9; basic attack buffered 0.1–0.856.

### Flurry

Tooltip: a poised stance and 5 fast piercing attacks, each for 60% of weapon damage. It can be cancelled at any time
by dodging or using another ability. It has grit.

- Clip `rapier_ability_flurry` @1.75. Grit (`ActivateGrit`) 0–1.367. `Ability_Startup` 0–0.366.
- Hits (weapon scope, capsule radius 0.6 m at 0.5 m from the blade base, each with its own key): 0.400–0.475
  `Strike_1`; 0.625–0.675 `Strike_2`; 0.762–0.800 `Strike_3`; 0.900–0.963 `Strike_4`; 1.104–1.212 `Strike_5` (or
  `Strike_5_Mod4` with Finalize). All 0.6 Thrust; the 5th pushes 1.25.
- Homing radius 4 m in three spans (0.062–0.733, 0.733–1.067, 1.067–1.367), approaches up to 1 m from the target.
- Dodge cancels from 0.175; Evade at any time; CanTransition 1.325; AnimEnd 2.563.

### Flourish and Finish

Tooltip: Flourish attacks for 60% of weapon damage and staggers and pushes enemies. Reactivating the ability or a
basic attack at the end of Flourish continues into Finish. Finish lunges forward for 120% of weapon damage and
consumes all rapier bleeds on every target hit, dealing 110% of their damage at once.

- Flourish: `rapier_ability_flourish` @1.4. Damage 0.325–0.400 weapon capsule radius 0.8 m and 0.338–0.362 capsule
  radius 1.5 m centered 2 m ahead (`xform`), same key. `1H_Rapier_Flourish`: 0.6 Slash, PowerLevel 2, knockback 3,
  HitStun 25. Homing 0.188–0.4 (radius 3 m, 60°). Advance `Attack_Translation` ×1.3. AnimEnd 0.844.
- Continuation: condition `CanAttack_Finish` = `attack_primary` with the ability active; buffer 0.1–0.65, immediate
  from 0.65. With all four upgrades (Mod1–Mod4) it goes to `Ability_Rapier_Finish_Ultimate`. The actionlist only has
  the basic attack input; reactivation by the ability key has no condition of its own.
- Finish: `rapier_ability_flourish_lunge` @1.8, advance ×2.6 (×2.9 in `Attack_Translation`). Homing 0.175–0.5
  (radius 7 m, 30°, approaches). Damage 0.267–0.433, weapon capsule radius 0.6 m, half-height 0.35.
  `1H_Rapier_Finish` 1.2 Thrust. CanTransition 0.967; AnimEnd 1.148. No cooldown of its own.
- Consumption (row `Ability_Rapier_Flourish` of the tree): `OnHit`, `RequireReaction`,
  `DamageTableRow 1H_Rapier_Finish`, `TargetStatusEffect Rapier_Bleed_DOT`,
  `RemoveTargetStatusEffectsList Rapier_Bleed_DOT`, `ConsumeTargetStatusEffectMult 1.1`, `ThreatDamage -0.9`. With
  Bloody End: `1H_Rapier_Finish_Consume_Ult` and 1.5.
- Grit in Flourish and Finish only with With Flair (condition `Flourish_Finish_Grit`).

### Riposte

Tooltip: a defensive stance for 1 s. If hit during it, counterattacks the attacker for 50% of weapon damage,
staggering and stunning for 2 s. Brief invulnerability after a successful riposte. With a Carnelian gem: taunts
enemies within 5 m for 4 s.

- Entry: the action goes straight to the fragment `Ability_Rapier_Riposte_Loop` (`rapier_ability_riposte_start` →
  `rapier_ability_riposte_loop`). Applies `Rapier_Riposte_Timer` (1 s; removed on exit). Movement ×0.001 (standing).
  Block off. `Spell_Taunt_AOE` 0–0.3 only with `HasTauntGem`.
- `Rapier_Riposte_Timer`: absorption 200% on all types (takes no damage), 100% resistance to afflictions, stun
  immune (`Stun=-1.0`), grit, `IgnoreDebuffs`. The reaction actions (`playeractions_reaction`) exit when the target
  has this effect: no hit reaction during the stance.
- Trigger `Riposte_Attack`: `IfDamagedByPowerLevel >= 0` with `FailOnNoReactionDamage=true`, tag `Rapier` and the
  timer active → action `Ability_Rapier_Riposte_Out` (started by the server).
- No hit: at 1.0 s the Loop switches to `Ability_Rapier_Riposte_Kickout` (`rapier_ability_riposte_cancel`,
  CanTransition 0.6).
- Counterattack (`Riposte_Out`): `rapier_ability_riposte_end` @1.6. Invulnerable 0–0.567. Homing 0–0.567 (radius
  2.75 m, approaches up to 1 m). Damage 0.433–0.567: capsule radius 0.5 m, half-height 0.4 m, 0.7 m ahead and 1 m up
  (`Xform`). `1H_Rapier_Riposte_Stun`: 0.5 Strike, PowerLevel 3, stagger 2.4, applies `Rapier_Riposte_Stun` (2 s,
  breaks on the first damage taken). With Lasting Consequence: `_Enhanced` (2.5 s). Camera shake `Large_short` at
  0.433. Applies `Rapier_Riposte_Uninterruptible_Riposte` (0.875 s). Attacks and abilities buffered 0.5–0.9;
  CanTransition 0.933.

### Fleche

Tooltip: leaves the ground and lunges 10 m in a piercing move through enemies, for 145% of weapon damage.

- Intro: `rapier_ability_aeriallunge_start` @2.8. Aligns to the camera 0–0.267. `Ability_Startup` 0–0.333. Goes to
  the Loop at 0.381. Animation movement ×1.3 in `Attack_Translation`, inherits up to 6 m/s.
- Loop: `rapier_ability_aeriallunge_loop` @1.5, movement ×0.8, gravity ×3. Damage from 0 s, capsule radius 1.1 m,
  half-height 0.25, offset (0; 0.5; 0.5) m, `1H_Rapier_Aerial_Lunge` 1.45 Thrust (`WeaponCategory Sword_Fleche`).
  Exits to `Stop` at 0.333 s. The keys `atk16..atk60` start at 0.502 s, after the exit: in practice 1 hit per target.
- In Intro, Loop and Stop: no actor-actor and no player-player collision (passes through).
- Stop: `rapier_ability_aeriallunge_end` @3, forced movement (`CharacterRelativeMovement` Speed Y 5, Decel 50).
  Applies `Rapier_Fleche_Backstab` (5 s). CanTransition 0.5.
- Continuation (Interruption): `CanAttack_Wake` = `attack_primary` + Mod1, Mod2 and Mod3 active. In the Loop it goes
  to `Stop_Instant_Wake` and from there to `Ability_Rapier_Wake` at 0.133 s. `Wake`:
  `rapier_ability_aeriallunge_stab` @1.12, damage 0.167–0.333 `1H_Rapier_Wake` (1.15), CanTransition 0.533.
- The 10 m come from the clips' root motion with these multipliers. Not measured (needs the converted clip).

### Evade

Tooltip: a small side step in the current movement direction that cancels any current activity and gives brief
invulnerability. Light attacks during Evade are exceptionally fast.

- Direction: navigation tag by the conditions `Evade_Back` (180° ± 50), `Evade_Left` (270° ± 45), `Evade_Right`
  (90° ± 45); otherwise `ForwardNav`. The sides and back only apply with camera lock, `AttackFacing` or
  `ForceCameraFacing`; without them the step goes forward, in the input direction.
- `rapier_ability_evade_f/b/l/r` @1.5. Invulnerable 0–0.444. Movement: forward ×1.5, back ×2.3, sides ×1.7. No
  stamina cost. Resets grit. Removes `Rapier_Riposte_Timer`.
- Light attack: `attack_primary` buffered 0–0.067 and immediate from 0.067 → `Ability_Rapier_Evade_Attack`. Heavy
  (`CanAttack_Heavy`) from 0.3 → charge. CanTransition 0.333. Dodge cancels from 0.2.
- Evade attack: `rapier_prim_c1` @1.4 starting at 20% of the clip. Damage 0.067–0.200 `1H_Rapier_PrimaryAttack`.
  Homing radius 3.25 m, moves up to 1.25 m. Chains into charge C2 (→ Light2): buffer 0.067–0.267, immediate from
  0.267.
- Evade can be used from almost every attack and ability fragment (`CanAttack_Evasion` immediate from 0 s).

## Passives, upgrades and ultimates

`kind`: `passive` = upgrade or passive; `ultimate` = row 5 of the tree.

| Tree | Row/Col | AbilityID | Name | Kind | Effect | Key data |
|---|---|---|---|---|---|---|
| Grace | 1/0 | `Rapier_Aerial_Lunge_Mod1` | Quick Lunge | passive | A kill with this ability reduces its cooldown by 80%. | OnKill=True; RequireReaction=True; DamageTableRow=1H_Rapier_Aerial_Lunge; CooldownTimer=-0.8; CDRImmediatelyOptions=AbilitySpecific |
| Grace | 2/0 | `Rapier_Aerial_Lunge_Mod2` | Backside | passive | After Fleche, the next critical hit within 5 s deals 15% more base damage. | OnCrit=True; AttackType=Light,Heavy,Ability; CritDamage=0.15; StatusEffect=Rapier_Fleche_Backstab |
| Grace | 3/0 | `Rapier_Aerial_Lunge_Mod3` | Interruption | passive | Reactivating the ability or pressing basic attack during Fleche stops the lunge and performs a standing follow-up attack for 115% damage. | OnKill=True; RequireReaction=True; DamageTableRow=1H_Rapier_Wake; CooldownTimer=-0.8; CDRImmediatelyOptions=AbilitySpecific |
| Grace | 0/1 | `Rapier_BonusDmgLowStamina` | Desperation | passive | +10% base damage while stamina is below 40%. | OnHit=True; BaseDamage=0.1; MyStaminaPercent=40 |
| Grace | 2/1 | `Rapier_BonusCDOnCrit` | Red Curtains | passive | Critical hits reduce all cooldowns by 5% (cooldown 0.26). | OnCrit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; CooldownTimer=-0.05; CDRImmediatelyOptions=ActiveWeapon; ActivationCooldown=0.26 |
| Grace | 4/1 | `Rapier_Evasion_Mod4` | Breathe In | passive | +20 stamina after successfully dodging an enemy attack. | OnHitTakenWhileInvulnerable=True; RequireReaction=True; SelfApplyStatusEffect=Rapier_Evasion_Mod1 |
| Grace | 2/2 | `Rapier_Evasion_Mod1` | Crescendo | passive | Each successful light attack reduces this ability's cooldown by 25% (cooldown 0.1 s). | OnHit=True; RequireReaction=True; AttackType=Light; CooldownTimer=-0.25; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.1 |
| Grace | 3/2 | `Rapier_Evasion_Mod2` | Allegro | passive | 12% Haste for 3 s after successfully dodging an enemy attack. | OnHitTakenWhileInvulnerable=True; RequireReaction=True; SelfApplyStatusEffect=Rapier_Evasion_Mod2 |
| Grace | 4/2 | `Rapier_Evasion_Mod3` | Adagio | passive | A forward evade gives +10% base damage to the next light attack. Ends on hit or after 1 s. | OnHit=True; AttackType=Light; BaseDamage=0.1; StatusEffect=Rapier_Evade_Forward_Damage |
| Grace | 5/2 | `Rapier_BonusDMGAfterAbility` | Momentum | ultimate | +25% base damage on the next light or heavy attack after an ability. Ends on hit or after 3 s. | OnHit=True; AttackType=Light,Heavy; BaseDamage=0.25; StatusEffect=Rapier_BonusDMGAfterAbility |
| Grace | 0/3 | `Rapier_BonusStamRegenOnKill` | Controlled Breathing | passive | +3 stamina on any hit. | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; SelfApplyStatusEffect=Rapier_BonusStamOnHit |
| Grace | 1/3 | `Rapier_BonusDMGMaxHP` | Perfectionist | passive | +10% base damage at full health. | OnHit=True; BaseDamage=0.1; MyHealthPercent=99 |
| Grace | 3/3 | `Rapier_BonusSpeedOnKill` | Swiftness | passive | 3% Haste for 4 s on rapier hits. Stacks up to 5 times. | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; SelfApplyStatusEffect=Rapier_BonusSpeedOnKill |
| Grace | 2/4 | `Rapier_Riposte_Mod1` | Insult to Injury | passive | A successful riposte hit empowers you: +10% damage for 3 s. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Riposte_Stun,1H_Rapier_Riposte_Stun_Enhanced; SelfApplyStatusEffect=Rapier_Riposte_Empower |
| Grace | 3/4 | `Rapier_Riposte_Mod2` | Priority | passive | A riposte counter hit reduces the other rapier cooldowns by 20%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Riposte_Stun,1H_Rapier_Riposte_Stun_Enhanced; CooldownTimer=-0.2; CDRImmediatelyOptions=AllExcept |
| Grace | 4/4 | `Rapier_Riposte_Mod3` | Lasting Consequence | passive | Riposte stun becomes 2.5 s. | OnHit=True |
| Blood | 1/0 | `Rapier_Flurry_All_In` | Overwhelm | passive | Each Flurry hit deals 25% more damage to block. | OnBlockedHit=True; BlockDamage=0.25 |
| Blood | 2/0 | `Rapier_Flurry_Haste` | Fleeting Strikes | passive | Each Flurry hit reduces its cooldown by 5%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Flurry_Strike_1,1H_Rapier_Flurry_Strike_2,1H_Rapier_Flurry_Strike_3,1H_Rapier_Flurry_Strike_4,1H_Rapier_Flurry_Strike_5,1H_Rapier_Flurry_Strike_5_Mod4; CooldownTimer=-0.05; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.1 |
| Blood | 3/0 | `Rapier_Flurry_Weakness` | To the Bone | passive | Each Flurry hit extends the rapier bleed by 1 s (future stacks of the extended bleed also get its longest duration). | OnHit=True; TargetStatusEffectDurationList=Rapier_Bleed_DOT; TargetStatusEffectDurationMod=1 |
| Blood | 4/0 | `Rapier_Flurry_Mod4` | Finalize | passive | The last Flurry hit applies a Bleed stack: 10% of weapon damage per second for 10 s. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Flurry_Strike_5_Mod4; OtherApplyStatusEffect=Rapier_Bleed_DOT |
| Blood | 0/1 | `PassiveRapier02` | Refreshing Strikes | passive | All rapier cooldowns −1% on any hit (max 5 reductions per attack). | OnHit=True; OnBlockedHit=True; RequireReaction=True; AttackType=Light,Heavy,Ability; CooldownTimer=-0.01; CDRImmediatelyOptions=ActiveWeapon; NumberOfTrackedHits=5 |
| Blood | 1/1 | `PassiveRapier04` | Unerring | passive | +5% base damage against targets with Rapier Bleed. | OnHit=True; TargetStatusEffect=Rapier_Bleed_DOT; BaseDamage=0.05 |
| Blood | 3/1 | `PassiveRapier03` | Heavy Puncture | passive | A heavy attack on a bleeding enemy extends the rapier bleeds by 2 s (future stacks also get the longest duration). | OnHit=True; AttackType=Heavy; TargetStatusEffectDurationList=Rapier_Bleed_DOT; TargetStatusEffectDurationMod=2 |
| Blood | 2/2 | `Rapier_Flourish_Mod1` | With Flair | passive | Grit for Flourish and Finish (grit prevents stagger from incoming attacks). | |
| Blood | 3/2 | `Rapier_Flourish_Mod2` | Swagger | passive | +20 stamina on a successful Finish hit. | OnHit=True; DamageTableRow=1H_Rapier_Finish,1H_Rapier_Finish_Consume_Ult; SelfApplyStatusEffect=Rapier_Finish_Stam_Mod2 |
| Blood | 4/2 | `Rapier_Flourish_Mod3` | Fuel | passive | When Tondo's Bleed deals damage, this ability's cooldown is reduced by 9% (max 1 reduction per activation). | OnHit=True; RemoteDamageTableRow=1H_Rapier_Slash_Bleed; CooldownTimer=-0.09; CDRImmediatelyOptions=AbilitySpecific; ActivationCooldown=0.5 |
| Blood | 5/2 | `Rapier_Flourish_Mod4` | Bloody End | ultimate | Finish deals 150% of the rapier bleed damage instead of 110%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Finish_Consume_Ult; TargetStatusEffect=Rapier_Bleed_DOT; ConsumeTargetStatusEffectMult=1.5 |
| Blood | 0/3 | `PassiveRapier05` | En Garde | passive | +10% base damage when the target is above 50% health. | OnHit=True; BaseDamage=0.1; TargetHealthPercent=50 |
| Blood | 2/3 | `PassiveRapier01` | Light Edge | passive | +8% base damage on both middle attacks of the light chain. | OnHit=True; DamageTableRow=1H_Rapier_PrimaryTwoThirdsAttack; BaseDamage=0.08 |
| Blood | 2/4 | `Rapier_Slash_Mod1` | Thirst For Blood | passive | Applying the first bleed stack to an opponent reduces Tondo's cooldown by 10%. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Slash; TargetStatusEffect=Rapier_Bleed_DOT; CooldownTimer=-0.1; CDRImmediatelyOptions=AbilitySpecific |
| Blood | 3/4 | `Rapier_Slash_Mod2` | And Again | passive | Hitting only 1 target, even if blocked, reduces Tondo's cooldown by 25%. | RequireReaction=True; DamageTableRow=1H_Rapier_Slash,1H_Rapier_Slash_Bleed; CooldownTimer=-0.25; CDRImmediatelyOptions=AbilitySpecific; NumberOfTrackedHits=1; Duration=2; AfterAction=Ability_Rapier_Slash |
| Blood | 4/4 | `Rapier_Slash_Mod3` | Proper Spacing | passive | +100% base damage on Tondo's initial hit when the target already has Rapier Bleed. | OnHit=True; RequireReaction=True; DamageTableRow=1H_Rapier_Slash; TargetStatusEffect=Rapier_Bleed_DOT; BaseDamage=1 |

Notes on the tree:

- `Rapier_Evasion_Mod1` shows the text of Crescendo (`@Rapier_06_Mod4`) and `Rapier_Evasion_Mod4` shows Breathe In
  (`@Rapier_06_Mod1`). The data matches the text shown, not the ID number.
- `Rapier_Aerial_Lunge_Mod3` (Interruption) has the fields `OnKill`/`CooldownTimer -0.8` copied from Mod1; the real
  rule is in the actionlist (`CanAttack_Wake`).
- Refreshing Strikes and Red Curtains count hits per action: every rapier attack action does `ClearAbilityHits` of
  both on exit (max 5 reductions per attack for the first).

## Status effects

| StatusID | Name | Duration | Rule |
|---|---|---|---|
| `Rapier_Bleed_DOT` | Bleed | 10 s (max 18) | Tick 1.01 s; each tick deals 10% of the attacker's weapon damage (`HealthModifierDamageBased -0.1`) × coef of the override row (Tondo: `1H_Rapier_Slash_Bleed` 1.0; Flurry with Finalize: `Common_DamageOverTime_Override` 1.0, no reaction). Up to 3 stacks; a new application refreshes the duration of earlier stacks (`RefreshDuration`, `InheritTotalDuration`); at least 0.5 s between applications. Extensions: +1 s per Flurry hit (To the Bone), +2 s per heavy (Heavy Puncture), up to 18 s. Removed on death. |
| `Rapier_Riposte_Timer` | (timer) | 1 s | Absorption 200% (no damage), 100% against afflictions, stun immune, grit, `IgnoreDebuffs`. |
| `Rapier_Riposte_Stun` | Riposte | 2 s | `Stunned`; ends on the first damage taken (`StopOnHitCount 1`, `OnHitTaken`). |
| `Rapier_Riposte_Stun_Enhanced` | Riposte | 2.5 s | Same, with Lasting Consequence. |
| `Rapier_Riposte_Uninterruptible_Riposte` | (grit) | 0.875 s | Grit and stun immune during the counterattack. |
| `Rapier_Riposte_Uninterruptible` | (grit) | 3 s | Not applied by the rapier actions; only referenced in `playeractions_reaction`. |
| `Rapier_Riposte_Empower` | Riposte Empowered | 3 s | +10% damage on all types (Insult to Injury). |
| `Rapier_Evasion_Mod1` | Evasion | instant | +20 stamina (Breathe In). |
| `Rapier_Evasion_Mod2` | Evasion | 3 s | +12% speed (Allegro). |
| `Rapier_Finish_Stam_Mod2` | Evasion | instant | +20 stamina (Swagger). |
| `Rapier_Evade_Forward_Damage` | Empowered | 1 s | Marker for the +10% on the next light (Adagio). |
| `Rapier_Fleche_Backstab` | — | 5 s | Marker for the +15% critical damage (Backside). |
| `Rapier_BonusDMGAfterAbility` | — | 3 s | Marker for the +25% on the next light/heavy (Momentum). Removed on sheathing (action `Rapier_Momentum_Active`). |
| `Rapier_BonusStamOnHit` | — | instant | +3 stamina (Controlled Breathing). |
| `Rapier_BonusSpeedOnKill` | Haste | 4 s | +3% speed per stack, up to 5, at least 0.1 s between applications (Swiftness). |

## Damage rows

`WeaponCategory` Sword on all rows (Fleche: `Sword_Fleche`). All have `CanCrit` (except the taunt). An empty
`PowerLevel` in the table = 0. PowerLevel is a reaction category ([../combat.md](../combat.md#5-power-level-and-stagger)).

| DamageID | Type | Coef | Head | PowerLevel (block) | HitStun / Block / Crit | Knockback Y | Stagger | Block stamina | Extra |
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

`Common_DamageOverTime_Override` (Flurry bleed with Finalize): coef 1.0, Standard, `NoReaction`, `AttackType DOT`.
`1H_Attack_Exhausted` (attack with zero stamina): 0.5, Standard, PowerLevel 2.

Camera shakes referenced: `Small` (offset 0.01, 0.1 s) on normal hits; `Large_short` (0.02, 0.1 s) on criticals and
on the Riposte counterattack (`javelindata_camerashake`).

## Locomotion and loops

| Item | Fragment | Clips |
|---|---|---|
| Idle | `Idle` (1H_Melee+Rapier) | `cmbt_idle_1h` + `rapier_pose_add` (additive, loop) |
| Exhausted idle | `Idle` (+Exhausted) | `Unarmed_Exhaust_Idle` + `rswrd_exhaust_idle` |
| Strafe | `Strafe` (1H_Melee+Rapier) | `melee1h_cmbt_nav_strafe_blend` (starts at 50%) + `v2_rapier_cmbt_walk_layer_l` + `v2_rapier_cmbt_walk_layer_r` (weight 0.5) |
| Sprint | `Sprint` (1H_Melee) | `unarmed_sprint_veerslope_v2_blend` + `rswrd_sprint_veerslope_blend` (no rapier version) |
| Sprint start | `Sprint_Start` (1H_Melee) | `unarmed_sprint_start_strafe_turn_v2_blend` + `rswrd_sprint_start_strafe_turn` |
| Sprint stop | `Nav_Stop` (+Stand+Sprint) | `rswrd_sprint_stop_blend` + `rapier_sprint_stop_blend` |
| Walk/run stop | `Nav_Stop` (+Stand+Walk/Run) | `cmbt_walk_stop_1h_f` + `rapier_pose_add` |
| Block standing | `Block_UpperBody` (1H_Melee) | `rswrd_run_block_ub_layer`; the port's sword and hatchet use `rswrd_block_idle` |
| Block walking | — | `rswrd_walk_block` (same as the sword) |
| Fall | `Fall` (1H_Melee) | `unarmed_fall_flail_loop` + `rswrd_fall_flail_loop` |
| Land | `Land` (+Normal/Soft / +Hard) | `rapier_fall_softland_to_idle` / `rapier_fall_hardland_to_idle` |
| Stunned | `r_Stun_Loop` (1H_Melee) | `cmbt_idle_r_stun_1h_loop` |

`rapier_pose_add` is an additive pose (arm position with the rapier) applied over the generic 1H clips (idle, dodge
step, reactions, stops). Built as a normal layer, it replaces the pose instead of adding to it.

## Critical, backstab and specific rules

- Critical: `CritChance` 9% and `CritDamageMultiplier` 1.4 on the weapon (sword: 7% and 1.3). All attack rows have
  `CanCrit`. `CritWindow` appears in `Attack_Sprint_Land` (0.033–0.167); the effect of that marker is not in the
  data read.
- Back: no rapier row sets `NoBackstab`. `Rapier_Fleche_Backstab` is not a backstab bonus: it marks the 5 s in which
  the next critical gets +15% (Backside). The weapon perk "Fleche Ladino" (pt-BR name; +20% from behind with Fleche)
  is an item perk, outside the tree.
- Head: `DmgCoefHead` exists in the rows (1.2 on the light); the data read has no headshot rule for melee.
- Grit on basic attacks: `ActivateGrit` with condition `GRIT_On_Basic_Attacks` (defined outside the actionlists
  read; probably a perk or gem).
- Every ability use removes `PotionHaste` (haste potion) on entry.
- Taunt: the heavy (`1H_Rapier_HeavyAttack*`) has `IsTaunt` 0.5 s (affects AI only).

## Unused (legacy in the files)

- Fragments: `Attack_Heavy`, `Attack_Heavy_C2/C3`, `Attack_Start_Charge_C4..C6`, `Attack_Special`,
  `_Special_Charge`, `_Special_Charge_Exit`, `Attack_Special3` (old system with `SpecialAttack` mana),
  `Ability_Rapier_Aerial_Lunge` (uses a Life Staff clip), `Attack_Primary_Strike4..6` (weapon scope only).
- Damage rows: `1H_Rapier_SpecialAttack*`, `_Disarm`, `_Rupture`, `_Add_Bleed_Only`, `_Slash_Damage_Mod`,
  `_HeavyAttack` (no path).
- Cooldown `Ability_Rapier_Lunge` (16 s); conditions `Lunge_Mod*` and `Evasion02_Mod*`; spells
  `Rapier_Evasion_Spell*` and `Rapier_Evasion_AllVision_Mod3` (`javelindata_spelltable_rapier`, old Evasion aura).

## Unknowns

- Real distance of the advances (Fleche "10 m", Finish, Evade): depends on the clips' root motion, not measured.
- Clip durations (end of fragments without `AnimEnd`): come from the converted `.caf`.
- Meaning of `CritWindow`.
- How "consuming" the bleed counts the damage (remaining ticks × 1.1 is the most likely reading of the text; the field
  is only `ConsumeTargetStatusEffectMult`).
- `GRIT_On_Basic_Attacks` and `HasTauntGem`: global conditions not read.

## Proposal for the port

### Montages (prefix `AM_Rp_`)

Format of `tools/sets/sword_shield.json` (weapon part in `tools/sets/player/armas.json`). On the rapier both `.adb`
layers use the same clip, so each section has one clip (apart from the additive poses). Block and sheathe clips are
the Hatchet's: reuse `AM_Hx_Block*` and `AM_Hx_Sheath`.

| Montage | Sections (clips) | Fragments (windows) | Tags |
|---|---|---|---|
| `AM_Rp_LightCombo` | `rapier_prim_c1`, `rapier_prim_c2`, `rapier_prim_c3` | `Attack_Primary`, `Attack_Primary_Strike2`, `Attack_Primary_Strike3` | 1H_Melee,Rapier |
| `AM_Rp_HeavyCharge` (hold) | `rapier_hvy_c1_stab` (up to 0.667 s) | `Attack_Start_Charge` | 1H_Melee,Rapier |
| `AM_Rp_HeavyCharged` | `rapier_hvy_c1_stab` from 74% | `Attack_Heavy_Charged` | 1H_Melee,Rapier |
| `AM_Rp_SprintAttack` | `rapier_sprint_downstrike` | `Attack_Sprint_Land` (the Land starts at 40% of the clip: add the offset) | 1H_Melee,Rapier |
| `AM_Rp_Dodge_F/B/L/R` | `cmbt_1h_quickstep_f/b/l/r` (+ additive `rapier_pose_add`) | `Dodge_Roll` | +ForwardNav/BackNav/LeftNav/RightNav |
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
| `AM_Rp_EvadeAttack` | `rapier_prim_c1` from 20% | `Ability_Rapier_Evade_Attack` | 1H_Melee,Rapier |
| reuse | `AM_Hx_BlockStart/End/Idle/React/Break`, `AM_Hx_Sheath`, `AM_Hit_*`, `AM_StunLoop`, `AM_Death` | | |

Blend spaces and loops: `BS_Rp_Strafe` = `melee1h_cmbt_nav_strafe_blend` (+ `v2_rapier_cmbt_walk_layer_l/r` when
there is an additive layer); `BS_Rp_Sprint` = same as `BS_Hatchet_Sprint`; `Rp_Idle` = `cmbt_idle_1h`
(+ `rapier_pose_add`). Weapon:
`"rapier": {"file": "objects/weapons/melee/swords/1h/rapiera/wep_me1h_sw_rapiera_t5.cdf", "socket": "right_hand_attach"}`.

### Actions (`DT_RapierActions`, columns of `make_datatables.py`)

Stamina from the tables: attacks and abilities 0; dodge `Dodge_Roll` 45. Shapes in cm; "Blade" = blade sweep (base 0
→ tip 92.5 cm; the game uses a capsule radius 60 cm at 50 cm from the base on the lights and 40 cm on the heavy).

| Action | Montage | DamageID | Stamina | ComboNext | Shape | Cooldown | Effect |
|---|---|---|---:|---|---|---:|---|
| HeavyCharge | AM_Rp_HeavyCharge | — | 0 | Light1 (released) / HeavyCharged (0.667 s) | — | 0 | |
| Light1 | AM_Rp_LightCombo_1 | 1H_Rapier_PrimaryAttack | 0 | Light2 (via charge C2) | Blade | 0 | |
| Light2 | AM_Rp_LightCombo_2 | 1H_Rapier_PrimaryTwoThirdsAttack ×2 | 0 | Light3 (via charge C3, 0.594 s) | Blade, 2 windows | 0 | |
| Light3 | AM_Rp_LightCombo_3 | 1H_Rapier_PrimaryAttack_Ender | 0 | Light1 | Blade | 0 | |
| HeavyCharged | AM_Rp_HeavyCharged | 1H_Rapier_HeavyAttack_Charged | 0 | — | Blade | 0 | |
| SprintAttack | AM_Rp_SprintAttack | 1H_Rapier_SprintAttack | 0 | — | Blade | 0 | |
| Dodge_F/B/L/R | AM_Rp_Dodge_* | — | 45 | — | — | 0 | invulnerable 0–0.5 |
| Block* / Sheath | AM_Hx_* | — | (Block 10) | — | — | 0 | |
| Draw | AM_Rp_Draw | — | 0 | — | — | 0 | |
| Tondo | AM_Rp_Tondo | 1H_Rapier_Slash | 0 | — | Box 535 × 200→450 (or Front 535/60) | 6 | bleed |
| Flurry | AM_Rp_Flurry | 1H_Rapier_Flurry_Strike_1..5 | 0 | — | Blade, 5 windows | 16 | grit |
| Flourish | AM_Rp_Flourish | 1H_Rapier_Flourish | 0 | Finish (input 0.1–0.65 buffer, ≥0.65) | Front 350/90 | 20 | knockback 3 |
| Finish | AM_Rp_Finish | 1H_Rapier_Finish | 0 | — | Blade | 0 | consumes bleed ×1.1 |
| Riposte | AM_Rp_Riposte | — | 0 | RiposteOut (if hit) / RiposteCancel (1 s) | — | 18 | no damage for 1 s |
| RiposteOut | AM_Rp_RiposteOut | 1H_Rapier_Riposte_Stun | 0 | — | Front 120/90 (at the attacker) | 0 | TargetStun 2.0; invulnerable 0–0.567 |
| Fleche | AM_Rp_Fleche | 1H_Rapier_Aerial_Lunge | 0 | Wake (with passives) | Around 110 following the body | 20 | passes through |
| Wake | AM_Rp_Wake | 1H_Rapier_Wake | 0 | — | Blade | 0 | |
| Evade_F/B/L/R | AM_Rp_Evade_* | — | 0 | EvadeAttack | — | 12 | invulnerable 0–0.444 |
| EvadeAttack | AM_Rp_EvadeAttack | 1H_Rapier_PrimaryAttack | 0 | Light2 (via charge C2) | Blade | 0 | |

`ABILITY_IDS`: Tondo → `Ability_Rapier_Slash`, Flurry → `Ability_Rapier_Flurry`, Flourish →
`Ability_Rapier_Flourish`, Riposte → `Ability_Rapier_Riposte`, Fleche → `Ability_Rapier_Aerial_Lunge_Intro`,
Evade_* → `Ability_Rapier_Evasion`. `make_abilities.py`: `"rapier": {"name_key": "ui_rapier_mastery", "trees":
["ui_blood", "ui_evade"], "hud_icon": "lyshineui/images/icons/items/drawing/1hrapier.dds"}`.

### Pipeline requirements

1. Read the weapon-scope `.adb` too (`playermaleanims_1h_meleeattach.adb`, fragments without `AnimLayer`) and merge
   its windows. Without it the lights, the heavy and the sprint attack have no `DamageWindow`/`WeaponTrail`.
   Covered by `window_attach_adbs`.
2. Add the `ExitTime` after an empty clip. Covered by the pipeline.
3. `window_overrides` per montage with extra tags (`ForwardNav`/`BackNav`/`LeftNav`/`RightNav`) for dodge and Evade.
4. Clip start inside the section (`StartTime` of the `Blend`): HeavyCharged at 74%, Light3 at 8%, EvadeAttack at
   20%, SprintAttack Land at 40%. Covered by `montage_starts`.
5. Additive pose `rapier_pose_add` in the Blender/Unreal montage (additive layer, not replacement).

### Requirements for the C++

1. **Rapier charge.** There is no intermediate heavy: on the rapier `HeavyMinHold` = `HeavyFullCharge` = 0.667 s
   (0.594 s on the charge that comes from Light2), and there is no `Heavy` row. The two numbers per weapon (or per
   combo step) come from the table, not from a fixed component property.
2. **Several damage windows per montage, each with its own key** (Light2 with 2 hits, Flurry with 5): reset the
   list of targets already hit at each `DamageKey`.
3. **New shapes:** box in front attached to the root (Tondo) and a capsule that follows the body during the advance
   (Fleche). `Front`/`Around` approximate them, but Tondo loses the 4.5 m width at the end and Fleche must test every
   frame.
4. **Bleed as in the game:** renew the duration and add a stack; cap of 18 s with extensions; minimum 0.5 s between
   applications; consumption by the remaining damage (110% × stacks × 10% × weapon damage × remaining ticks), not a
   fixed value per stack.
5. **Conditional continuation** (Flourish → Finish; Fleche → Wake): an attack input inside a window of the previous
   fragment leads to another montage, with no cooldown of its own.
6. **Riposte:** 1 s state with no damage, no reaction and stun immunity; when hit (server), counterattack with
   invulnerability 0–0.567 and homing to the attacker; 2 s stun that **ends on the first damage**; no hit, exit at
   1 s.
7. **Evade as an ability:** short directional step without stamina, cooldown 12 s, invulnerable 0–0.444, cancels any
   action (abilities included) and allows the quick attack that chains into Light2.
8. **Fleche:** advance with multiplied root motion (×1.3 / ×0.8), gravity ×3, no collision with characters, one hit
   per target during the movement, forced braking at the end.
9. **Grit per window:** the whole Flurry (0–1.367) and the start of the basic attacks are not interrupted.
10. **Homing with approach** (`MoveToTarget`: up to 1 m from the target, at most 4.25 m on the lights).
11. Optional (mastery): cooldown reduction per hit/critical, buffs (Momentum, Insult to Injury, Adagio), stamina per
    hit, stacking haste.

## Known differences

Missing in the port: the Tondo box, collision off during Fleche, Weapon Mastery passives, Interruption (Wake).
