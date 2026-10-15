# Tests: systems and Everfall

Systems area ([game/city.md](../game/city.md), [game/crafting.md](../game/crafting.md),
[game/items.md](../game/items.md), [game/economy.md](../game/economy.md)). Map `L_QuedaEterna` (Windsward for what
already lives there). For each item, take a screenshot if something is wrong. How checklists work:
[README.md](README.md). UI text is quoted from the en-US client; the pt-BR client shows the Portuguese strings.
Testing with the backend: [tech/backend-testing.md](../tech/backend-testing.md).

## City

1. Open the editor on `L_QuedaEterna`: the Everfall capital shows complete, textured and on the terrain, with no gray or floating pieces.
2. Play (PIE): the character spawns on a clear city street (not inside a statue or wall), walks the streets and climbs stairs without falling off the map.

## NPCs

3. In `L_QuedaEterna`, Play: the city NPCs show (magistrate, commander, alchemist, barkeep, guards, ambient NPCs), standing on the ground, dressed and textured, each looping its idle animation.
4. Walk into an NPC: it blocks the way like a person (you cannot pass through).
5. No name shows over NPCs. Only the drawn NPC of the week for each trade has the game's "!" icon (at most 6 in the city, one per trade); the others have no icon.
6. Get ~2 m from a named NPC, facing it: the game's card shows (ring, name strip and "[E] Talk"); press E: the NPC turns to the character. Near a gatherable with no NPC in focus, the card and E stay the gathering ones.
7. When the editor opens, the Message Log no longer shows "SK_NpcQuedaEterna_Skeleton is missing bones" (Ou Shen).
8. Go to an NPC with "!", press E: the card says "[E] Claim"; the trade's starting items show in the loot notice (example, guard: ~1000 ore and 500 stone). Press E again: the weekly gift (one material and one reagent, 100 to 300). After that the icon turns into a clock and the card goes back to "[E] Talk".
9. Log out and back in on the dedicated server with backend: the NPC already claimed still has the clock and does not give again.

## Refining

10. F10 > "Dev bag (weight)": inventory weight changes to ".../ 10000000"; again, it goes back to 1000.
11. The 5 city refining stations (Smelter, Woodshop, Tannery, Loom, Stonecutting Table): near each one the card shows the game name ("Smelter Tier 5"...) and "[E] Refine".
12. Press E at the Smelter: the refining screen opens (list on the left, panel on the right). With iron ore in the bag, "Iron Ingot" shows how many can be made; pick it, set the quantity (−, +, Max) and Refine: the ore goes (4 per ingot), the ingots appear, coins drop by the fee and Smelting XP rises.
13. Recipe without material: the numbers turn red and the Refine button is dimmed. Esc closes the screen.
14. With any screen open (inventory, K window, map, refining), click buttons, tabs and the navigation bar: the character does not attack. This includes the click that switches or closes the screen.

## Level tutorials

15. Everfall no longer has the 4 training dummies in the square. Defeat 10 enemies with the sword (Lost from F10): message "Mission complete: Sword and Shield...", 2 levels up (banner). F10 > Missions shows 1/13.
16. Switch to the hatchet and defeat 10: 2 more levels. Gather 10 times in one trade skill (Windsward): 2 more.
17. Refine 1 item at each Everfall station: 1 level per station.
18. Log out and back in on the server with backend: mission progress stays (F10 > Missions).

## Storage

19. Everfall storage shed (near the square): card "Storage Shed · [E] Open". E opens the screen with bag and storage.
20. Click an item in the bag: the stack goes to storage and the weight on both sides changes. Click it in storage: it comes back.
21. Fill storage above 1000 kg: "storage full" warning and the item stays in the bag.
22. With the dedicated server and backend: store, log out, log in and open storage: the items are still there.

## Refining screen

