# Items

Port rules for weapons, armor, jewelry and tools. They do not follow the original game. Crafting, stations and
refining: [crafting.md](crafting.md). Trading Post rules for items: [city.md](city.md#trading-post).

Perk decisions: `tools/perks_decisoes.json`, collection `decisoes`, one entry per `PerkID` with the fields `v`
(`fica` = keep, `sai` = drop), `nova` (the port perk that absorbs this one) and `nota` (effect change). The file is an
export of the maintainer's "perk cut" page.

## Damage and defense

- Two damage types: physical and magic. There is no elemental damage.
- Empowered, Fortify, Haste and perk debuffs do not exist.
- Support is healing and shields. Shield = % × max health of the caster, with the duration and stacks of the original
  effect.
- Threat belongs to the tank class, not to items.
- Luck exists only on trophies.

## Attribute and gear score

- Each item has one attribute (STR, DEX, INT, FOC or CON). No split attribute, no Maxify.
- The attribute is rolled when the item is crafted. It can be rerolled at the station of the item's trade skill, paying
  trade materials. Only the attribute changes; GS, quality and perks stay.
- GS changes only that attribute. Example: an iron item at GS 800 gives CON 40.
- GS on craft: rolled in the material's range, plus the crafter bonus.
- Rerolling GS changes only GS and the attribute. Quality, perks, damage and resistance do not change.
- Refine GS: 51% success (`chance_sucesso_gs`). Success raises GS by 1 up to the up-cap; failure lowers it by 1 down
  to the down-cap. In both cases 1 is more likely than 2 and so on (weights N, N−1, ..., 1). GS never leaves the
  crafting range (material + crafter bonus by level, cap `gs_maximo`, provisional 800). The caps shrink as GS rises
  above the range minimum (provisional, `tools/itens.json` `passo_gs`):

  | GS above the range minimum | Up to | Down to |
  |---|---|---|
  | up to +10 | +15 | −10 |
  | up to +25 | +10 | −10 |
  | above +25 | +10 | −20 |

  Done at the station of the item's trade skill, "Improve items" tab, paying trade materials.
- Improvement material: the player picks one of the recipe materials on the screen (ingot, timber, leather of the
  tier; `materiais_melhoria`). Each material can have its own `passo_gs` (today all use the same one). The screen shows
  only the minimum and maximum possible GS (not the success chance). For the attribute, it shows the chance of each of
  the other attributes.

## Quality

- One value from 0 to 100% for the whole item.
- It places every value of the item in its range: value = minimum + (maximum − minimum) × quality. Example: resistance
  500–520 at 10% gives 502.
- Values have one decimal place.
- Rolled from the minimum quality up to 100%. The minimum rises with the crafter: trade skill at max, 20%; trade
  skill, trade clothing and trophies at max, 50%.
- Tooltip shows: attribute, resistances, quality, perks.

## Material

- The material sets only the base resistance (example: leather 200–220).
- The material does not change perks.

## Perks

- Each perk has a fixed range (example: magic amplification 6–10%), independent of GS and material.
- 3 perks per item; ring, 2. No repeats; at most 1 ability perk per item.
- An ability perk only drops on the weapon of that ability.
- Cooldown reduction has no cap: cooldown = base ÷ (1 + total). 100% halves the cooldown.
- Critical chance is capped at 50% (the game has no critical resistance). Critical damage has no cap.

| Equipment | Perks |
|---|---|
| Weapon | Critical chance, Critical damage, Cooldown reduction, Lifesteal, weapon ability perks; Life Staff and Void Gauntlet also Healing done |
| Shield | Critical chance, Critical damage, Cooldown reduction, Stamina regeneration, Lifesteal, shield ability perks |
| Armor | Physical amplification, Magic amplification, Max health, Cooldown reduction, Stamina regeneration, Lifesteal |
| Amulet | Max health, Cooldown reduction, Stamina regeneration, Fortified Recovery, Refreshing Recovery |
| Ring | Critical chance, Critical damage, Healing done, Healing Breeze, Stamina regeneration |
| Earring | Cooldown reduction, Stamina regeneration, Fortifying Toast, Regenerating |
| Tool | Gathering speed, yield and experience |

Named perks use the game's en-US names (`perkid_amulet_fortifyonlow`, `perkid_amulet_lowcooldown`,
`perkid_ring_onhealhot`, `perkid_earring_onpotionfort`, `perkid_earring_healthregen`). Ability perks with a changed effect (no
Empowered, Fortify, speed or debuff): field `nota` in `tools/perks_decisoes.json`.

