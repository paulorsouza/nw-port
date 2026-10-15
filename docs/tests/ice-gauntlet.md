# Tests: Ice Gauntlet

Sub-area 1a of the weapons area. Map `L_Arena`. For each item, send a screenshot if something is wrong. Spec:
[../game/weapons/ice-gauntlet.md](../game/weapons/ice-gauntlet.md).

## Part 1: weapon, mana, light, heavy, block, dodge

1. Pick F10 > Ice Gauntlet + sword and press 1: the glove appears on the right arm and follows hand and fingers; crystal in the palm. Press 2 to switch to the sword (the glove disappears).
2. Click once: the ice projectile leaves at 0.167 s through the screen center while walking (only the torso animates). It disappears at 50 m.
3. Hold the click: at 0.667 s the heavy fires by itself, with a larger projectile. The target hit is 10% slower for 3 s.
4. Click repeatedly: the next shot only fires after the game window (~0.27 s after the shot, buffer up to 0.93 s).
5. Hold the right button: gauntlet block pose; blocking spends stamina like the other weapons.
6. Press Shift with each of the 4 directions: short one-hand step to each side, invulnerable at the start.
7. Check the mana bar (blue, above stamina) is full. F10 > Refill mana and Infinite mana respond.
8. With another player: confirm they see the glove on your arm and the projectiles.
9. Click nonstop: each click released before the window is kept and the next light fires when the window opens (~0.83 s); the current swing is not cut before the projectile leaves.
9b. Run (Shift held) and click: the normal light fires (the gauntlet has no sprint attack).

## Part 2: abilities

10. Use Q/R/F with the gauntlet: Ice Storm, Wind Chill and Ice Spikes (defaults). Swap to Ice Shower, Ice Pylon and Entombed on the K screen. Each spends the table's mana (15/20/25) and enters cooldown (the blue bar drains).
11. Cast Ice Spikes: 7 spikes in a ~8 m line in the camera direction and the Mighty Spike at the end (pushes). Clicking attack right after casting releases the Mighty Spike early.
12. Cast Wind Chill: 2 s of wind ahead while walking slowly; the dummy up to ~6 m takes several hits. No knockback.
13. Cast Ice Storm: blue 5 m area where the camera points (up to 20 m), 5 s, damage every 0.33 s and the enemy slower.
14. Cast Ice Shower: arc of boxes ~2–3 m ahead for 4 s; whoever enters is almost stopped for 1 s and slow while inside and for 3 s after leaving.
15. Cast Ice Pylon: it appears 2 m ahead and shoots the nearest enemy within 20 m for 15 s; another pylon removes the previous one; the 10 s cooldown starts when it disappears.
16. Cast Entombed: ice tomb for 10 s, no damage taken, no movement, mana filling; a basic attack breaks it (damage around, spends 10 mana); the 30 s cooldown starts on exit. The tomb pose holds still for the 10 s, without going back to idle midway.
