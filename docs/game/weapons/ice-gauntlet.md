# Ice Gauntlet

Survey of the original game's Ice Gauntlet, the mana it brings to the port, and the port requirements. Sub-area 1a
of the weapons area ([README.md](README.md)). Shared rules: [../combat.md](../combat.md).

Full generated report (every window per fragment, passive texts, spells): `extract/research/icegauntlet_report.md`
and `.json`, generated with:

```bash
python3 tools/weapon_report.py icegauntlet --tag Ice --adb player/player_magicgauntlet_ice.adb \
    --base MagicGauntlet --damage IceMagic_ --cost IceMagic --ability icemagic
```

Conventions: times in seconds of the fragment (ProcLayer of the `.adb`); "end" = until the end of the fragment.
Tables relative to `extract/data/sharedassets/springboardentitites/datatables/` (DT). Actionlists in
`extract/serverpak/client/sharedassets/springboardentitites/playerdata/cage/`. Open questions carry the id
"awaiting manopla-gelo-NNN" (reverse-engineering question ids).

## Sources

| Data | File |
|---|---|
| Item | `javelindata_itemdefinitions_weapons` (`1hElementalGauntlet_IceT2..T5`) |
| Damage | `javelindata_damagetable` (15 rows `IceMagic_*`) |
| Mana (costs) | `javelindata_manacosts_player` (`IceMagic_*`, `Zero_Mana`) |
| Mana (resource) | `slicejson/slices/player.dynamicslice.json` (`ManaComponent`), `vitalstables/javelindata_basevitals_player` (`Player`) |
| Cooldowns | `javelindata_cooldowns_player` (`IceMagic_*`) |
| Spells | `javelindata_spelltable_icemagic` (91 rows) |
| Statuses | `javelindata_statuseffects_icemagic` (42 rows) |
| Abilities and mastery | `weaponabilities/javelindata_ability_icemagic` (40 rows) |
| Animations and windows | `adb/player/player_magicgauntlet_ice.adb`, `player_magicgauntlet.adb`, `player.adb` |
| Action logic | `playeractions_ability_icemagic.actionlist` (19 actions), `playeractions_attack.actionlist` (basic attack entry), `player.grid` (L2178–2199) |
| Texts | `extract/loc/localization/pt-br/weaponabilities.loc.xml` (and `en-us`) |

## 1. Basic mechanics

### Item

`1hElementalGauntlet_IceT5`: BaseDamage 67, CritChance 0.09, CritDamageMultiplier 1.3, BaseStaggerDamage 43,
BlockStaminaDamage 43, BlockStability 30, `CanBlockRanged` (scale 0.5/0.5), scaling INT 0.0095 / STR 0.004,
`MannequinTag=MagicGauntlet,Ice`, `RangedAttackProfile=IceMagic`, `FireJoint=aim_direction`, mastery
`IceMagicAbilityTable`. Mesh: `.skin`
(`objects/weapons/magic/gauntletcore/1h/icegauntlett5/wep_mag_gauntcore_1h_icegauntlett5.skin`), a mesh skinned to
the player skeleton, not a static weapon in a socket.

Port scale (Sword 64 = 100): BaseDamage 67 → **104.7**; StaggerDamage and BlockStaminaDamage 43.

Mesh (appearance `Elemental_Gauntlet_T5_Ice`, `javelindata_itemappearancedefinitions`): the glove
`objects/weapons/magic/gauntlets/1h/icegauntlett5/wep_mag_gaunt_1h_icegauntlett5.cdf` is skinned on the right arm
(forearm, hand and fingers); the palm crystal (`gauntletcore/.../wep_mag_gauntcore_1h_icegauntlett5.cdf`) is rigid on
`handattach`, like the rapier. The appearance replaces the body's right forearm (`RightHandOnlySkin`
`male_naked_right_forearm.skin`).

### Grid (`player.grid`)