## Armor and jewelry

### Rules

- Lines: plate (heavy), leather (medium) and cloth (light). 5 pieces (head, chest, hands, legs, feet), T2–T5. The
  game's "dress" line is left out.
- Material is only refined material of the tier, as with weapons: plate = ingot + leather + cloth; leather = leather +
  cloth + ingot; cloth = cloth + leather + ingot (quantities from the game recipe).
- Jewelry (amulet, ring, earring) is made straight from the precious ingot, with no components: silver T2, gold T3,
  platinum T4, orichalcum T5, in the summed quantity of the game recipe.
- Physical and magic resistance are equal (the variation comes from gems), placed by quality in the material range.
  Chest: plate 350–385, leather 193–212, cloth 106–117 (game factors 0.875 / 0.481 / 0.265 × 400). Legs 57%, head,
  hands and feet 43% of the chest (game fractions 0.20 and 0.15 over 0.35). Provisional (balancing).
- Jewelry has no resistance.

### Game data

Recipes:

- Armoring (`javelindata_crafting_armorer`, trade skill `Armoring`): 80 recipes T2–T5 (level 0/50/100/150), 5 pieces
  (head, chest, hands, legs, feet) in 4 lines:
  - plate (`Heavy*_Plate`): forge (`blacksmith2`), metal + leather + cloth;
  - leather (`Medium*_Leather`): outfitting station (`outfitting2`), leather + cloth + metal;
  - cloth (`Light*_Cloth`) and dress (`Light*_Dress`): outfitting station, cloth + leather + metal.
- Quantities of the main material (chest / head / hands / legs / feet): 16 / 8 / 4 / 10 / 5 (plate), 22 / 11 / 4 / 10
  / 5 (leather and cloth).
- Jewelcrafting (`javelindata_crafting_jeweler`, `Jewelcrafting`): amulet, ring and earring T2–T5. Each one needs a
  component (chain, band, hook; made of precious ingot), a setting and precious metal. Silver (T1), gold (T2), platinum
  (T3) and orichalcum (`IngotT5`) at T5.

Item:

- `javelindata_itemdefinitions_master_crafting`: `ItemClass` (`EquippableChest+Armor+Heavy`), GS 200–300 ... 500–600,
  `ItemStatsRef`, `ArmorAppearanceM/F`, weight.
- Resistance (`javelindata_itemdefinitions_armor`): in the game it comes from GS × slot fraction × class factor.
  - Slot fraction (`ArmorRatingScaleFactor`): chest 0.35, legs 0.20, head, hands and feet 0.15.
  - Class factor (`*ArmorSetScaleFactor`, Balanced variant): heavy 0.875, medium 0.481, light 0.265. The
    Physical/Elemental variants change the physical/magic ratio (example: heavy 1.05 / 0.70).
  - Piece weight (`WeightOverride`): heavy chest 110.6, medium 62.4, light 35.2.
- Jewelry: `TrinketBonus` and scaling of health (amulet), damage (ring) and cooldown (earring) by GS; no resistance and
  no appearance (icon only).

Appearance:

- `javelindata_itemappearancedefinitions`: per piece its own `.skin` (`Skin1`), material, icon (`IconPath`; armor has
  no icon on the item) and the flags `HideHead`, `HideHair`, `HideFacialHair`, `HideLegs`, `HairChop`.