23. Refining screen (Smelter) with the game layout: title bar "SMELTER TIER 5" with coins; tree Learned > Refined Resources > Metals (clicking the header collapses/expands); center panel with tier, icon, name, "have/need"; right panel with trade skill level, bonus item chance, quantity (−/+/number), green/red requirements, animated blue CRAFT button, fee and "+N Smelting"; footer ENTER/SPACE/ESC. Compare with a screenshot of the game.
24. Craft: the background darkens, the total counts up from 0 in ~1 s, Base and Bonus show; a click returns to the list.
25. Refining: hold and drag the quantity bar marker: the number follows the mouse. Hold + or −: after half a second it repeats, and speeds up after ~2 s.
26. Refining: Space on the selection sets the max quantity; Space during the count skips to the end; Space on the result returns to the list. The character does not jump.
27. Refining: leave the screen open ~10 s looking at the CRAFT button: the blue shine runs smoothly, with no flicker.
28. Refining at the Smelter: the tree starts with Refined Resources > Metals, Precious Metals, Charcoal, Prospecting; Material Conversion comes after. The title reads "SMELTER TIER 5" (in pt-BR, with uppercase Ç and Ã).
29. Refining: below CRAFT the screen shows "Everfall fee: 0.26" for 26 ingots and the XP as "+N Smelting". Coins in the title bar, inventory and loot notice show two decimals (50 coins of a new character = 0.50).
30. Refine 1 Iron Ingot: the screen shows "+44 Smelting" (11 × 4 ores) and Smelting XP rises 44 in the K window.
31. Refine a recipe for the first time (example: Steel Ingot): the screen shows tripled XP and Smelting rises by that. The second time it is back to normal. Log out and in with backend: still normal (the recipe is marked as done).

## Shop

32. Inventory and loot: T1–T4 potions no longer exist; a new character starts with 5 T5 health potions and the Lost sometimes drops 1 T5 potion. An old character that had T1–T4 logs in without them.
33. In Everfall, the merchant (ambient merchant NPC) shows the card "Shop · [E] Buy". E opens the shop grid: T5 health potion (2.00) and the 5 refining reagents (0.20).
34. Click a card to open the detail; drag the quantity and click the button with the price: coins drop, the item shows in the loot notice and in the bag. The X returns to the grid; Esc closes.
35. Buy 100 of a reagent: the card gets the red clock with the time until Tuesday 05:00 (BRT), and the detail shows "Next cooldown". Log out and in with backend: the limit stays.
36. Without enough coins: the button is gray and "Insufficient funds" shows.

## Dev menu, lighting and NPC details

37. F10: the menu shows in two columns, fully on screen (Extras and Debug in the right column, scrolling if needed). Extras > "Get 100.00 coins": coins rise by 100.00 and the button shows the balance.
38. In Everfall, enter the church or the tavern: in 1 to 2 s the interior brightens (eye adaptation); on leaving, the street returns to its earlier brightness, not blown out.
39. The merchant has the shop icon over its head, visible from far away and through walls.
40. Talk to an NPC (E): it turns to face the character, not sideways. Standing, before talking, each NPC keeps the same pose and facing as before.

## Inventory

41. Tab: the bag shows the game sections in two columns (square and round tiles, tier, quantity). The 4 sort buttons reorder; clicking the active one again reverses it. The arrow collapses the section.
42. Drag a weapon from the bag to its slot (or anywhere on the equipment side) to equip; drag from equipment to the bag to unequip. Compatible slots light up during the drag.
43. In storage: drag between bag and storage, Shift + click, the section arrow (moves the whole section) and "Take all".
44. Ctrl + click on a stack: the dialog starts at half; Move/Take moves only the chosen part; without storage, Drop asks for confirmation and destroys only the chosen part.
45. Drag from the bag to "Drop items" (on the right, without storage open): drops the stack in a bag on the ground (test 72).
46. Search: typing in the field filters the list by name, ignoring accents ("fundicao" finds Fundição in pt-BR); clicking outside returns the keyboard to the game.

## Crafting

