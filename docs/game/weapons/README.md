# Weapons

Player weapons in the port: the list, the state of each one, and how a weapon spec is written. Rules shared by all
weapons (damage, reaction, knockback, camera shake, stamina, block) are in [../combat.md](../combat.md). The
step-by-step procedure to port or refine a weapon is in [../../tech/weapon-porting.md](../../tech/weapon-porting.md).
Tests: [../../tests/weapons.md](../../tests/weapons.md). History: [../../notes/combat.md](../../notes/combat.md).

## Goal and rules of the weapons area

One weapon at a time, until it matches the game.

- Numbers, windows and animations come from the game data, with the source (file + ID) in a short code comment or in
  the weapon spec. No guessing.
- A rule with grade H, an item still open in reverse engineering, or behavior the data does not explain becomes a
  question for reverse engineering (ask the `nw-re` agent), one question per doubt, with what the port needs and the
  leads already known. At most two `nw-re` agents run at the same time (Ghidra is shared).
- While the answer is pending, the item stays "awaiting <weapon>-NNN" (for example `manopla-gelo-001`, `armas-001`)
  in the weapon spec, and work moves to another item. A number, window or behavior without a source does not enter
  the code.
- Damage, critical hits, grit and mitigation are server-side and not in the client. Do not ask reverse engineering
  about them.
- Tests run in `L_Arena`. The maintainer runs the game and sends screenshots; sessions do not run it themselves.

## Order

1. Polish the four weapons that already exist: Sword and Shield, Hatchet, Bow, Rapier (missing abilities, full
   Weapon Mastery tree, sounds, effects).
2. Spear, Great Axe, War Hammer, Greatsword, Flail.
3. Musket, Blunderbuss.
4. Fire Staff, Life Staff, Ice Gauntlet, Void Gauntlet. The first magic weapon brings mana.

Sub-areas run in parallel with the main weapons area, one weapon each:

- **1a. Ice Gauntlet** (`IceGauntlet`). First magic weapon in the port. It brings **mana** (resource,
  regeneration, cost per attack and ability, HUD) for the other magic weapons. Ice projectiles, channeling, cold
  statuses (slow, freeze) and the tree abilities. Spec: [ice-gauntlet.md](ice-gauntlet.md).
- **1b. Greatsword** (`Greatsword`). Two stances (offensive and defensive) with switching and the abilities of each,
  long combos, heavy charge, knockbacks and the weapon's block. No spec yet; it goes in `greatsword.md` in this
  folder, with tests in `docs/tests/greatsword.md`. The survey must cover: stance by tags
  (`SetMannequinTag`/`IfMannequinTag`) and named conditions; `HoldTimeRequired` on the charge; stamina
  (`Greatsword_Offense_Attack` 10); block inside the `BlockActive` window; knockback and reaction of each hit in the
  damagetable. Open questions to ask from the start: `HoldTimeRequired`, the `SingleActivation*` locks, where the
  named conditions live.

A sub-area owns its weapon's files: `NWArma<Nome>.cpp`/`.h`, `tools/sets/player/weapons_<weapon>.json` (added to `parts`
of `tools/sets/sword_shield.json`, minimal change), the weapon spec and the weapon test file. Commit prefix:
`armas(<arma>):`. Files shared by all weapons (`NWCombatComponent`, `NWCharacterWeapons.cpp`, `NWProjectile`,
`make_abilities.py`, Weapon Mastery) get a minimal change, described in the report, with notice to the main weapons
area. A new shared mechanic (for example mana) is born in a new component or file. `build_set.py` builds the whole
player set, so whoever runs it also picks up the other sub-areas' changes; agree on it before running. Area and
file-owner rules for all areas: [../../workflow.md](../../workflow.md).

## Weapon state