- The 80 recipes use 80 distinct meshes (male only in the port): conqueror T1–T3 and metal T5 (plate), military,
  leather T1/T2 and set A T5 (leather), cloth T1/T2/T4/T5, basic dress and "feminine light" (dress).
- All pieces use the player skeleton (`player_male.chr`, 148 bones).

### Implementation (mechanics, no visuals)

- Recipes: `tools/make_crafting.py` (`CRAFT_GEAR` with `(Plate|Leather|Cloth)(Head|Chest|Hands|Legs|Feet)` and
  `Amulet|Ring|Earring`; fixed material of the tier; jewelry sums the ingot of the components). Plate at the forge;
  leather, cloth and jewelry at the outfitting station. Armoring trade skill = index 13 (`NWTradeskill`, icon
  `icon_tradeskill_outfitting`).
- Catalog: `tools/make_items.py` gives armor and jewelry the slot (`set` Head…Earring), the weight class (`armor`)
  and the appearance icon.
- `NWEquip` slots 7–14 (head, chest, hands, legs, feet, amulet, ring, earring) in the inventory panel, at the game's
  positions; `NWEquip::IsTool` separates tools. The panel's average GS counts weapons, armor and jewelry. nw-core has
  `MAX_EQUIP_SLOTS = 16`, so indexes 7–15 were free without a migration.
- Rules: `FNWItemRules::EquipClass` (armor, amulet, ring, earring), `Resistance` (`tools/itens.json`
  `resistencia_peito`, `resistencia_slot`); tooltip with physical and magic resistance; rarity is rolled (not set by
  tier).
- Result screen: `Stats/Armor` panel (base at quality 0 → item value, Physical and Magic) and `Stats/Trinket`.
- Dev (F10): "Armor and jewelry (levels and materials)".
- Captures: `tools/ui_shots/armaduras.json` (recipe and rolls) and `tools/ui_shots/vestuario.json` (panel and
  tooltips).
- The character still has one mesh (`SK_player_male`) with the heroicheavy armor merged in Blender (`tools/nwchar.py`
  `attach_armor`, `tools/export_ue.py`). There is no armor component in C++.

## Slot and gems

- 1 slot per weapon, shield and armor piece. Jewelry has no slot.
- Gems: the game's 4 cuts (Flawed, Cut, Brilliant, Pristine; item tier 2–5) give 2, 3, 4 and 5%. There is no 1%
  gem.
