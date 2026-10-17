# Tests: admin page

Page and API: [tech/admin.md](../tech/admin.md). Editor Play on `L_SouthWindsward` (creatures nearby) and
`L_Arena` (arena fighters). Before each session, type `nw.admin` in the console and open http://127.0.0.1:8780 in the
browser. How checklists work: [README.md](README.md).

## Page

1. `nw.admin` in the console: the log says "Admin page on http://127.0.0.1:8780/"; the page opens with the map name, your character in the list and on the map.
2. Stop Play and start it again: the page reconnects by itself, with no new `nw.admin`.
3. `curl -H 'Origin: http://example.com' 127.0.0.1:8780/api/state`: 403 "local only".

## Player

4. Coins "+1,000": the coins in the game bag rise by 1,000.00; leave and enter again: they are still there.
5. Level 13 > Set: XP bar and level jump to 13, with the attribute points of the levels.
6. Unlimited bag: the inventory max weight becomes 10,000,000; again, back to normal.
7. Each F10 command on the page does the same as in the F10 menu (e.g. "Arena Wolves": three wolves around the character).
8. Click on the map: the character walks to the spot by itself and stops; you can take over with the keys at any time.
9. Light, Heavy, Q, Dodge, Jump: the character does the action with the same animation as the key.
10. With a second player connected (network test), drive that player from the page: they walk and attack on their screen and on yours.

## Bots

11. Bots ×3 "follow" with your character as reference: three characters appear around you and follow you.
12. Bot > Fight with a wolf as target (right click): the bot goes to the wolf and attacks until one of them dies.
13. Bot > Weapon "Bow": the bot holds the bow; Light shoots an arrow.
14. Remove summoned: bots and summoned creatures disappear; the map's creatures and the players stay.

## Creatures and AI

15. Summon "Wolf_Grey_8" ×2 at 12 m with aggro: two wolves appear in front and attack right away.
16. Wolf > Freeze: it stops where it is and does not attack; again: it goes back to fighting.
17. Wolf > Passive: drops the target and does not attack even when you walk up to it; hitting it does not make it fight.
18. Wolf > Attack target with a bot as target: the wolf leaves you and goes after the bot (dashed red line on the map).
19. Running path changes as the wolf moves from wandering to combat (attack names on the last line).
20. Blackboard "Difficulty_Tier" = 0: the value turns orange and the wolf stops pouncing and combos; × releases it.
21. Vision × 0: an idle wolf does not see you walking past it; × 4: it sees you from far away.
22. `L_Arena`, F10 "Arena Duelist", then on the page: style "Archer": it backs off and keeps distance; Block 1: it blocks almost every hit.
23. Pause on a creature: it stops mid-animation; again, it moves on.
24. Time 0.3×: the whole server slows down (everyone); back to 1.0×.
