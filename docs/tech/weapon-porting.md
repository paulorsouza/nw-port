# Porting a weapon

Step-by-step procedure to bring a New World weapon into the port (Spear, Great Axe, Musket, staffs...) or to refine
one that exists. Numbers, windows and animations come from the game data; the code only applies the rules.

Read first:

- [../game/combat.md](../game/combat.md): shared combat rules (damage, critical, reaction, knockback, camera shake,
  stamina, block) and the reverse-engineering results for action flow (combo procclips, `CAGE-Named` windows, exit
  and `.grid`, homing, turning, animation-driven movement).
- [../game/weapons/README.md](../game/weapons/README.md): weapon order, area rules (no guessing, open questions,
  sub-areas and file ownership), the "done" table and how a weapon spec is laid out.

Models of a finished survey: [../game/weapons/bow.md](../game/weapons/bow.md),
[../game/weapons/rapier.md](../game/weapons/rapier.md).

## 0. Before you start

- `tools/linux/build_all.sh` run up to step 04 (tables, texts and animation recipes extracted). See
  [../setup.md](../setup.md).
- Unreal Editor closed during the import and compile steps.
- One heavy job at a time ([../status.md](../status.md), pitfalls). Heavy jobs go through the queue:
  `python3 tools/pesado.py <command>` (Linux also `tools/linux/pesado.sh`).
- The original New World closed during heavy work.
- Reverse engineering: check what is already resolved in [../game/combat.md](../game/combat.md). What is missing
  becomes a question for reverse engineering (ask the `nw-re` agent).

## 1. Game report

```bash
python3 tools/weapon_report.py <weapon>
```

It only reads what is already extracted; Unreal is not needed. It writes `extract/research/<weapon>_report.md` (and
`.json`) with:

1. Mannequin fragments with a variant for the weapon: clips per layer and windows (damage, chain, cancel, dodge,
   homing, sound);
2. the weapon's damage rows (`javelindata_damagetable`);
3. stamina and cooldowns;
4. Weapon Mastery abilities (name, description, cooldown, tree);
5. spells (areas, projectiles) and status effects.

A weapon not in the script's `WEAPONS` list: pass the tag and prefixes, for example

```bash
python3 tools/weapon_report.py spear --tag Spear --adb player/player_spear.adb --base 2H_Melee \
    --damage 2H_Spear_ --cost 2H_Spear --ability spear
```

To find the tag, open the weapon's `.adb` in `extract/models/animations/mannequin/adb/player/` and look at the
`Tags="..."` of the fragments.

## 2. Survey

Write the weapon spec in `docs/game/weapons/<weapon>.md`, following the layout in
[../game/weapons/README.md](../game/weapons/README.md): what each action does in the game, with numbers and source,
and at the end the proposal (actions, montages, DamageID, stamina, cooldown, hit shape, new code).

What is not in `extract/`:

- State logic: the CAGE actionlists (`extract/serverpak/client/.../cage/`).
- Any pak file: `python3 tools/pesado.py nwbt cat -g "$NW_GAME" <path>` (heavy queue, ~6 GB each).

## 3. Animations: recipe in the player set

All player weapons live in the same set (`tools/sets/sword_shield.json`, same skeleton). The weapons part is
`tools/sets/player/armas.json`; a sub-area weapon has its own part `tools/sets/player/armas_<arma>.json`, added to
`parts` of `sword_shield.json`. Each weapon has a montage prefix: `AM_` Sword, `AM_Hx_` Hatchet, `AM_Bw_` Bow,
`AM_Rp_` Rapier, `AM_Ig_` Ice Gauntlet.

In the `.json`:

- `weapons`: the weapon mesh (`.cgf`/`.cdf`) and the socket (`right_hand_attach`, `left_hand_attach`).
- `montages`: `AM_Xx_Name` → list of sections; each section is the list of clips per layer (full body first, then
  the layers on top, as in the Mannequin). A 3-hit combo = 3 sections (`AM_Xx_LightCombo_1..3`).
