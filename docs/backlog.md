# Backlog

Open work across areas, one line per item. Each area spec has the detail and its own open questions. Done items and
past rounds are in [notes/project.md](notes/project.md). Area rules and owners: [workflow.md](workflow.md).

## Movement and traversal ([game/combat.md](game/combat.md))

- Odd movement when turning the camera with the mouse while walking: reproduce and read the diagnostic line (`corpo` × `camera`, `dir`).
- Walking while blocking drags the character: add the walk-with-shield-up blend space (only the forward loop `rswrd_walk_block` exists; look for the directions in the `.adb` with the tag `Blocking`).
- Left diagonal looks wrong: in the game's clips the hips face right in almost every direction; likely the game's torso adjustment for aim (aim/look IK).
- Refine the 0° pose (`straighten_loops`); it is better but not final.
- Traversal, still to test: choice between variants, fit at the end (teleport to the computed point), network (remote client), heavy landing. Test track in `L_Arena` behind the start; `nw.DebugTraversal 1` draws the rays and logs the decision.
- Ladder (`unarmed_ladder_*`) not implemented.

## Combat and weapons ([game/combat.md](game/combat.md), [game/weapons/README.md](game/weapons/README.md))

- Heavy attack: check the charge bar clock on a remote client (it uses server time).
- Sword and Shield: Shield Rush must really push (today only the clip's root motion).
- Sword and Shield: Defiant Stance's "−20% stamina damage when blocking" and the end of its animation (`_end`).
- Hatchet: secondary effects (Vulnerable, slow, Disease, Social Distancing's invulnerable recoil, Berserk extension per hit).
- Hatchet: the hatchet disappears from the hand during the throw.
- Hatchet: reactions and death use the sword montages (shield arm).
- Hatchet: specials 4/5 were mapped to Infected Throw/Berserk by deduction; check visually.
- Unarmed locomotion `BS_Unarmed_Strafe` (`unarmed_cmbtstrafe_blend`) is in the weapons set, but `build_set.py tools/sets/sword_shield.json` stops at "blend space not found: ice_gnt_idle_rhand" (`BS_Ig_Strafe` in `weapons_ice-gauntlet.json` lists a clip as a layer). Until that is fixed and the set rebuilt, unarmed uses `BS_Hatchet_Strafe`.
- Hatchet's own locomotion (deferred): game layers `raxe_pose_add` (idle) and `v2_raxe_cmbt_walk_layer_l/r` (walking) on `melee1h_cmbt_nav_strafe_blend` are additive (`_add`); the nwbt/pipeline does not add additive layers yet. Today `BS_Hatchet_*` = one-hand base + sword arm.
- Weapon Mastery: passive and ultimate effects (they only count points today).

## UI ([game/ui/README.md](game/ui/README.md), [game/ui/original.md](game/ui/original.md))

- Import the new screen textures with the editor closed: `tools/linux/ue_import_all.sh 09` (lists `tooltip`, `lootticker2024`, `worldmap`, `levelup`, `tradeskills`, `attributes` and the `xp` screen of `hud_textures`; the 52 map tiles go in compressed, DXT1).
- Check on screen: tooltip (Tab, mouse over an item) and comparison tooltip (Inventory weapon against an equipped one).
- Check on screen: loot ticker (gather), including trade skill XP in it.
- Check on screen: map (M: drag, wheel, buttons, POIs), group, marker (right click on the map, shows on the compass), last death location.
- Check on screen: compass with POIs.
- Check on screen: XP bar and level-up banner (F10 > Gain 1 level).
- Check on screen: Attributes and "+" in the status panel; Attributes tab (K > Attributes): rune with points (mask), +/−, Confirm, XP bar.
- Check on screen: Keybindings tab (change key, Esc cancels, Restore Defaults) and auto-run (=).
- Check on screen: stacked damage numbers (Settings > Game) and Unstuck (Esc).
- Check on screen: Trade skills tab in the K window (level and ring).
- Check on screen: hit indicator (get hit by a Lost from the sides: arrow and glow on the right edge; the direction of the Canvas rotation was not seen).
- HUD: active effects above the health bar (`statuseffects/statuseffectsv2`, `hud2024/afflictions`: food and potion icons with time).
- Look: cloth (skirts, capes) follows the skin without simulation; female character and dye masks not done.
- Travel between servers: if the target map's server is down, the character stays saved on that map and returns to the login screen until it comes up.
- HUD: target bar and damage numbers in the game's style (`hud/damagenumbers`, nameplates).
- HUD: H2 (menus).
- Mastery screen: tooltip with the game's tooltip art; unlock animations (pulses); drag an ability to a slot.
- Map: tracked gatherables, NPCs, fog over undiscovered areas.
- Keybindings: keyboard and mouse only, no controller.
- Keybindings: a conflict swaps both keys; the game clears the other one instead.

## Progression and city systems ([game/city.md](game/city.md), [game/world/everfall.md](game/world/everfall.md))

- Character XP has no source: it does not come from gathering or kills in the port (the tables do not give the formula; see [game/ui/original.md](game/ui/original.md)).
- Attributes do not change damage or health yet.
- Trading Post and housing: backend ready, no screen.
- Everfall open work: [game/world/everfall.md](game/world/everfall.md) and [game/city.md](game/city.md).

## World: Windsward ([game/world/windsward.md](game/world/windsward.md))

- Ground with one texture: the game mixes dozens of layers through `*_splat` (rough, soil, cliffs...) and the per-biome material (`region.tractmap.tif`); `macro_diff` is not a color map of the region.
- Character "flying" at some spots and a floating boulder: no diagnosis yet.
- Town tree canopies show colored planes (masked foliage).
- Waterfall looks wrong (screenshot pending).
- POI creatures (boar, wolf, skeletons...), town NPCs, chests: steps G3–G5.
- Not converted (nwbt makes only static meshes and lights): 25 guild flags (`RenderGuildFlagComponent`), decals, particles (fire, smoke), water, loose region vegetation (`region.vegetation`). (Water: `tools/make_water.py` now exists; unclear in the original whether this item is closed.)
- No animation on windmills, water wheels, flags and NPCs (they come as static meshes).
- Crafting stations: the complete slice points to T5 versions (e.g. `Settlement_Station_Blacksmithing_T5`); check that no placeholder remains after the NoDraw filter.
- `.aliasasset` (variant pick): the nwbt walker includes every variant, overlapped; check whether it shows in the town.
- Player houses are separate instances in the game; here every house can be entered. Decide whether to close the doors.
- Boar and wolf: only partly fixed after the user test.

## Lighting ([game/lighting.md](game/lighting.md))

- Only sun, sky and fog; none of the game's lights (candles, fireplaces, lamp posts); interiors are dark.

## Gathering and mounts

- Gathering: [game/gathering.md](game/gathering.md). Mounts: [game/mounts.md](game/mounts.md).

## Tools ([tech/tools.md](tech/tools.md))

- `nwbt models slicestats` converts textures it does not need (because of the collector); fix before using it.
