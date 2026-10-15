# Tests: weapons

Weapons area ([../game/weapons/README.md](../game/weapons/README.md)). Map `L_Arena` (F10 > Arena; weapons
from F10 > Test items, equipped in Weapon 1/2 in the inventory). For each item, send a screenshot if something is wrong. How test checklists work: [README.md](README.md).
Ice Gauntlet: [ice-gauntlet.md](ice-gauntlet.md).

## Arena test for any weapon

Go to `L_Arena` (F10 > Go to the arena). Turn on hit display with F10 > Show hits (`nw.DebugHits 1`) when checking
windows and hit shapes.

1. Play each action: right animation, no sliding, weapon in the right hand.
2. Check the windows: the combo only chains inside the window; dodge cancels where the game allows; the hit only
   lands during the damage span.
3. Check the numbers: damage on the training dummy = base × the row's DmgCoef (dummy without armor); stamina and
   cooldown match the weapon report.
4. Check the reactions: stun, knockback (distance and direction), camera shake for attacker and target.
5. Listen for the sounds at the right times.
6. Fight the AI: F10 > Arena > Duelist / Pack / Boars / Mixed wave.

Compare with the game: record the same action in New World and in the port (same camera) and compare frame by frame.
Write each difference in the weapon spec under "Known differences".