47. Forge, Workshop and Arcane Repository open the screen; a weapon or tool recipe shows the Gear score and Quality range (or the training notice) and the Craft button, without quantity.
48. Below level 50 of the trade skill, Craft is training: it spends material, gives XP and makes no item.
49. F10 "+50 levels in crafting skills" and "Crafting materials", Craft: material and fee go and the item shows at once in the bag. Tooltip with attribute, quality, perks with value and icon, empty gem slot (socket frame), "Binds on equip" and "Crafted by" when quality is above 80%. Bag full: it does not craft and warns (unclear in the original: test 92 expects the item in a ground bag).
50. Equip the crafted item: the tooltip changes to "Bound". Log out and in: the item stays the same.
51. At the Forge, "Improve items" (top corner): lists the crafted items in the bag made there, with GS, attribute and quality; buttons Reroll attribute, Refine GS and Unbind (only if bound), with the material icon and "cost / have" below (red if short). Clicking opens the result screen with the item after the change. Each one changes only its own part; Refine GS counts from the old GS to the new one, inside the recipe range (up or down).
52. Weapon and tool recipes need only ingot, timber and leather of the item's tier.
53. Craft a weapon or tool from level 50 on: the game's result screen: GS counting up to the item's, rarity, base damage, attribute, perks one by one with icon and the empty gem slot; name in the rarity color with the quality below. A click during the animation finishes it; another click returns to the station. The item has at most 1 ability perk.
54. Item with quality 80% or higher: "Crafted by <nick>"; log out and in, store and withdraw from storage: it stays the same (the serial number goes with it and nw-core does not accept the same item in two places).

## Gems

55. F10 "Gems and Jewelcrafting 200": at the Outfitting station the list has Cut Gems; cutting a Cut Flawed Onyx spends the raw gem and the reagent and gives Jewelcrafting XP. The Forge shows the 2 runeglass capsules; the Stonecutting Table, the 14 runeglasses (capsule + Pristine gem).
56. Tooltip of a gem in the bag: the port effects (example: Brilliant Diamond "+2% physical resistance", "+2% magic resistance") and "Fits armor and shields" or "Fits weapons", without the game description.
57. Drag an Emerald onto a crafted sword with no gem: "Attach Gem" with the effect; Confirm takes 1 from the stack and the sword tooltip shows the icon, name and "+5% critical hit chance". Onyx on a sword opens nothing; a gem on a tool or a looted item does not either.
58. Drag another gem onto the sword that already has one: nothing opens and it warns "Remove the gem first". The sword thumbnail in the bag and in equipment shows the gem icon in the bottom-right corner.
59. Shift + right-click on the sword with a gem: "Remove gem" with the cost; Confirm returns the gem and the slot is empty. With a full bag, it warns and nothing changes.
60. Onyx Punishment runeglass on a shield: tooltip with +5% physical resistance and +3% physical damage. Log out and in, store and withdraw: the gem stays on the item. An equipped item also accepts a gem.

## Play with backend

61. With `tools/linux/backend_dev.sh` running, Play: the log says "Playing with backend: Exilado (editor)". Pick up items, walk, stop Play and Play again: same bag, same levels and same position. With nw-core stopped, "Playing without backend" shows and Play continues with the starting kit.

## Crafting screen

62. Open "Improve items" at the Forge, leave and open a refining station: it opens on the recipe list. Back at the Forge: it also opens on the recipes.
63. Station recipe list: the last visible row ends whole above the bottom border; scrolling shows the rest.
64. Craft a weapon: at the end of the roll, Continue to Craft, Equip and Exit Station show on the left. Equip puts the item in its slot and returns to the station; Exit closes the station; Continue (or clicking outside) returns to the list. A tool shows "Equip Tool".
65. Sounds: opening and closing the station, clicking a recipe and Craft/Refine play the game sound; the weapon roll plays the rarity, one sound per perk and the gem one; refining plays the complete sound. Clicking to skip does not fire them all at once.

## Armor and jewelry

66. F10 "Armor and jewelry (levels and materials)": the Forge shows plate armor; the Outfitting station shows leather and cloth armor and jewelry. Each recipe needs only ingot, leather and cloth of the tier (jewelry: one precious ingot).
67. Craft an armor piece: result screen with the Resistance panel (Physical and Magic, base → item value), attribute, 3 armor perks and the gem slot; jewelry with no slot and a ring with 2 perks.
68. Equip each piece and each jewel in its slot (drag, right-click or Equip at the end of crafting). Tooltip with resistance and comparison with the equipped one; panel GS changes. Log out and in: everything stays equipped.
69. Armor gems (Onyx, Opal, Diamond, Sapphire, Jasper) fit armor; weapon gems do not.

## Bag and ground

