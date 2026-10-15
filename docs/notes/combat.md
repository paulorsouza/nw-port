# Notes: combat and weapons

Dated history of the combat rules, the weapons area and particles. Current rules and specs:
[../game/combat.md](../game/combat.md), [../game/weapons/README.md](../game/weapons/README.md),
[../tech/weapon-porting.md](../tech/weapon-porting.md), [../tech/particles.md](../tech/particles.md).

## 2026-10-11: first mechanics plan

Goal: Sword and Shield combat for 3 players, with the game's numbers and windows where they exist and simplified
rules elsewhere.

Decisions:

- Network from the start. Listen server (one of the three hosts). The server decides hit, damage, stamina and death;
  the client only predicts animation and movement.
- C++ for the rules (`UNWCombatComponent`: state, stamina, damage, replication), Blueprint for animation and tuning.
  No GAS: for this scope it adds more pieces than it solves.
- Game data in DataTables (CSV from the `.datasheet` and `.adb`), so changing a number needs no recompile.

Planned rules at the time (several were replaced by the game rules on 2026-10-12):

- Damage: `damage = weapon_base_damage × DmgCoef × (1 − armor_mitigation)`. Base damage and mitigation constant;
  attributes, Gear Score, critical and damage types left out.
- Block: holding the button raises the shield (`BlockStart` → `BlockIdle`); frontal 180° arc; a blocked hit takes no
  health, takes stamina and applies `BlockHitStun` (`BlockReact`); stamina zero on block → `BlockBreak` and a short
  stun.
- Dodge: input direction (F/B/L/R), costs 45 stamina, invulnerable 0–0.5 s, cancels attacks in `CanDodgeCancel`.
- Stamina: max 100, regenerates after a short delay. At 0 the player is exhausted: no dodge and no block until part of
  it recovers.
- Reaction: a hit with `PowerLevel` above the target's resistance interrupts and plays `Hit_F/B/L/R` by impact side,
  with the table's knockback. During `ActivateGrit` the target is not interrupted.
- Targets: first a training dummy; then a game NPC extracted with animations (`nwbt models npcs`) with a simple
  Behavior Tree (approach, attack, sometimes block).

Open at the time: the unit of `HitStun` (15–50) and the scale of `PowerLevel`. Resolved on 2026-10-12 (frames at
30 fps; PowerLevel is a reaction category).

Milestones (each ends with a PIE test):

| # | Delivery | Test |
|---|---|---|
| M1 | Project, import, character with camera and locomotion (strafe/sprint) | walk and run with the right animations |
| M2 | Light/heavy attacks with combo, buffer and windows | 3-hit combo chains only inside the window |
| M3 | Training dummy, sweep hit, damage, floating numbers | damage matches `DmgCoef` |
| M4 | Block, dodge with invulnerability, stamina and exhaustion | a hit in the dodge window misses; zero stamina breaks the block |
| M5 | Reactions, interruption, death and respawn; weapon sounds and trail | trade hits between two characters |
| M6 | Network: 3 players on a listen server | PIE with 3 clients; all three see the same result |
| M7 | Game enemy with AI | fight an NPC |

The map/scenery was a separate track, after M3.

## 2026-10-12: game combat rules and the first fidelity round

Survey of the game's combat numbers from the extracted data, to remove the guesses in `NWCombatComponent`. Result:
[../game/combat.md](../game/combat.md).

Snapshot of the `UNWCombatComponent` constants then (`Public/NWCombatComponent.h`) against the game:

| Constant | Port then | Game | Rule |
|---|---|---|---|
| `SecondsPerHitStun` | 0.01 | 1/30 = 0.0333 | Done: `combate.quadros_por_s` and `HitStunStart` (code). |
| `Poise` + test `PowerLevel >= Poise` | 0 | No poise | PL is a category. Player target: PL 0 only flinch, 1–3 interrupts (without grit), ≥4 knockdown. AI target: use the `_AI` columns (0 = no override); stagger rating 20 (damned) / 10 (animals) against `BaseStaggerDamage × StaggerDmgModifier`; 2 s limiter. |
| `ArmorMitigation` | 0.3 | 0.35 for AI | Vitals `PhysicalMitigation`; cap 0.9. Player: no formula. |
| `BlockStaminaBase` + `BlockStaminaPerPower` | 15 + 8 × BPL | — | `BlockStaminaDamage × BlockStaminaDmgMod × max(1 − BlockStability/100, 0.1)`. Sword 36/22, round shield 54/30, damned 100. |
| `StaminaRegenPerSecond` | 35 | 33.33 | Player slice. |
| Regen while blocking | 0 (stops) | 10/s | Off only while raising the guard. |
| `StaminaRegenDelay` | 1 | 1 | Ok. Apply after dodge and on block release. |
| Exhaustion exit | 50% | 100% | Action `Exhausted` (later replaced by the RE rule: 10 s effect, ends at stamina ≥ 1). |
| Dodge | blocked if exhausted | same + requires stamina > 0 | Cost 45 (50 on heavy load). A dodge can start with less than 45. |
| `BlockArcDegrees` | 180 | 180 | Ok (inferred). |
| `MaxStamina` | 100 | 100 | Ok. |
| `WeaponBaseDamage` | 100 | 64 (sword), 67 (hatchet) | Plus attribute scaling and GS. AI: level `BaseDamage` × modifiers. |
| `MaxHealth` | 1000 | 856 + 40/CON point above 5 | Vitals `Player` and `javelindata_attributeconstitution`. |
| `HitStopSeconds` | 0.07 | 0 | Done: `combate.hit_stop_s` (code). |
| Knockback: constant force for 0.25 s | `ImpactDistanceY` | decay | Total `D`, `v_k = v0 · r^k` per 1/30 s tick, target scale (damned 1.2). Use `ImpactDistanceX` and `UseAttackerPosForReaction`. |
| Critical | absent | 7% sword, ×1.3 | Backstab at ≥ 135° = critical. `CritHitStun`, `CritImpactDistanceY`, shake `Large_short`. |
| Camera shake | absent | table | Only on criticals for sword hits; AI → target `Small`. |

By 2026-10-15 the mechanics table in [../tech/weapon-porting.md](../tech/weapon-porting.md) lists critical hits,
backstab, headshot, reaction by category, AI stagger with the 2 s limiter, knockback with decay, the game camera shake
and status effects. The knockback still had no Z axis and no `PushBackScale`.

### Bow survey (2026-10-12)

Survey written from the extracted files ([../game/weapons/bow.md](../game/weapons/bow.md)). The projectile and AoE
files were fetched with nwbt in a single call and kept only in the session scratch area; the values used are in
`extract/research/bow.json`. At that time the port only had fixed buffs (Berserk, Last Resort), no status effects
with tick.

### Rapier survey (2026-10-12)

Survey in [../game/weapons/rapier.md](../game/weapons/rapier.md). Two findings about the pipeline at that time
(nothing in `tools/` was changed during the survey):

1. The basic-attack damage windows are in the weapon-scope `.adb` (`playermaleanims_1h_meleeattach.adb`).
   `adb_windows.best_fragment` required an `AnimLayer` and `build_set.py` only read the character `.adb` files, so
   those windows did not enter.
2. `tools/adb_windows.py` did not add the `ExitTime` that comes after an empty clip. It put the Flurry hits at
   0.000–0.334 (correct: 0.400–1.212) and the second Light2 hit at 0.400 (correct: 0.533). It also affected 40
   variants of `player_sword.adb` (in practice only `ApplyRotation_PostDodge` of the fast roll, 0.067 → 0.167).

Both were later covered by the pipeline (`window_attach_adbs`, correct `ProcLayer` timing after an empty clip), as
described in [../tech/weapon-porting.md](../tech/weapon-porting.md) on 2026-10-15.

Already in `NWCombatComponent` on 2026-10-12: charge with `HeavyMinHold`/`HeavyFullCharge` and combo via
`ComboNext`; statuses with stacks and tick as a fraction of weapon damage (`ApplyStatus`, `StatusID`...); consumption
by stack (`ConsumeStatusID`/`ConsumeDamage`); speed on self (`SelfMoveSpeed`); target stun (`TargetStun`); shapes
`Blade`/`Front`/`Around`. `ApplyStatus` already renewed the duration and added a stack, like the game; the `ApplyStun`
then only ended by time.

Rapier unknowns at the time that the combat rules later answered: the meaning of `PowerLevel`/`HitStun` and the
backstab rule.

## 2026-10-13: reverse-engineering results and work areas