- `UpperBodyAttack`: Attack_Primary, Attack_Heavy (play on the upper body, while walking).
- `Attack`: Spikes, Entombed, IcePylon.
- `Aim`: IceStorm (+ AimSubState, Casting, Cast), WindChill (+ AimSubState, Outro), IceShower.
- `IsIgnoredByGrid`: Entombed_DodgeExit, IcePylon_Dodge, IcePylon_Cooldown(2), IcePylon_Destroy, Quick_Frost.
- `Attack → Aim` allowed with `IfMannequinTag Ice` (an aim ability interrupts the others).

### Basic attack: light and heavy

Entry: `Attack_Start_Charge_Upperbody` (`playeractions_attack`) is active with `UseUpperBodyAttacks` (main-hand
weapon tagged Ice/Fire/Heal/VoidGauntlet). Fragment `Attack_Start_Charge_UpperBody [MagicGauntlet+Ice]`
(`ice_gnt_windchill_charge_ub`), `CAGE-TransitionInstant` with `TransitionKey=Attack_Branch`:

| t | Condition | Action |
|---|---|---|
| 0.033 | `Attack_Heavy_NotHeld_Magic` (released, or mana < heavy cost) | `IceMagic_Attack_Primary` |
| 0.500 | `CanAttack_Heavy_Magic_GreyWizardHatArtifact` | `IceMagic_Attack_Heavy` (artifact, out of scope) |
| 0.667 | `CanAttack_Heavy_Magic` (tag Ice and `IfManaCost ≥ IceMagic_Heavy`) | `IceMagic_Attack_Heavy` |
| 0.700 | always | `IceMagic_Attack_Primary` |

Same charge rule as the other weapons: release before 0.667 = light, hold until 0.667 = heavy (in the port,
`HeavyMinHold` 0, `HeavyFullCharge` 0.667). Mana cost of both = 0.

| Action | Fragment / clip | Projectile | DamageID | Combo |
|---|---|---|---|---|
| Primary | `Ability_IceMagic_Primary` / `ice_gnt_lt_atk_up` | `CAGE-CastSpell IceMagic_Light_Projectile` at 0.167 | `IceMagic_Primary` (1.0) | `CanAttack_Light_Magic` buffered 0.267–0.933, instant 0.933 → new charge |
| Heavy | `Ability_IceMagic_Heavy` / `ice_gnt_lt_atk_up` | `IceMagic_Heavy_Projectile` at 0.167 | `IceMagic_Heavy` (1.4) + `IceMagic_HeavyAttackSlow` (−10%, 3 s, cooldown 6 s) | buffered 0.1–0.833, instant 0.833 |

Activities of both (`playeractions_ability_icemagic` L3, L227): `CameraRelativeFacing` Default 0, `AlignToCamera`
0.40 exponential, `AlignToCameraFast` 0.90; `ResetGrit`; `SetHaltMotionOnCollision 170`; `Sprint_Delay_Timer`.

Projectiles (`rangedattackdatabase.radb`, profile `IceMagic`, and slices `items/weapons/icemagic_primary_proj` /
`_heavy_proj`, extracted in `extract/weapons_icegauntlet/`): 100 m/s (`m_basespeed`; `m_extraspeed` 25 with no known
use), lifetime 0.5 s (`m_flyinglifetime`) = range 50 m (`m_maxrange`), radius 0.15 m (Primary) / 0.33 m (Heavy),
straight for 100 m (`m_projectileacceldelaydist`, no drop within range), spawned at (0.3; 0.3; 1.2) m from the
character (`m_characteroffset`), destroyed on hit or when blocked.

Common windows: `ForceWalk` (walks while attacking), `BlockDisabled` 0.1–0.9, `CanDodgeCancel` 0–0.1 and
0.667–end, `CanBlockCancel` 0.9–end. Any ability can start from the 0–0.1 window of the light/heavy
(`CanAttack_<ability>` buffered and then instant).

### Block, dodge, draw

- Block: `Block_UpperBody_Raise` / `Block_UpperBody` (`ice_gnt_shield_idle_ub`), `BlockActive` over the whole
  fragment. Uses stamina only (`playeractions_block`); `IceMagic_Block` (10 mana) exists in the table but no
  actionlist or `.adb` uses it.
