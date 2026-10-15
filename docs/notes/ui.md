# UI notes

History of the UI area (screens and HUD) and notes for the next session. Specs: `docs/game/ui/`.

## Next UI session (state on 2026-10-15)

- Refining: the maintainer still has to test the items marked "pending test" under 2026-10-13.
- Refining title bar: the port shows only coins. Add the station's trade skill circle (the 300×300 slice inside
  `SkillContainer1`, 60×60, scaled by the Lua), comparing against a screenshot of the original. The port has no azoth.
- Screen order after refining: storage (`catcontainer.lua`), attributes, trade skills, inventory (RE shortcuts) and
  weapon mastery (together with the weapons area).
- `bExactText` is on only for refining. Turning it on elsewhere: one screen at a time with a capture (text gets about
  1.56× smaller).
- Inventory: known differences and open questions in `docs/game/ui/inventory.md` §5 and "Open questions".

## 2026-10-11

- Surveyed the game files for screens and HUD: `docs/game/ui/original.md`, with the generated per-canvas survey in
  `extract/ui/survey.md`. The "screens the port does not have yet" list dates from this survey.

## 2026-10-13

- The maintainer took the reference screenshots in `inspiracao/`.
- Refining screen built (`FNWRefiningMenu`, `tools/ui_screens/refining.json`). Text measured in `bExactText` mode:
  131×17 px in the port against 135×17 in the game.
- `caseoverride` added to `tools/ui_canvas.py`; `heroframe2023` and `blue_loop/*` extracted through the spec's
  `extra_textures`.
- `UNWUIShotCommandlet` had a branch only for `refining`.
- Inventory, equipment and storage: items 1 to 7 of the plan in `docs/game/ui/inventory.md` done in
  `NWInventoryMenu` (capture `tools/ui_shots/inventory.json`), tested by the maintainer. Maintainer's priority:
  mechanics before refining the filters.

### Maintainer's notes for the next HUD session (2026-10-13)

Refining:

- Done, pending the maintainer's test: drag the marker and hold − and + to repeat (`button.lua`
  `repeatParamsDefault`); Space = action `tryskip` (`uishared.xml`): maximum on the selection screen, skips the count,
  returns to the station. The CRAFT button (pt-BR "CONFECCIONAR") flickered because the game's loop has 30 frames at
  30 fps and the port played 15 at 15 fps (now `blue_00..29`, and `gray_00..29` when disabled, loaded when the screen
  opens).
- Done: tree order by `CategorySortOrder`/`GroupSortOrder` from `craftingsortinfo.lua` (`make_crafting.py` writes
  `category_order`/`group_order`).
- Done: fee with `@ui_craft_fee` (pt-BR: "Taxa de Queda Eterna: 0,26") and coins in cents on every screen
  (`UNWInventoryComponent::FormatCoins`, ÷100 like `GetLocalizedCurrency`); XP with `@ui_craft_bonus` = reward × Σ Qty
  of the ingredients × quantity (`sistemas-005`). First craft ×3 (crafted recipes in `progression.crafted`); the port
  has no faction buffs or boosts.
- Title bar: coins only. The game (`stationproperties.lua` → `screenheader.lua`) shows azoth, coins and the station's
  trade skill circle (`SetSkill1Circle`: level, progress ring, icon). The circle is the 300×300 slice inside
  `SkillContainer1` (60×60), scaled by the Lua: do it with a screenshot of the original to compare. The port has no
  azoth.
- Pin Recipe: already hidden in the layout.
- Uppercase with accents ("FUNDIçãO"): `caseoverride` now uses `FText::ToUpper` (`FNWUILayout`, shared).

Inventory, equipment and storage: spec in `docs/game/ui/inventory.md` (drag and drop, sections, storage).

Screen order after refining: storage (`catcontainer.lua`), attributes, trade skills, inventory (RE shortcuts) and
weapon mastery (with the weapons area).

`bExactText` is on only in refining. Turning it on in the other screens is one line in each one's `Load`, but it
changes their text size (about 1.56× smaller than now, as in the game): do it screen by screen with the capture.

## 2026-10-14

- Shared storage tabs added at the maintainer's request (the game has none): spec in `docs/game/ui/inventory.md` §4,
  captures `armazem_aba2` and `arrasto_aba` in `tools/ui_shots/inventory.json`.
- The maintainer approved the Slate style of the Trading Post screen (`SNWMarket`, the port's first Slate screen):
  new screens follow this style; screens copied from the game stay as they are. `SNWCity` (style `NWSlate`) followed.

## 2026-10-15

- UI docs moved to `docs/game/ui/` and `extract/ui/survey.md`, in English.
- Checked in code: `UNWUIShotCommandlet` (`unreal/NWPort/Source/NWPort/Private/NWUIShotCommandlet.cpp`) accepts the
  screens `refining`, `shop`, `inventory`, `market` and `city`.