- `fragments`: `AM_Xx_Name` → `.adb` fragment(s) the windows come from (one per section).
- `window_overrides`: `{"AM_Xx_": {"adbs": [...], "tags": "..."}}` with the weapon's `.adb` and tags.
- `blendspaces` and `loops`: locomotion with the weapon (strafe, sprint) and idle poses (idle, guard, aim).
- `hold_montages`: montages that hold the last frame (heavy charge, bow draw).
- `upper_montages`: upper-body montages (fragments `_UpperBody`, clips `_ub`). They play in the `UpperBody` slot of
  `ABP_NW`, over locomotion, without root motion (the bow shoots while walking).
- `montage_starts`: a montage that starts in the middle of the clip (fraction), like the `.adb` `StartTime` (rapier
  charged attack at 0.74). Windows are shifted at import and the montage plays from that point.
- `window_attach_adbs` (default) and `attach_adbs` (per prefix): the weapon-scope `.adb`
  (`playermaleanims_1h_meleeattach.adb`), where the exact damage windows of the basic attacks live (`CAGE-Damage`).
  A weapon without its own scope: `"attach_adbs": []` in the override.

What the pipeline does by itself with the `.adb`:

- clip speed in the fragment (`speed="1.1"`) becomes the montage rate, and the windows move to sequence time (rapier
  Tondo plays at 2.2×; sword heavy at 1.1×);
- `ProcLayer` windows with the right time after an empty clip;
- windows of montages that share the same clip are merged (the last one used to overwrite).

Convert (Blender + nwbt, heavy, original New World closed):

```bash
source tools/linux/env.sh && tools/linux/pesado.sh python3 tools/build_set.py tools/sets/sword_shield.json
# or, on any platform
python3 tools/pesado.py python3 tools/build_set.py tools/sets/sword_shield.json
```

`build_set.py` builds the whole set: whoever runs it also picks up other sub-areas' changes. Agree before running.

## 4. Actions: the weapon's action table

In `tools/make_datatables.py`:

- `<ARMA>_ACTIONS`: port action → (montage, DamageID, stamina CostID, next in combo). Fixed names the C++ uses:
  `Light1..3`, `HeavyCharge`, `Heavy`, `HeavyCharged`, `SprintAttack`, `Dodge_F/B/L/R`, `BlockStart`, `BlockEnd`,
  `BlockIdle`, `BlockReact`, `BlockBreak`, `Hit_F/B/L/R`, `Death`, `StunLoop`, `Draw`, `Sheath`. Abilities use any
  name (the same in `ABILITY_IDS` and in `make_abilities.py`).
- `<ARMA>_SHAPES`: hit shape when it is not the blade: `Front` (cone: range cm, angle), `Around` (circle),
  `Projectile` (range). Calibrate with F10 > Show hits (`nw.DebugHits 1`).
- `<ARMA>_ABILITY_IDS` (game cooldown) and `<ARMA>_EFFECTS` (stun target, buffs).
- `SETS["sword_shield"]`: add `("DT_<Arma>Actions", ...)`.

Extra action columns (in the `<ARMA>_EFFECTS` dictionary):

| Column | Purpose | Example |
|---|---|---|
| `StatusID`, `StatusSeconds`, `StatusTick`, `StatusDamage`, `StatusMaxStacks`, `StatusMoveSpeed` | effect on the target (damage per tick = fraction of weapon damage) | Tondo bleed |
| `SelfMoveSpeed`, `SelfMoveSeconds` | speed on self | haste |
| `ConsumeStatusID`, `ConsumeDamage` | consumes the status and deals ×remaining damage | Finish (1.1) |
| `FireTimes`, `FireDamageIDs` | shots (s) and the row of each; on a melee swing, the row of each damage window | Rapid Shot, Flurry |
| `ProjectileSpeed`, `ProjectileRadius`, `GravityDelay`, `bPierce` | arrow (cm/s, cm, cm in a straight line) | 10000, 15, 500 |
| `ExplodeRadius`, `ExplodeDamageID` | explosion on hit/impact | Explosive Arrow |
| `GroundRadius`, `GroundRange`, `GroundDelay` | area at the aimed point | Rain of Arrows |
| `NextAction` | action that starts by itself at the end | shot → reload |
| `StartAt` | fraction of the clip where it starts (from `montage_starts`) | 0.74 |
| `CooldownGroup` | shared cooldown | Evade (4 directions) |
| `bCancelsAny` | exits any action | Evade |
| `CounterSeconds`, `CounterAction` | stance that counters whoever hits | Riposte |
| `bStunBreaksOnDamage` | stun ends on the first damage | Riposte counterattack |
| `ManaCost` | mana CostID (`IfManaCost`) | Ice Gauntlet abilities |