- Dodge: `Dodge_Roll` / `Dodge_Roll_Slow` with `cmbt_1h_quickstep_f(_hvy)` + layer `ice_gnt_pose_add`; invulnerable
  0–0.5; `CanDodgeCancel` 0.267–0.5 buffered.
- Draw: `Unsheathe_UpperBody` (`ice_gnt_idle_draw_ub`), weapon appears at 0.2 (`unsheathe_weapon`).
- Locomotion: `Strafe` = `melee1h_cmbt_nav_strafe_blend` + layers `v2_ice_gnt_cmbt_walk_layer_l/r`; `Idle` =
  `cmbt_idle_1h` + `ice_gnt_pose_add` + `ice_gnt_idle_rhand`; stop `ice_gnt_sprint_stop_blend.bspace`.

## 2. Mana

The Ice Gauntlet is the first magic weapon in the port; its mana model is the one the other magic weapons use.

| Piece | Value | Source |
|---|---|---|
| Resource | min 0, max 100, initial 100 | `ManaComponent` (`m_min`, `m_initmax`); `basevitals_player` (`ManaBaseMax`, `ManaInitial`) |
| Regeneration | **conflict**: `m_regenrate` 1 (slice) × `ManaBaseRegenRate` 2 (table); unit not stated | awaiting manopla-gelo-001 |
| Delay | `ManaRegenDelay` 0, `ManaRegenDelayOnEmpty` 2 | `basevitals_player`; meaning awaiting manopla-gelo-001 |
| Out of combat | `Status_OutOfCombat_PassiveRegen` ManaRate 1, 180 s | `statuseffects_common` |
| Focus 100 | `Foc_Bonus_100_1` ManaRate 0.2, MaxMana 0.1 | `ability_attributethreshold` |

Costs (`javelindata_manacosts_player`) and where each is charged:

| CostID | Mana | Charged by | Where |
|---|---:|---|---|
| IceMagic_Primary / Heavy | 0 | — | — |
| IceMagic_Spikes | 15 | procclip `CAGE-PayManaCost` | `Ability_IceMagic_Spikes` 0.2 |
| IceMagic_WindChill | 20 | procclip | `WindChill_Aim(_Long)` 0.067 |
| IceMagic_Entombed_Break | 10 | procclip | `Entombed_Break` 0.033 |
| IceMagic_IcePylon | 15 | activity `PayManaCost` | action `IceMagic_IcePylon_Cooldown`, after `Pylon_Spawn` |
| IceMagic_IceStorm | 25 (5 with Storm Summoner and full mana) | spell column `ManaCost` | `IceMagic_Spell_IceStorm[_Mana]` (awaiting manopla-gelo-002) |
| IceMagic_IceShower | 25 | `ManaCost` column of `Shower1` | same |
| IceMagic_Entombed | 0 | `ManaCost` column of `Spell_Entomb` | — |

Conditions: `IfManaCost CostID ComparisonType` (an ability only starts with mana ≥ cost; 96 uses) and `IfManaValue`
(percentage: `Equal 1` = full mana). The Ice Gauntlet has no channel paid over time (only the Fire Staff has one);
Wind Chill pays once. No activity turns mana regeneration off.

Mana gains: `IceMagic_Entombed_ManaRegen` (ManaRate 6, 10 s), `IceMagic_EmpoweredFrost` (+6 per attack in a frozen
area, passive Empowered Frost), `IceMagic_UnbrokenWindsPerk` (+10, global perk), `IceTempestPass2` (+10% on
critical), `_Helper` (+5% on heavy, cooldown 0.5 s), `IceTempestPass4` (+15% every 3 lights in a row),
`IceMagic_BlockingStamina` (−5 mana, +10 stamina when blocking with mana > 3%).

## 3. Active abilities

