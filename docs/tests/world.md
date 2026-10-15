# Tests: world

Windsward terrain, water, scenery, creatures and creature AI, plus the combat rounds run in the same sessions.
Specs: [game/world/windsward.md](../game/world/windsward.md), [game/creatures-ai.md](../game/creatures-ai.md),
[game/combat.md](../game/combat.md). Dated context of each round: [notes/world.md](../notes/world.md).

Before: the screen textures (import step 09) and boar/wolf (import step 12) must be imported. Open the editor,
`L_SouthWindsward`, Play. For each item, send a screenshot if something is wrong (preferably with the console
`stat fps`).

## G2: terrain, water, scenery

1. ~~**Character flying.**~~ Resolved: the cause was `ChaoSeguranca` (invisible plane from `fix_city`) above the valleys; it is now 10 m below the lowest terrain. Original check: walk along the town edges and the hills outside it; where the character stays above the ground (or goes through it), send a screenshot and the position (F10 > the diagnostic HUD shows it; or `nw.HudDebug 1`) and say whether it is over terrain, rock, bridge or ruin.
2. **Floating boulder.** Look for stones and gatherables above the ground; send a screenshot and the position.
3. **Scenery collision.** Touch cliffs, ruins and fences outside the town: does the character go through or get stuck? If stuck, try Esc > Unstuck (pt-BR "Desprender"; it returns you 5 s later).
4. **Water.** Check that the river and lakes appear, at the right height (not floating, not buried), and that the waterfall is visible.
5. **Ground.** It uses a single texture (known). Report only holes or visible seams between pieces.
6. **Town trees.** Canopies show colored plates (known); report anything worse.
7. **Performance.** Note the FPS standing in the square, running out of town and looking at the valley from a hill; report hitches when turning the camera.
8. **Gathering.** F10 > Go to ore and gather it: the loot notice shows the item and "+64 Mining" with the bar. Gather a large tree: it should ask for a level (only with a server character; in the editor's Play it is free).

## New screens

9. **Inventory (Tab).** Hover an item for the tooltip; on a bag weapon with another weapon equipped, both appear side by side. The navigation bar on top (Inventory / Character / Map) switches screens.
10. **K window.** Check the tabs Weapon Mastery / Trade Skills / Attributes. F10 > Gain 1 level: level banner, XP bar at the bottom, points badge on the Character tab; in Attributes, +/− and Confirm. F10 > Trade skills +1 level: the circles of the Trade Skills tab.
11. **Map (M).** Drag, mouse wheel, buttons o/+/−, POIs with names when zoomed in, right click places a marker (it appears on the compass with the distance), player icon facing where the player looks.
12. **Compass.** Nearby POIs appear on the strip at the right bearing (compare with the map).
13. **Settings (Esc).** Keybindings tab: change a key, Esc cancels, Restore Defaults; autorun with "=". Game > Stacked damage numbers. Video > Brightness.
14. **Combat.** Get hit by a Lost from the sides and from behind: the arrow and the red glow show on the right side.
15. **Notices.** Fill the bag: a yellow "bag full" banner at the top, not in the chat.

## G3: creatures (boar and wolf)

**Manual step, once** (the rest is already imported): open `/Game/NW/Creatures/ABP_NW_Creature` (template ABP, class
NWAnimInstance) and build the AnimGraph: `Blend Space Player` → `Slot 'DefaultSlot'` → `Output Pose`. In the Blend
Space Player, expose the *Blend Space* pin and connect it to the variable **Locomotion**; X axis = **TurnRate**,
Y axis = **Speed**. Compile and save.

16. **They spawn near you.** F10 > Go to creature (puts you 25 m from a boar or wolf spawner). Boars and wolves appear in the POIs within 60 m and disappear when you move 90 m away; they walk the waypoints and stop at their tasks (grazing...).
17. **Combat.** Get close: they chase and attack (boar: gore and charge; wolf: bites and pounce), with hit indicator and damage numbers. Run far away (40 m from their home): they give up and go back.
18. **Death.** Die to one: the death screen names it (pt-BR "Javali"/"Lobo Grisalho").
19. **Skinning.** Kill one and approach the carcass: card "Esfolar Javali" (without a knife it asks for the knife). Press E: skinning animation, the game's loot (meat, hide) and "+95 Skinning" in the notice.
20. **Scale and ground.** Check the size (boar 1.4×) and that the feet touch the ground (not floating or buried); screenshot if something looks odd.

## Round after compiling

21. **Fly (F10 > Fly).** Forward follows the camera (looking up climbs), Space climbs, Ctrl descends; it goes through terrain and houses. With Fast run on it flies 4× faster. Turn it off in the air: you fall normally.
22. **Knockback.** Get hit by boars and wolves several times, also on a slope: it pushes at most a few meters, stuck to the ground, never flying.
23. **Creature sounds.** Boar: roar and whoosh on the gore, snort and hooves before the charge, pain and flesh impact when hit, death. Wolf: bite on the pounce, jump on the leap, alert when it sees you, pain, death. Getting hit by an animal no longer plays the sword-on-armor impact (only your pain voice). Creature footsteps still have no sound (known).
24. **Hit weight.** Short camera kick when you hit and when you get hit (stronger with more damage). Say whether it is too strong or too weak.

## Combat rounds

### Arena

`L_Arena` opens by itself on Play, or F10 > Go to the arena.

25. **Scenery.** Legion arena walls and stands, lit braziers, banners, rocks outside, dirt floor. Note the FPS standing in the center and looking at the wall; screenshot any piece out of place (sunk, floating, rotated) or a strange floor.
26. **Zones.** Shooting range to the south (signs 10 to 50 m and dummies), training dummies to the north, traversal course by the north wall, duel circle to the west with a Lost inside (step into the circle: it comes at you).
27. **Waves (F10 > Arena).** Boars: from afar they snort and dig before the charge (you can dodge), up close they gore. Wolf pack: they surround from the sides and attack one at a time, leap from afar. Lost: circles, comes in with strikes and backs off. Duelist: raises the shield and dodges when you attack. Archer: stays far, backs off if you approach, Evade Shot when you get close. Rapier duelist: combos, Tondo, lunge. Mixed wave.

### Game rules ([game/combat.md](../game/combat.md))

28. **Reaction.** On another player, the light sword attack does not interrupt (only flinches); the heavy one does. Dummies react to everything (resistance 0). Against Lost: a light attack sometimes does not interrupt (stagger resistance 20); after a reaction, 2 s in which light attacks do not interrupt. Against boar/wolf the light attack interrupts.
29. **Critical.** Large golden number. Hitting from behind is always critical.
30. **Knockback.** Starts strong and slows down (it used to be constant); some attacks push sideways.
31. **Camera shake.** The game's shake is very small (1–2 cm). The port's extra kick remains (console `nw.HitKick 0` turns it off; `nw.ShakeScale 2` doubles the game's). Say which is better.
32. **Stamina.** Regenerates 33/s; with the guard up, 10/s; exhausted only ends with a full bar.
33. **Animation speed.** The sword heavy attack and the spin are a bit faster (as in the game).

### Bow (weapon-specific)

These items belong to the weapons area ([tests/weapons.md](weapons.md)); kept here to preserve the numbering.

**Manual step, once (bow):** the bow shoots while walking, with upper-body-only animations. In `ABP_NW`
(`/Game/NW/sword_shield/ABP_NW`), AnimGraph:

1. At the output of `Slot 'DefaultSlot'`, create `New Save cached pose` named **Base**; connect the Slot to it.
2. Two `Use cached pose 'Base'`.
3. A `Slot` node named **UpperBody** (Details, Slot Name; if it is not in the list, Window > Anim Slot Manager > Add
   Slot `UpperBody` in `DefaultGroup`). Source = one of the `Use cached pose 'Base'`.
4. A `Layered blend per bone` node: Base Pose = the other `Use cached pose 'Base'`; Blend Poses 0 = output of the
   UpperBody Slot. In Details: Layer Setup > 0 > Branch Filters > + > Bone Name **Spine1**, Blend Depth 0; check
   **Mesh Space Rotation Blend**.
5. Output of the Layered blend → `Output Pose`. Compile and save.

Without this step the bow shoots, but the upper body does not animate (the arrow still flies).

F10 > Switch weapon pair until "Bow + Rapier"; key 1.

34. Click without aiming: quick shot. Right button: aim (camera comes close, slow walk). While aiming, hold the left button to draw; release before 0.5 s = normal shot, after = charged (stronger). After the shot it reloads by itself and returns to aim.
35. The arrow drops after ~5 m of straight flight (aim above a far target). Hit a dummy's head: more damage.
36. Abilities: Rapid Shot (3 arrows, the last one knocks back), Evade Shot (jumps back and shoots), Penetrating Shot (goes through several). Swap in K: Poison Shot (damage per second), Explosive Arrow (explodes in 2.5 m), Rain of Arrows (4 m area where the camera points).

### Rapier (weapon-specific)

Key 2 in the "Bow + Rapier" pair. These items belong to the weapons area ([tests/weapons.md](weapons.md)).

37. Combo of 3; the second strike hits twice. Hold the attack: charged thrust at 0.667 s.
38. Dodge is a short step. Q Tondo (bleeds: damage numbers every second, up to 3 stacks). R Riposte: if hit in the first second, counterattacks and stuns (the stun ends at the first damage). F Evade: quick step that leaves any action; attacking right after gives a quick strike.
39. In K, swap to Flourish and Finish: after Flourish, attacking does Finish, which consumes the bleed (large number). Flurry: 5 thrusts. Fleche: long advance.

### Fixes after the first test

Items 40–42 are bow-specific (weapons area).

40. Bow aiming: the second arrow draws while holding the button (draw starts at 0.733 s of the reload, as in the game); a click during the reload is buffered and fires at 0.667 s, no double click needed. A click right as the bow is raised also fires.
41. Bow without aiming, button held: rapid shots in a row (1 per second).
42. Shield: with the bow + rapier pair it disappears; with a sword in the pair, it comes back. The stowed bow goes on the back (screenshot if crooked).
43. Reactions by hit power: a light hit gives a small flinch (R1), a heavy one a bigger flinch (R3), power-4 hits knock down and the character gets up. Abilities chain in the game's windows (e.g. Shield Bash right after the combo only when the game allows it).
44. An attack "pulls" toward the enemy in front (up to ~2.75 m, 45° cone) during the homing window.

### Second bow fix (weapon-specific)

45. Aim → draw → release → reload → aim: the bow does not lower between them (chaining starts when the previous one starts to exit; 0.1 s transitions). Raising the bow no longer blocks the draw.
46. Arrow in the right hand while aiming and drawing; it disappears on release and comes back on reload.
47. Arrow in flight with a 2.5 m glowing trail (in the game it is a particle).
48. Abilities with effects: Explosive Arrow (orange flash), Poison Shot (green 3 m cloud for 6 s that poisons whoever enters), Rain of Arrows (arrows falling in the area before the hit).

## Creature AI from the game's trees

Wolf and boar follow the game's behavior tree ([game/creatures-ai.md](../game/creatures-ai.md)). Arena: F10 > Arena >
Wolf pack / Boars (they spawn already in combat). Windsward: F10 > Go to creature.

49. The wolf sees you only from the front and up to 12 m; when it sees you it growls (alert) and only then attacks. Other wolves within 7.5 m join.
50. Wolf: from afar it runs very fast (9 m/s) and pounces or runs and bites; at mid distance it sometimes strafes to reposition; up close it bites, does a two-bite combo, or jumps back and pounces.
51. Boar: passive. Walk past it and it does not attack; hit it and it (and the boars within 20 m) comes. Charge with wind-up.
52. Run far away (70 m from their home): they run back and recover health.
53. Clear sky on every map: no large shapes in the air and no shadow of them on the ground. The armor's cape and skirt stay on the character's body.
54. Attacks without a pause on hit (the game has no hit stop; the `nw.HitKick` camera kick remains). A hit reaction lasts 0.2 s + HitStun/30 and the animation ends with it, faster on light attacks. Knockback with reaction: fast at the start, stops when the reaction ends.