- One gem, one effect (the closest game effect; the game's other gems are left out):

| Gem | Armor and shield | Weapon |
|---|---|---|
| Onyx | Physical resistance | — |
| Opal | Magic resistance | — |
| Diamond | Hybrid 50/50 | — |
| Sapphire | Hybrid 70/30, more magic | — |
| Jasper | Hybrid 70/30, more physical | — |
| Emerald | — | Critical chance |
| Malachite | — | Critical damage |

- A hybrid gem splits the total (5% at 50/50 = 2.5% + 2.5%).
- Shields use armor gems, as in the game.
- Runeglass: a single item made of a capsule + a Pristine gem, as in the game. It takes the slot instead of a gem and
  gives the gem effect (5%) plus physical damage amplification (Punishment capsule) or magic amplification (Arcane
  capsule). It goes on weapons, shields and armor.
- Socketing: drag the gem or runeglass onto the item in the bag. It only goes into an empty slot; to swap, remove the
  old one first.
- Removing: returns the gem or runeglass to the bag, paying trade material (cost under balancing).

### Implementation

- Items: the game's 4 cuts (`<Gem>CutT1..T4`, item tier 2–5, 2–5%) of Onyx, Opal, Diamond, Sapphire, Jasper, Emerald
  and Malachite, and runeglass `Runeglass_Gem_<Gem>_Melee|Arcane` (Pristine gem + physical or magic amplification).
  Effects per item in `Data/Items/regras_itens.json` `gema_itens` (`tools/make_itens.py` from `tools/itens.json`
  `gemas`, `gema_pct_por_tier`, `vidro_capsulas`, `vidro_amplificacao`).
- Recipes (cutting, capsule, runeglass): [crafting.md](crafting.md#gems-and-runeglass).
- Server: `UNWInventoryComponent::ServerSocket` (gem from the bag into an item in the bag or equipped; only into an
  empty slot: with a gem in place it refuses and tells the player to remove it first) and `ServerUnsocket` (the gem
  goes back to the bag; provisional cost: 1 raw gem of the same type and tier, `FNWItemRules::UnsocketCost`).
  Socketing rule: `FNWItemRules::CanSocket` (armor gem on armor and shield, weapon gem on weapon; crafted items only).
  The gem is stored in `props.gem` (no migration).
- Screens: dropping a gem on an empty item opens "Attach Gem" (game text) with the effect; Shift + right-click on an
  item with a gem opens "Remove gem" with the cost. The item thumbnail shows the gem icon in the bottom-right corner.
  The item tooltip shows the gem icon, name and effects. A loose gem's tooltip shows its effects and where it fits
  (without the game description, which talks about the game's effects). Result screen: `GemPerkSlot` with the icon and
  effects.
- Capture: `tools/ui_shots/gemas.json` (bag, tooltips, attach, remove). Dev (F10): "Gems and Jewelcrafting 200"
  (Jewelcrafting and Stonecutting at 200, 7 Pristine gems, 2 runeglasses, a raw gem for the cost, material to cut and
  assemble one runeglass).

## Bind

- Bind on equip: the item is bound to the character when equipped. It can be unbound at the station of the item's
  trade skill ("Improve items" tab): it goes back to "binds on equip". The cost rises with the item's quality. Unbinding
  is a crafting action ([crafting.md](crafting.md#bind)).
- Bind on pickup: the item drops from loot already bound and cannot be unbound.
- Bound items do not go to the Trading Post ([city.md](city.md#trading-post)).

## Signature

- A crafted item with quality 80% or higher carries the crafter's signature: the nickname shows on the item.
- Each signed item has a serial number (crafter + sequence) issued by nw-core and stored in `props.serial`. The
  registry (`item_serials`) stores owner and location. Every write checks that the number exists and appears only once
  across bag, storage and Trading Post; otherwise the whole write is refused (migration 0009,
  `backend/src/routes/serials.rs`). The server gets the serial from nw-core before granting the item.

## Item data

- `FNWItem` holds attribute, quality, perks, gem, bind and signature (`props` in nw-core, migration 0008).
- Rules in `Data/Items/regras_itens.json` (`tools/make_itens.py`, `tools/itens.json`, `tools/perks_decisoes.json`).
- Tooltip with perks (icon and value) and the gem slot (socket frame).
- Rolling: `FNWItemRules::Roll`, range `FNWCrafting::GsRange` ([crafting.md](crafting.md#crafting-gear)).

## Open questions

Balancing (provisional values above):

- GS range of each material and crafter bonus.
- Curve from GS to attribute.
- Cost of improving and unbinding.
- Range of each perk (suggestions on the perk page) and base resistance of each material (provisional in "Armor and
  jewelry").
- Which items drop bound on pickup.
- Cost of rerolling the attribute.
- Cost of removing a gem; runeglass amplification.

Pending work:

- Effect of perks, attribute, gems and resistance on the character (critical chance and damage, cooldown, lifesteal,
  stamina regen, max health, amplification, shield in place of Fortify, armor weight class). Done together with the
  combat area ([combat.md](combat.md)); agree on it before changing combat code.
- Armor visuals (phase 2): export the bare body and each piece as a separate SkeletalMesh on the same skeleton. In C++,
  one `USkeletalMeshComponent` per slot with `SetLeaderPoseComponent` on the body, the bare mesh hidden when the slot
  is occupied, and the hair and head flags. This touches `NWCharacter` and the player asset set, which are shared:
  agree with the other areas before changing them.
- A long ability perk description (example: Healing Breeze) overflows its line on the result screen.