| Ability (key) | Action / fragments | Mana | Cooldown | DamageID | Mechanics |
|---|---|---:|---:|---|---|
| Ice Spikes (`IceMagic_Spikes`) | `Spikes` → `Spikes_Out1..5` | 15 | 10 | `IceMagic_Spikes` 0.61; last `IceMagic_Spikes_End` 1.57 + knockback 2 m | `ChannelSpell IceMagic_Spell_Spikes`: 7 spikes every 65 ms in a line (8 m); a basic attack between 0.267 and 0.633 (`Spikes_EarlyTrigger`) releases the Mighty Spike early (`Spawn_End1..5` at 2.2/3.3/4.4/5.5/6.6 m). Stationary (`PreventNav` 0–1.167) |
| Ice Storm (`IceMagic_IceStorm`) | `IceStorm_Intro` → `_Aim` (aim) → `_Cast` / `_Outro` | 25 | 20 | `IceMagic_IceStorm` 0.17 every 0.33 s | area aim up to 20 m (`CastSpellTargeting`), cast on marker `SpellCastIceStorm` (0.2); sphere radius 5, height 5, 5 s; status `IceMagic_IceStorm` (−25% speed) and `_Damage`. `ForceWalk` while aiming |
| Wind Chill (`IceMagic_WindChill`) | `WindChill_Intro` → `_Aim` (2 s) → `_Outro` | 20 | 15 | `IceMagic_WindChill` 0.24 + knockback 15 (Y) and `_End` 0.3 (last 2 m), every 0.2 s | 2 s channel (3 s with Enduring Chill), 7 m column; strafe walking (`ForceStrafeNav`, `PreventSprint`); early exit with `IceMagic_Heavy_Exit` |
| Ice Pylon (`IceMagic_IcePylon`) | `Pylon` | 15 (on spawn) | 10 (counts only after the pylon dies) | `IceMagic_Pylon` 0.75 per projectile | pylon with 20% of the player's max health, shoots enemies within 20 m, frozen area 1 m for 15 s; holding the key 30 ticks destroys it; infinite cooldown until `Pylon_Death` or 45 s (`PylonCooldownSafety`) |
| Ice Shower (`IceMagic_IceShower`) | `Shower` | 25 | 30 | — | 5 boxes 0.5×0.5×3 m at 0.4/0.467/0.533/0.6/0.667 at 2.8/2.25/2/2.25/2.8 m; 4 s; `IceMagic_Frostbite` |
| Entombed (`IceMagic_Entombed`) | `Entombed_Intro` → `_Loop` (10 s) → `_Outro` or `_Break` | 0 (break: 10) | 30 (starts on exit) | `IceMagic_Entombed_Break` 0.7, sphere 3 m, knockback 2 m | invulnerable, no movement, ManaRate 6; tomb with 50% of max health; exits with the key, block or dodge; a basic attack breaks it (area damage) |

Full text and numbers of each ability: report §4.

## 4. Cold statuses

| StatusID | Effect | Duration |
|---|---|---|
| `IceMagic_IceStorm` | slow −25% (walk and run) | while in the area (ClearOnExit) |
| `IceMagic_IceStorm_Damage` | damage 0.17 × weapon damage per 0.33 s tick | while in the area |
| `IceMagic_Frostbite` (Frostbite) | slow −50%; tick 10 → `IceMagic_IceShower_Freeze` | while in the shower, then `_Dur3` 3 s |
| `IceMagic_IceShower_Freeze` | rooted (`NoNav/NoRun/NoSprint`) | 1 s, then immune 6 s (`_Freeze_Cooldown`) |
| `IceMagic_Freeze` | rooted (heavy on a target with Frost, passive Heavy Freeze) | 1 s, then immune 4 s |
| `IceMagic_HeavyAttackSlow` | slow −10% | 3 s, reapplied every 6 s |
| `IceMagic_Entombed` | invulnerable, no movement, no health regeneration | 10 s |
| `IceMagic_Frost` | marker "in the frozen area" (passives) | while in the area |

Categories the port must understand: `Slow`, `Root`, `Frost`, `FrostBuff`, `Haste`, `ArmorRend`, `ArmorFortify`,
`Weaken`. Variants per passive (`_Rend`, `_DeadlyFrost`, `_UltimateChill`, `_Long`, `_Quick`):
`javelindata_statuseffects_icemagic`.