Reverse engineering of `NewWorld.exe` resolved the action-flow rules (combo by `CAGE-Transition*` procclips, exit by
`DeactivationCondition`, `.grid` interruption, `CAGE-Named` windows, animation-driven movement, turning, homing,
reaction lookup, knockback and hit stun in 1/30 s frames, no hit stop, stamina numbers). They are in
[../game/combat.md](../game/combat.md#1-action-flow). Weapon work was split into the weapons area and two sub-areas
(1a Ice Gauntlet, 1b Greatsword).

Divergences recorded then, to be fixed once by the weapons area: dodge needed 45 stamina; exhaustion blocked dodge
and block until part of the bar recovered; stamina regenerated while blocking after a delay; block active while the
button was held. The RE exhaustion rule (10 s effect, ends at stamina ≥ 1) replaced the earlier data reading (exit at
100%).

Plan for the weapons area:

1. An "RE × port" matrix as a section of the weapons porting doc: one row per RE rule (combo procclips, exit by
   `DeactivationCondition`, `.grid`, `CAGE-Named` windows and Default/Conditional, `AnimationDrivenMotion`, reaction by
   PowerLevel and side, knockback on three axes, hit stun, stamina, dodge, block and exhaustion, turning, homing,
   cooldowns and statuses), with what the port does (file and function), the divergence, the evidence and the open
   question id. Sub-areas read it before a weapon survey. Not present in the weapons docs on 2026-10-15.
2. Every H-graded rule, open RE item or doubt becomes a question for reverse engineering.
3. Fix what applies to all weapons (the divergences, chaining by procclips, windows, turning, homing, knockback).
4. Polish the four existing weapons one at a time.

## 2026-10-15: Ice Gauntlet v1

Survey and plan in [../game/weapons/ice-gauntlet.md](../game/weapons/ice-gauntlet.md).

Planned changes to shared files, with owners at the time:

| File | Owner | Change |
|---|---|---|
| `NWCharacter.h/.cpp` | shared | create the `UNWManaComponent`; `FNWWeaponSet` accepts a skinned mesh (`USkeletalMesh`, following the body pose) besides a static one |
| `NWCombatTypes.h` | shared | column `ManaCost` (CostID) in `FNWActionRow`; status category (`Root`) |
| `NWCombatComponent.cpp` | weapons area | `CheckAction` refuses when mana does not cover the cost (`IfManaCost`); charge on the procclip; root blocks walking |
| `NWCharacterArmas.cpp` | weapons area | one line calling the gauntlet registration in `AddPlayerWeaponSets` |
| `make_datatables.py`, `make_abilities.py` | weapons area | `ICEGAUNTLET_*` and `WEAPONS["IceGauntlet"]` (new blocks, other weapons untouched) |
| `tools/sets/sword_shield.json` | shared | `parts` += `player/armas_manopla-gelo.json` |
| `NWHUD.cpp` | systems area | the `ManaMeter` bar already exists in the layout; bind it to `UNWManaComponent` (ask the systems area or a 1-line change) |

`ABP_NW`: nothing new (the `UpperBody` slot already existed for the Bow).

State through the day:

- Part 1: weapon registered (`NWArmaManopla.cpp`, `Id` `IceGauntlet`), skinned glove (`SK_icegauntlet`, follows the
  body pose) and palm crystal (`SM_icegauntlet_core`), `UNWManaComponent` (HUD bar, F10 > Refill mana / Infinite
  mana), `ManaCost` column on the actions (`IfManaCost`), light and heavy with the `IceMagic` profile projectile,
  block, dodge and draw/sheathe with the gauntlet animations (`AM_Ig_*`), mastery tree in the data
  (`abilities.*.json`, abilities still without actions). Test pair: F10 > Ice Gauntlet + sword. Tests:
  [../tests/ice-gauntlet.md](../tests/ice-gauntlet.md) Part 1. No mana regeneration until manopla-gelo-001.
- Abilities: the six actives with game animation (`AM_Ig_*`) and mechanics in `UNWIceGauntletComponent`
  (`NWArmaManopla.cpp`, numbers with source in `namespace NWIce`): Ice Spikes (7 spikes every 65 ms, Mighty Spike at
  the end or earlier with a basic attack), Wind Chill (11 ticks in 2 s, capsules from the `.adb`), Ice Storm (5 m
  area for 5 s, tick 0.33 s, −25%), Ice Shower (5 boxes for 4 s, −50%, root 1 s and immune 6 s, −50% for 3 s after
  leaving), Ice Pylon (15 s, one shot per second within 20 m, cooldown when it disappears), Entombed (10 s,
  invulnerable, +6 mana/s, break with a basic attack; cooldown on exit). Defaults on Q/R/F: Ice Storm, Wind Chill,
  Ice Spikes. Passives: not yet.
- Fixes after the first test: clicking while running asked for `SprintAttack`, which the gauntlet does not have, so
  the light did not fire (now the sprint attack only starts if the weapon table has the row,
  `NWCharacterArmas.cpp`). Actions that chain into themselves (`WindChillAim`, `EntombedLoop`) now play the montage
  in a loop (`MulticastPlayLoop`) instead of restarting each round: restarting the same montage interrupted the
  previous one and ended the tomb right after it started.
- Game effects: 28 effects from `wfx_icegaunt_base`, `wfxo_icegaunt_tempest` and `wfxo_iceguant_builder` converted to
  Niagara (`/Game/NW/FX/NS_<Library>_<Effect>`).
- New damage rows for the gauntlet disappeared after import: `DT_NWDamage` is one table for all sets and the
  `lost_sword` import runs after the player. Fix: generate the CSVs of both manifests.

**First version closed on 2026-10-15** with known bugs (the maintainer's decision: continue as is). Maintainer test
on 2026-10-15: the basic attack still has many defects (the light sometimes does not fire, consecutive clicks cut the
animation, sometimes no damage); the sprint attack input works only in part; some abilities still fail; some ability
effects are still far from the original. Ask for screenshots per ability before changing them.

Maintainer report (2026-10-15): the light and heavy sometimes deal no damage; the maintainer suspects magic weapons
have a minimum animation time the port's calculation does not respect. To investigate later: shot time (0.167 s)
against the action switch, `Ability_Startup`, `g_attackSlowTime`/`g_attackRecoveryTime` (armas-001).

## 2026-10-15: particles study and decision

Question (Ice Gauntlet sub-area): can the port bring the game's visual effects (particles) instead of placeholders?
Case studied: Ice Storm (`IceMagic_Spell_IceStorm`). The port then had only `ANWFxActor`: spheres with `M_NW_FX`
(color + intensity), no sprites, no decals, no atlas. The project backlog listed particles as outside the conversion
([../backlog.md](../backlog.md)).

Options considered:

- **A. Niagara from templates.** Build by hand, once, 3 template emitters (sprite, mesh, decal) with user parameters;
  a Python script creates one system per game effect, copies the templates and fills the parameters. Pro: Niagara
  performance and lighting. Con: the Niagara Python API does not create modules, and the `ParticleAge` curves do not
  go in as simple parameters (they would need a curve texture). Needs manual editor work by the maintainer.
- **B. Own C++ emitter (`UNWParticleComponent`).** Reads the JSON from the XML (generated by our script) and simulates
  on the CPU the subset the game uses: count and continuous, lifetime, speed and angle, gravity, air resistance,
  simple turbulence, age curves (size, alpha, color), atlas, camera-facing or free. Sprites drawn in an
  `UInstancedStaticMeshComponent` of quads with per-instance color, alpha and frame; decals with `UDecalComponent`;
  meshes with `UStaticMeshComponent`. Three master materials (additive/translucent sprite, mesh with rolling noise,
  decal). All by script and code, no manual step, like the port's screens. Con: less performance than GPU Niagara
  (hundreds of particles per effect are fine, thousands are not), no per-particle light.

Recommendation before the decision: option B, starting with the blizzard:

1. `tools/make_particles.py`: XML → JSON (emitters, curves, atlas, materials) + list of textures and meshes for nwbt
   (`--x-dds png`).
2. Master materials `M_NW_Particle_Sprite`, `M_NW_Particle_Mesh`, `M_NW_Particle_Decal` (by script, like the others).
3. `UNWParticleComponent` with sprite, decal and mesh; the Ice Storm area plays `Blizzard_AOE`.
4. Compare with a game screenshot; then spikes, wind, shower, pylon and tomb (other `wfxo_icegaunt_*` libraries).

Estimate then: 1 to 2 days for the blizzard to look close, since it was the first effect and included the emitter,
the 3 materials and the converter. Left out in that phase: `EmitterStrength`, `MotionBlur`, `SoftParticle`,
`CameraNonFacingFade`, dynamic light (`LightSource`) and the PostFX timeline. Ownership noted then: shared code
(`NWFxActor`) belongs to the weapons area; use in the scenery to the lighting area.

**Maintainer decision (2026-10-15): Niagara.** An API probe (`UNWNiagaraTools::ProbeNiagara`, `NWNiagaraTools.cpp`,
called from Python in a commandlet) showed that editor C++ can, without opening the editor, create a system from an
engine template emitter, set constants and curves, swap material and atlas, compile and save. Plan:
`tools/make_particles.py` (XML → JSON per effect), master particle materials by script, `UNWNiagaraTools::BuildEffect`
(one Niagara emitter per game emitter, from the engine template), and the Ice Storm area plays the system. The
pipeline was in place the same day ([../tech/particles.md](../tech/particles.md)).

Incident: opening the editor with the original New World running exceeded the 8 GB of the graphics card and hung the
driver; a reboot was needed.