70. Bag opened with Tab: the mouse wheel scrolls the list.
71. The bag accepts up to 500 stacks; weight gives encumbrance. With all 500 slots used, crafting, gathering and F10 keep working: what does not fit shows in a bag on the ground, with the message "Bag full: the item was dropped on the ground".
72. Drag a stack to "Drop items": it leaves the bag and a sack shows at the feet with the card "N items" and [E] Take all. E returns everything to the bag. Ctrl + click > Drop drops only the chosen part.
73. The sack disappears 3 minutes after it was dropped. Dropping more than 1000 kg in total is refused with "You already have 1000 kg of items on the ground".
74. Crafting station > Improve items: each item shows the recipe materials (ingot, timber, leather) with the quantity in the bag; clicking one changes the icon and the cost below the buttons. Without enough material, the cost is red.
75. "Refine GS" button with only "GS a–b" below. Refine several times: it goes up and down about half the time each, almost always 1 or 2 points, never outside a–b. Below Reroll attribute: "25% each".
76. Drop items on the ground and press [E] on the sack ("Open"): the inventory opens with the "Dropped Items" window on the right, bag icon, 3 minute warning, "N items", weight / 1000 and the list of dropped items.
77. In the sack window: dragging an item to the inventory, right-click and Ctrl + click > Take with part of the stack move only that; the section arrow takes the section; Take All takes the rest. Dragging from the bag to the sack window does nothing.
78. Empty the sack with the screen open: the window closes and only the inventory remains; the sack disappears from the ground. A forgotten sack disappears in 3 minutes.

## Shared storage

79. A new character enters Everfall: message "City discovered: storage +1000 kg". Open storage: weight / 1000. Log out and in: still known, same capacity.
80. Store items at one storage shed and open the other storage shed of the city: same items (shared storage). Items stored before this version show in Tab 1.
81. Tabs: 5 tabs below the search. Switching tabs shows only its items; storing goes to the open tab; dragging from the bag to another tab stores there; dragging from storage to another tab moves the stack. Take All takes only the open tab.
82. Double-click the open tab: a text field; type a name and Enter. Log out and in: the name stays.
83. Weight above capacity (example: through F10 if available, or a lower capacity): it does not store ("storage full"), withdrawing works.

## Recorded operations

84. Play, store and withdraw from storage, craft an item, stop Play. `tools/item_log.py --character Exilado`: each action shows with the reason (storage, craft:<recipe>), the location (bag, storage:<tab>) and the +/− quantity.
85. Stand still 10 s in Play: the log gets no new lines (the periodic save writes only the difference).
86. Stop Play right after dropping an item on the ground (before 5 s): on return, the item is neither in the bag nor on the ground (the ground was not saved before stage 2; see test 89); the log shows the bag removal with the save reason.
87. Editor Play: F10 opens and its buttons work; each use shows in `tools/item_log.py --character Exilado` with the reason `dev:<command>` (when it changes items or coins).
88. Network test with an account without admin: F10 does not open; after `tools/linux/network.sh admin <account>` and logging in again, it opens.
89. Drop an item on the ground, stop Play and Play again: the sack is in the same place, with the items and the time left; picking up returns them to the bag. `tools/item_log.py --character Exilado`: `drop` (bag −, ground:<sack> +) and `ground_take` (ground −, bag +).
90. Drop and wait the 3 minutes with Play running: the sack disappears; stop and come back: it does not return (log: `ground_expired`).
91. With nw-core turned off in the middle of Play (`tools/linux/backend_dev.sh down`): crafting, storing and dropping show "Could not save; nothing changed" and nothing leaves the bag; turn it on again (`tools/linux/backend_dev.sh`) and repeat: it works.
92. Craft with a full bag: the item goes to a sack on the ground, which comes back after stopping and pressing Play again.
93. Craft, refine and improve several times in a row, fast: everything goes in order, with no extra or missing item (compare with the log).
94. Gather some ore and kill a creature: the item and the loot notice show as usual; the log has `gather` and `loot:<table>` with only what entered (no rewrite of the bag).
95. Stand still 10 s: the periodic save writes nothing for the bag (no new line in the log).
96. Buy at the merchant's shop, socket and remove a gem, destroy an item: each one shows in the log (`shop:...`, `socket`, `unsocket`, `destroy`).
97. Claim an NPC's weekly gift: the items enter and the log shows `npc_reward:...`; try again: nothing enters.