## 5. Passives and trees

Two trees (`TreeId` 0 Ice Tempest, 1 Builder; `ui_icemagic_tree1`/`ui_icemagic_tree2`), 6 actives and 34 passives
with position (`TreeRowPosition`, `TreeColumnPosition`). List with text: report §4. Those that change mana, cold or
cooldown depend on the passive trigger system (`OnHit`, `OnCrit`, `OnBlockedHitTaken`, `NumConsecutiveHits`,
`TargetStatusEffectCategory`), which the port does not have in general form yet.

## 6. Sounds and effects

Sounds in the `Audio` procclips (`play_gauntlet_ice_*`), read by `make_sound_map.py`. Effect and spell prefabs
(`items/weapons/icemagic_*_proj`, `spell_aoe_blizzard`, `spell_aoe_shower_*`, `Ice_Entomb`, `Ice_Pylon*`,
`Ice_Spike_*`) and materials `materials/vfx/weapon/icegauntlet_*` are in the pak, not extracted.

Particle effects: 28 effects from the libraries `wfx_icegaunt_base`, `wfxo_icegaunt_tempest` and
`wfxo_iceguant_builder` are converted to Niagara (`/Game/NW/FX/NS_<Library>_<Effect>`), with the pipeline in
[../../tech/particles.md](../../tech/particles.md). Projectiles carry a trail (`ProjectileFx`); hand and body effects
are placed on the montages by `tools/ue/fx_notifies.py`; areas, spikes, pylon and tomb are spawned by
`UNWIceGauntletComponent`.

## Unknowns

- Mana regeneration: rate (1 or 2), unit, delay after spending and at zero mana. **Awaiting manopla-gelo-001.**
- Charging through the spell's `ManaCost` column (`CastSpell`, `CastSpellRaycast`) and the `PayManaCost` formula
  (modifiers as for stamina?). **Awaiting manopla-gelo-002.**
- Pylon, tomb and spikes: health, pylon target, fire rate, who applies `IceMagic_Entombed_ManaRegen` and
  `IceMagic_Frost`: prefabs `Ice_Pylon*`, `Ice_Entomb`, `Ice_Spike_*` (extract them; if the logic is not in the
  prefab, ask reverse engineering).
- `ChannelSpell` / `CastingTypes=IceSpikes|PathOfDestiny` (spike spacing and timing): `SpawnRate 65` and
  `NumToSpawn 7` read; the rest awaiting manopla-gelo-003.
- Wind Chill knockback: awaiting manopla-gelo-004.
- Duration of the `.caf` clips (windows marked "end").
- `IceMagic_Windchill_Shatter` is always false in the actionlist (`And(X, Not X)`): in the game the Shatter variant
  never fires. Follow the data.
- `IceMagic_WindChill_SW` is referenced by the ability and missing from the damagetable.
- Minimum animation duration: the light and heavy sometimes deal no damage; the maintainer suspects magic weapons have
  a minimum animation time the port does not respect. To check: shot time (0.167 s) against the action switch,
  `Ability_Startup`, `g_attackSlowTime`/`g_attackRecoveryTime` (armas-001).

## Port design

Montage prefix `AM_Ig_`. The weapon's part of the player set is `tools/sets/player/armas_manopla-gelo.json` (listed
in `parts`). `window_overrides` with tags `MagicGauntlet,Ice` and adbs `player/player_magicgauntlet_ice.adb`,
`player/player_magicgauntlet.adb`, `player/player.adb`; `attach_adbs` = `PlayerMaleAnims_1H_RangedAttach.adb`.
Upper-body montages (slot `UpperBody`, which the Bow already needs) for light, heavy, charge, aim and `_ub` casts.