| Weapon | Actions | Abilities | Missing |
|---|---|---|---|
| Sword and Shield | complete | 6 | — |
| Hatchet | complete | 6 | — |
| Bow | unaimed shot, aim, draw (normal/charged), automatic reload, dodge, reactions | Rapid Shot, Evade Shot, Penetrating Shot, Poison Shot, Explosive Arrow, Rain of Arrows | poison cloud, Rain of Arrows indicator, aim offset, animated string, full-charge shake; [bow.md](bow.md) |
| Ice Gauntlet (1a) | light and heavy (projectiles), charge, block, dodge, draw/sheathe; mana; game effects in Niagara | Ice Spikes, Wind Chill, Ice Storm, Ice Shower, Ice Pylon, Entombed | v1 with known bugs in the basic attack and some abilities; passives, mana regeneration, sounds; [ice-gauntlet.md](ice-gauntlet.md) |
| Rapier | 3-hit combo (Light2 with 2 hits), charge up to 0.667 s, sprint attack, short step, block | Tondo, Flurry, Flourish and Finish, Riposte, Fleche, Evade | Tondo box, collision off during Fleche, Weapon Mastery passives, Interruption (Wake); [rapier.md](rapier.md) |

Known differences common to all weapons: Gear Score and player mitigation have no game formula; the port damage
scale is Sword = 100 (the game uses 64). See [../combat.md](../combat.md#known-differences-in-the-port).

## Specs

| Weapon | Spec |
|---|---|
| Bow | [bow.md](bow.md) |
| Rapier | [rapier.md](rapier.md) |
| Ice Gauntlet | [ice-gauntlet.md](ice-gauntlet.md) |
| Sword and Shield, Hatchet | none; data in `extract/research/sword_report.md` |
| Greatsword | planned (`greatsword.md`) |

## When a weapon is done

| Part | Done when |
|---|---|
| Animation | every game action; locomotion with the weapon; draw, sheathe and swap; aim; transitions without pops |
| Mechanics | combo, charge, cancels and `.adb` windows; stamina or mana; cooldowns; PowerLevel, HitStun, knockback |
| Abilities | all abilities in the tree, with upgrades and Weapon Mastery passives |
| Sound | swing, impact, voice, abilities |
| Effects | weapon trail, abilities, statuses |
| HUD | icons, cooldowns, ammo or mana, reticle |
| Network | the other player sees the same; owner prediction without hitches |

A weapon is done when its tests pass with the maintainer and its spec has no pending item that depends only on
reverse engineering.

## How a weapon spec is laid out

One file per weapon in this folder, named in English (`bow.md`, `rapier.md`). Models: [bow.md](bow.md),
[rapier.md](rapier.md). Read the shared rules in [../combat.md](../combat.md) before the survey, and read the weapon
through them: combo procclips (`CAGE-TransitionInstant`/`CAGE-TransitionBuffered`: `Input`, `BackTicks`,
`Condition`, `NewFragment`/`NewAction`), `CAGE-Named` windows and the `Conditional` parameters that depend on them,
the exit of each action and its `.grid` entries, `Homing` and `CameraRelativeFacing` per hit,
`AnimationDrivenMotion`, stamina or mana, reaction and knockback of each hit, and the actionlist activities
`ConsumeCharge`, `SetCooldownTimer`, `ModifyStatusEffects`, `PerformActiveAbility`. For a magic weapon, read the mana
data in the player slice and tables; what only the executable defines (regeneration, delay, per-frame channel cost,
charges) becomes a reverse-engineering question.

Sections, in order:

1. **Conventions and sources.** Time units, table paths, and a table of the files used (item, damage, stamina or
   mana, cooldowns, spells, abilities, statuses, `.adb`, actionlists, localization). The generated report
   `extract/research/<weapon>_report.md` and its `.json` are the raw data.
2. **The weapon.** Item fields (`BaseDamage`, `CritChance`, `CritDamageMultiplier`, `BaseStaggerDamage`,
   `BlockStaminaDamage`, `BlockStability`, scaling, Mannequin tags), mesh and socket.
3. **Basic mechanics.** Controls, state flow, each basic action with fragment, clips, windows, DamageID, stamina,
   cancels.
4. **Abilities.** Each active ability with AbilityID, cooldown, DamageID, hit shape, fragments and times; upgrades,
   passives, statuses.
5. **Damage rows.** The weapon's rows in `javelindata_damagetable`.
6. **Locomotion and loops.** Blend spaces, idle, aim poses.
7. **Unknowns.** What has no source, with "awaiting <weapon>-NNN" when a question is open.
8. **Proposal for the port.** Montage prefix, actions table (action, montage, fragment, DamageID, stamina, cooldown,
   hit shape), order of work, and what needs new C++.
9. **Known differences.** What the port still does differently, and what is missing to close it.

Dated state and test reports go to [../../notes/combat.md](../../notes/combat.md), not to the spec.