Generate the CSVs (after `build_set.py`, which writes the manifest). `DT_NWDamage` is one table for all sets, and
the `lost_sword` import runs after the player: generate the CSVs of both manifests, or new damage rows are lost.

```bash
python3 tools/make_datatables.py extract/ue/sword_shield/manifest.json
python3 tools/make_datatables.py extract/ue/lost_sword/manifest.json
python3 tools/make_sound_map.py extract/ue/sword_shield/manifest.json
```

## 5. Sounds, abilities on screen, icon

- Sounds: `tools/make_sound_map.py` reads the `Audio` windows of the `.adb` by itself. A sound that did not come out:
  check whether the event is in `extract/audio/sfx_index.csv`; a bank not extracted goes into
  `tools/extract_audio.py` (`SFX_GROUPS`).
- Abilities on the HUD and Weapon Mastery: entry in `WEAPONS` of `tools/make_abilities.py` (trees, actions, the three
  defaults, icon); extraction step 12 (data and icons).

## 6. C++: register the weapon

In `ANWCharacter::AddPlayerWeaponSets` (`NWCharacterArmas.cpp`), one `FNWWeaponSet` per weapon: `Id`, mesh, action
table, locomotion blend space, icon, default abilities, hand (`Socket`), weapon numbers from
`javelindata_itemdefinitions_weapons` (`BaseDamage` on the port scale = game damage × 100/64, `CritChance`,
`CritDamageMultiplier`, `StaggerDamage`, `BlockStaminaDamage`, `BlockStability`), heavy charge (`HeavyMinHold`,
`HeavyFullCharge`) and `bAims` (aiming weapon). A sub-area weapon registers from its own `NWArma<Nome>.cpp` with one
line in `AddPlayerWeaponSets`. The weapon pair in use (keys 1 and 2) is the `Loadout`; F10 > Swap weapon pair cycles
the pairs.

Add new mechanics to `UNWCombatComponent` only if the table below does not cover them. Write the game number and its
source in a short comment, and add a row to this table. A new shared mechanic of a sub-area (for example mana) goes
into a new component or file.

| Mechanic | Where | Data |
|---|---|---|
| Light combo with buffer and windows | `UNWCombatComponent::CheckAction` | curves `CanTransition`, `CanDodgeCancel`, `CanBlockCancel` |
| Heavy charge (early release = light) | `AttackPressed/Released`, `ReleaseCharge` | `HeavyMinHold`, `HeavyFullCharge` |
| Hit by blade sweep | `SweepWeapon` | curves `WeaponTrail`/`DamageWindow` |
| Hit in cone/circle | `AreaHit` | `HitShape`, `HitRange`, `HitAngle` |
| Projectile (throw, arrow) | `SpawnProjectile`, `ANWProjectile` | `HitShape = Projectile` |
| Block, guard break | `ReceiveHit` | `BlockPowerLevel`, stamina |
| Dodge with invulnerability | `RequestDodge` | curve `Invulnerability` |
| Reaction, knockback, camera shake, hit stop | `ReceiveHit`, `MulticastShowHit`, `MulticastHitStop` | damage table |
| Stun, buffs | `ApplyStun`, `StartAction` | `<ARMA>_EFFECTS` |
| Critical (weapon roll and from behind ≥ 135°), head | `ReceiveHit`, `ANWProjectile::IsHeadHit` | `DmgCoefCrit`, `DmgCoefHead`, `NoBackstab` |
| Reaction by category (PowerLevel), AI stagger, 2 s limiter | `ReceiveHit` | `PowerLevel(_AI)`, `StaggerDmgModifier` |
| Knockback back and sideways with decay | `Push` | `ImpactDistanceX/Y`, `ImpactDecayRate` |
| Game camera shake | `ANWCharacter::PlayCameraShake` | `CameraShakeID`… and `DT_NWCameraShake` |
| Status effects with tick and stacks | `ApplyStatus`, `TickStatuses` | `Status*` columns |
| Aim (bow), draw, reload | `RequestAim`, `ReleaseCharge` | `bAims`, `AimFullCharge` |
| Arrow with gravity, pierce, explode | `SpawnProjectile`, `ANWProjectile` | projectile columns |
| Aimed ground area | `FireGround` | `Ground*` |
| Riposte, Evade | `ReceiveHit`, `CheckAction` | `Counter*`, `bCancelsAny`, `CooldownGroup` |
| Mana | `UNWManaComponent` | `ManaCost`, `javelindata_manacosts_player` |
| AI that uses the weapon | `UNWBrainComponent` | styles Duelist, Pack, Charger, Archer |