| Action | Montage | Fragment | DamageID | Mana | Cooldown | Shape |
|---|---|---|---|---:|---:|---|
| Ig_Draw / Ig_Sheath | AM_Ig_Draw / _Sheath | Unsheathe/Sheathe_UpperBody | — | — | — | — |
| HeavyCharge | AM_Ig_Charge (hold, ub) | Attack_Start_Charge_UpperBody | — | — | — | charge 0.667 |
| Light1 | AM_Ig_Primary (ub) | Ability_IceMagic_Primary | IceMagic_Primary | 0 | — | projectile at 0.167 |
| Heavy | AM_Ig_Heavy (ub) | Ability_IceMagic_Heavy | IceMagic_Heavy | 0 | — | projectile at 0.167 + slow |
| BlockStart/BlockIdle | AM_Ig_Block* (ub) | Block_UpperBody(_Raise) | — | — | — | — |
| Dodge_F/B/L/R | AM_Ig_Dodge_* | Dodge_Roll | — | — | — | invulnerable 0–0.5 |
| Spikes | AM_Ig_Spikes + AM_Ig_SpikesOut | Ability_IceMagic_Spikes(_Out1..5) | IceMagic_Spikes / _End | 15 | 10 | line of 7 areas + final spike |
| IceStorm | AM_Ig_StormIntro/Aim(loop)/Cast/Outro (ub) | IceStorm_* | IceMagic_IceStorm | 25 | 20 | aimed area 5 m, 5 s, tick 0.33 |
| WindChill | AM_Ig_WindIntro/Aim/Outro (ub) | WindChill_* | IceMagic_WindChill / _End | 20 | 15 | cone/column 7 m, tick 0.2, 2 s |
| IceShower | AM_Ig_Shower (ub) | Ability_IceMagic_Shower | — | 25 | 30 | 5 boxes, Frostbite |
| IcePylon | AM_Ig_Pylon | Ability_IceMagic_Pylon | IceMagic_Pylon | 15 | 10 after death | pylon actor |
| Entombed | AM_Ig_TombIntro/Loop/Outro/Break | Entombed_* | IceMagic_Entombed_Break | 0 / 10 | 30 on exit | tomb actor |
| Hit_*, Death | the set's | r_R2_*, r_Death | — | — | — | — |

Order of work: (1) mana + light/heavy/block/dodge with projectiles; (2) Wind Chill and Ice Spikes (no new actor);
(3) Ice Storm and Ice Shower (persistent area with cold status); (4) Ice Pylon and Entombed (actors with health);
(5) passives, sounds, effects.

Code:

- `UNWManaComponent` (`NWManaComponent.h/.cpp`): Mana/MaxMana replicated, regeneration and delay
  (manopla-gelo-001), `CanPay(CostID)`, `Pay(CostID)`, gain by status (`ManaRate`, `Mana`). Costs read from
  `javelindata_manacosts_player` (table generated by `make_datatables.py`). HUD bar `ManaMeter`.
- `NWArmaManopla.cpp/.h`: weapon registration (`Id` `IceGauntlet`), `UNWIceGauntletComponent` with the ability
  mechanics (numbers with source in `namespace NWIce`): area aim (Ice Storm), channel (Wind Chill), spikes in a line,
  shower, pylon (`ANWIcePylon`) and tomb (`ANWIceTomb`), F10 buttons via `NW_REGISTER_DEV`.
- Cold statuses: categories `Slow`/`Root`/`Frost` and immunity after a root (`*_Freeze_Cooldown`).
- Shared files touched: `NWCharacter.h/.cpp` (creates the `UNWManaComponent`; `FNWWeaponSet` accepts a skinned mesh
  (`USkeletalMesh`) that follows the body pose, besides a static one), `NWCombatTypes.h` (column `ManaCost` (CostID)
  in `FNWActionRow`; status category `Root`), `NWCombatComponent.cpp` (`CheckAction` refuses when mana does not cover
  the cost, `IfManaCost`; charge on the procclip; root blocks walking), `NWCharacterArmas.cpp` (one line calling the
  gauntlet registration in `AddPlayerWeaponSets`; the sprint attack only starts if the weapon table has the row),
  `make_datatables.py`/`make_abilities.py` (`ICEGAUNTLET_*` and `WEAPONS["IceGauntlet"]`),
  `tools/sets/sword_shield.json` (`parts` += `player/armas_manopla-gelo.json`), `NWHUD.cpp` (`ManaMeter` bound to
  `UNWManaComponent`). `ABP_NW`: nothing new.