## Trading Post

98. Everfall: [E] at the Trading Post opens the screen (spinner until the book arrives) with Buy, Sell and "My Orders n/50". Esc or the X in the corner close it. Coins and prices with the coin icon. Mouse over an item name in the table: frame with attribute, quality and perks. Away from the post, nothing opens.
99. Sell: only crafted items, cut gems and final refined materials (Asmodeum and the other T5.1) show; ore and ingots do not. Pick an item: panel with attribute, quality, perks with value; the price field is prefilled with the lowest listed price, if any. Price, duration, Enter or Place Sell Order: the item leaves the list at once and the order shows in My Orders with a spinner, which goes away when the server writes.
100. My Orders > Cancel: the row dims with a spinner and goes away; the item returns to the shared storage.
101. Buy (with 2 players, `tools/linux/net_test.sh` or two accounts): A's sell order shows for B with A's name under Seller; buy: B's coins drop at once, the item goes to B's storage; A reopens the post and receives.
102. Filters: tier, rarity, minimum GS, attribute, perk and Can Afford narrow the list; Clear All resets; clicking a column title sorts.
103. Refusal: sell with nw-core off (`tools/linux/backend_dev.sh down`): the row shows, goes away and the item returns to the list with the row flashing red and the message.
104. Create 50 orders: the sell button is dimmed at the 50th.
105. `tools/item_log.py --character Exilado`: `market_post_sell`, `market_buy` and `market_credit` with the item and coins of each action.

## City governance

Rules: [game/economy.md](../game/economy.md).

106. Everfall: [E] at the Town Project Board opens the screen with Sell to Town, Buy and Town; at the Territory Planning Board, without a governor, Summary and Government with the city price.
107. Sell to Town: pick iron ore, quantity, Sell: the ore leaves the bag, coins arrive at once and the weekly limit rises; the spinner goes away when nw-core writes. Ask for more than the limit (5,000): refused.
108. Buy: a potion or flux from the list; coins drop and the item enters the bag.
109. Government > Buy the Town (with enough coins, F10 gives coins): you become governor; the Territory Planning Board shows Summary, Prices, Stations, Defense and Government.
110. Prices: change the iron ore price and the tax, Save; on the Town Project Board the new price shows. A price out of range turns red and does not save.
111. Stations: with material in stock (sold to the city), Upgrade the Smelter: level 1. Buff +10% trade XP at the Smelter: shows under Town; refining at the Smelter gives more XP.
112. Craft at a station with a level: GS and minimum quality slightly higher; coins drop by the cost + the city tax, and the treasury rises by the tax.
113. Defense: Buy Weapon adds a weapon and subtracts it from the treasury.
114. `tools/item_log.py --character Exilado`: `city_sell:*`, `city_buy:*`, `city_buy_city` with items and coins.
115. An NPC's weekly gift: besides the material, 200 to 400 coins.
116. Government, as governor after the grace period (or with an old `owner_since` in the database): starting bid and Start Auction: "Auction" shows, ending in 24 h; with no bid, Cancel Auction.
117. With another account (`tools/linux/net_test.sh`): Place Bid at the minimum: coins drop; another account outbids: the first gets the bid back (credit when opening the board). At the end (or `POST /internal/cities/advance` with `now` 25 h later), the highest bidder becomes governor and the old one receives the bid minus 10%.
118. Territory Planning Board: the top shows the city treasury (large coin, amber); Summary without the weekly target; the player's coins show only in Government (buy the town, bid).
119. Stations: each station shows its tier (starts at 2) and the basket for the next one (stock / needed, red if short). With the stock complete, Upgrade: the station is "Under construction" with the time; after 24 h (or `POST /internal/cities/advance` with `now`), it rises a tier. At tier 5, the next one is "Upgrade to Level 1", much more expensive.
120. Craft at a station with tier 3+: GS and minimum quality slightly higher (tier 3: +1 GS; 5: +3; each level +1).
121. Defense: five fort weapons (Ballista, Repeater, Explosive, Burning Oil Vat, Horn of Resilience), tier 1 to 3, basket and 24 h construction; defense is the sum of the tiers.