Compile with the editor closed (import step 01). To only check syntax with the editor open, compile the game target
(platform tooling; Linux: `tools/linux/check.sh --cpp`).

## 7. Import and test

```bash
tools/linux/ue_import_all.sh 01   # compile
tools/linux/ue_import_all.sh 02   # player set (animations, montages, sounds, tables)
```

Step `09` imports the icons. When only tables changed:

```bash
python3 tools/pesado.py UnrealEditor-Cmd ... -run=pythonscript -script="tools/ue/import_tables.py <manifest>"
```

A weapon with upper-body montages needs the `UpperBody` slot in `ABP_NW`. This is a manual step, done once per
project (it already exists if the Bow works). In `ABP_NW` (`/Game/NW/sword_shield/ABP_NW`), AnimGraph:

1. On the output of `Slot 'DefaultSlot'`, create `New Save cached pose` named **Base**; connect the Slot to it.
2. Two `Use cached pose 'Base'` nodes.
3. A `Slot` node named **UpperBody** (Details, Slot Name; if it is not listed, Window > Anim Slot Manager > Add Slot
   `UpperBody` in `DefaultGroup`). Source = one of the `Use cached pose 'Base'`.
4. A `Layered blend per bone` node: Base Pose = the other `Use cached pose 'Base'`; Blend Poses 0 = output of the
   UpperBody slot. In Details: Layer Setup > 0 > Branch Filters > + > Bone Name **Spine1**, Blend Depth 0; check
   **Mesh Space Rotation Blend**.
5. Layered blend output → `Output Pose`. Compile and save.

Without this step the bow fires but the torso does not animate.

Test in the arena (`L_Arena`, F10 > Go to the arena) with [../tests/weapons.md](../tests/weapons.md). The maintainer
runs the game and sends screenshots.

Compare with the game: record the same action in New World and in the port (same camera) and compare frame by frame
side by side. A difference found goes to the "Known differences" section of the weapon spec, with what is missing to
close it.

## 8. Where to tweak

| Symptom | Where to look |
|---|---|
| Swing chains early/late | fragment windows (report, section 1) and `window_overrides` |
| Different damage | row `DmgCoef`, `WeaponBaseDamage`, mitigation ([../game/combat.md](../game/combat.md)) |
| Target does not react / reacts too much | `PowerLevel` × target resistance, `HitStun` |
| Odd knockback | `ImpactDistanceX/Y/Z`, `ImpactDecayRate` |
| Camera still on hit | row `CameraShakeID`/`TargetCameraShakeID` and `javelindata_camerashake` |
| Hit from too far/near | `<ARMA>_SHAPES` or the weapon mesh (blade) |
| Sound out of time | fragment `Audio` windows; `extract/ue/sword_shield/sounds.json` |

## 9. Check at the end

- Every new number has a source (weapon spec or code comment).
- [../game/weapons/README.md](../game/weapons/README.md) (weapon state) and the mechanics table above are updated.
- Test items added to [../tests/weapons.md](../tests/weapons.md) or the weapon's own test file.
- Remaining differences are in the weapon spec's "Known differences".
- Polish until the "done" table in [../game/weapons/README.md](../game/weapons/README.md) is closed (animation,
  mechanics, abilities and mastery, sound, effects, HUD, network).