- Actions that chain into themselves (`WindChillAim`, `EntombedLoop`) play the montage in a loop
  (`MulticastPlayLoop`) instead of restarting it each round. Restarting the same montage interrupts the previous one.

Default abilities on Q/R/F: Ice Storm, Wind Chill, Ice Spikes. Dev pair: F10 > Ice Gauntlet + sword.

## Known issues

- Basic attack: the light sometimes does not fire, fast clicks cut the animation, sometimes no damage. Sprint attack
  input works only in part. Some abilities still fail. Some ability effects are still far from the original. Ask for
  screenshots per ability before changing them.
- Passives: not implemented.

## Known differences

- Ice Storm has no aim phase: the area drops at the camera point on key press (the game shows the target and waits
  for the click).
- Ice Shower: the 5 boxes appear together at 0.4 s (in the game, one every ~0.067 s); position = distance and turn of
  each `Shower1..5` from the character (`CharRelPosOffsetY` 3 not used, H).
- Ice Spikes: no loop clip between intro and exit; spacing 1.1 m = `CastDistance` (H); the early Mighty Spike goes to
  the next `Spawn_End1..5` position after the last spike.
- Pylon and tomb take no damage (in the game: 20% and 50% of the player's max health); the pylon fires every 1 s (H,
  `m_cooldown` 1 of the slice); no 1 m frozen area around the pylon.
- Root = the port's maximum slow (−90%): there is no "cannot walk" state separate from stun yet.
- Wind Chill has no knockback (awaiting manopla-gelo-004) and no early exit.
- Leaving the tomb: only at the end (10 s) or by breaking it. Dodge does not exit: the loop clip
  `ice_gnt_entombed_loop` has 2 frames and the `CanDodgeCancel` window (0.2 s of the loop) does not fit in the
  montage. Pressing the ability again does not exit either.
- Niagara effects have no light (`LightSource`), no ribbons (`Connection`), no mesh size curve and no color and size
  curves (alpha only); decals have no alpha curve; `EmitterStrength`, `MotionBlur`, `SoftParticle` and the PostFX
  timeline are left out.
- `ice_gnt_pose_add` (additive gauntlet pose in dodges, standing and walking) is left out: the clip composer replaces
  curves instead of adding. Locomotion = `melee1h_cmbt_nav_strafe_blend` + `rswrd_ststrafe_blend` +
  `ice_gnt_idle_rhand` (game variant `Strafe [MagicGauntlet+Ice+AttackAction]`); dodges with `cmbt_1h_quickstep_*`.
- Light and heavy use the same clip (`ice_gnt_lt_atk_up`): the import merges the windows of both fragments in the same
  sequence. On the light, the next click opens at 0.833 s (heavy window) instead of 0.933 s.
- Block reaction and guard break: the Hatchet's (the gauntlet has no fragment of its own).
- `IceMagic_HeavyAttackSlow`: no 6 s `ApplicationCooldown` (the port reapplies it on every heavy).
- The projectile spawns at the port's default point (body center + 70 cm), not at (0.3; 0.3; 1.2) m; projectile mesh =
  palm crystal (prefab `IceMagic_Primary_proj` not converted).
- Mana has no regeneration until manopla-gelo-001.
- The armor's right forearm is not hidden under the glove (`RightHandOnlySkin`): it can clip through.
- Gauntlet sheathed (other weapon active): only the crystal shows on the hip.
- Sounds `play_gauntlet_ice_*` have no audio: the gauntlet bank is not in `tools/extract_audio.py` (`SFX_GROUPS`);
  it comes with the sound part.
- The export computed a hand damage window on `AM_Ig_HeavyCharge` and `AM_Ig_Heavy` (name contains "heavy"); the
  charge has no DamageID and the heavy uses a projectile, so the window hits nothing. Check in test 3.
