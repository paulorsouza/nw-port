# Game screens in the port

How to port a LyShine screen (canvas + the game's Lua) and test it without opening the game.

- Screenshots of the original game, for comparison and reference: `inspiracao/` (index in `inspiracao/README.md`,
  which also says which screenshots are stretched).
- Complete example: the refining screen (`docs/game/ui/refining.md`, `tools/ui_screens/refining.json`,
  `FNWRefiningMenu`).
- Survey of all screens: `docs/game/ui/original.md`, `extract/ui/survey.md`.
- Reverse engineering of the screens: ask the `nw-re` agent.
- Screen specs: `docs/game/ui/inventory.md` (inventory, equipment, storage), `docs/game/ui/refining.md`.
- History and notes for the next UI session: `docs/notes/ui.md`.

## Rules for all areas

- A new or rebuilt game screen follows the work cycle below: screen spec, capture without the game
  (`tools/linux/ui_shot.sh`), comparison with the original screenshot. Ask the maintainer for a screenshot only at the
  end.
- `FNWUILayout` is shared. The mode faithful to LyShine (`bExactText`) is turned on per screen, without changing the
  other screens.
- Screens copied from the game: game layouts built by `tools/ui_build_*.py` into `Data/UI/*.json`, drawn in C++
  (pattern of `NWInventoryMenu`, `NWTradeskillMenu`). Texts in `tools/strings/sistemas.json` (pt-BR and en-US).
- New screens (no counterpart in the game) use the port's Slate style (`NWSlate`; first used by `SNWMarket`, then
  `SNWCity`) with the game's colors and fonts. Screens copied from the game keep the game's layout.

## Work cycle

1. **Screen spec** in `tools/ui_screens/<screen>.json`: canvas, subtrees that stay, what the Lua turns on and off,
   text and button styles (format in the header of `tools/ui_build_screen.py`).
2. `python3 tools/ui_build_screen.py tools/ui_screens/<screen>.json` writes `Data/UI/<screen>_layout.<lang>.json` and
   the texture list. New texture: `tools/linux/build_all.sh 12` (extracts from the game) and
   `tools/linux/ue_import_all.sh 09` (imports).
3. **C++** fills in the data (`TextOverride`, `Visible`, `Tint`, `TextureOverride`, `Translate`, `DrawSubtree` for
   list rows), following the screen's Lua. Set `bExactText = true` in the screen's `FNWUILayout`.
4. **Capture without the game**: `tools/linux/ui_shot.sh tools/ui_shots/<screen>.json [capture]` →
   `extract/ui/shots/<screen>_<capture>.png` in ~40 s. Unreal without a window, empty world, a character with a bag
   and the scenario's levels, the same C++ as the screen (`UNWUIShotCommandlet`). Needs the module compiled and the
   editor closed. Language: `NW_LANG=en-us`.
5. **Compare**: `python3 tools/ui_compare.py <game screenshot> <capture> output.png [--crop x0,y0,x1,y1]` (original
   on top). The game screenshot is resized to the capture size.
6. **Measure**: `NW_SHOT_ARGS=-dump=<part of the path> tools/linux/ui_shot.sh ...` lists in the log
   (`Saved/ImportLogs/ui_shot.log`, `NWUIDump` lines) the rectangle, visibility, opacity, text and texture of each
   element.
7. Only at the end: compile, open the editor on the map and ask the maintainer for a screenshot.

Scenario (`tools/ui_shots/<screen>.json`): station, coins, trade skill levels, bag items and the captures (recipe,
quantity, mouse position, window size, `craft` + `t` for animated states). The bag is reloaded for each capture.
New screens need a branch in `UNWUIShotCommandlet`.

### Turning on `bExactText` on a screen

It is one line in the screen's `Load`, but it changes the text size of that screen (about 1.56× smaller than without
it, as in the game). Do it one screen at a time, with a capture.

## LyShine rules the port follows (`bExactText` mode)

| Rule | In the port |
|---|---|
| Font size = line height (ascender + descender) in 1080p pixels, not the em | scale = size ÷ line height of the font itself measured at 1 (`FNWUILayout::TextScale`) |
| Exact family from the `.font` | Nimbus regular, semibold, bold, medium, italic; Pica (IM Fell) and italic; Caslon (`GameFont` 0–7) |
| `wraptextsetting`, `linespacing`, `shrinktofit` | word wrap, line spacing, shrinks down to `minshrinkscale` |
| Horizontal `UiLayoutFitter` | element width = text width, around the pivot; children anchored to the edge follow it (e.g. ingredient quantity and name) |
| `StretchedToFit` image (type 4) | keeps the aspect ratio, centered |
| Additive blend = (SrcAlpha, One) | Unreal's additive is (One, One) and lights up RGB where alpha is 0; in faithful mode it is drawn as translucent |
| `scaletodevice` on a full-screen container | in a window wider than 16:9 the game **stretches** the screen horizontally (the screenshot looks oval). The port does not stretch; to compare, resize the screenshot to 16:9 |
| Flipbook (`UiFlipbookAnimation`) | no image in the canvas: the spec creates the image and the C++ swaps the frame (`blue_loop/blue_NN`) |
| Mask (`UiMask` with a child image, e.g. `mapmask.dds`) | the mask image must not be drawn: hide it in the spec |
| `caseoverride` | uppercase uses `FText::ToUpper` (`FNWUILayout`, shared), so accented letters are uppercased too |

## Lua: where to find each thing

- Text styles: `tools/ui_styles.py` runs `_common/uistyle.lua` and writes `extract/ui/uistyle.json` (1207 entries,
  588 styles). `python3 tools/ui_styles.py FONT_STYLE_X` prints one.
- Who uses which style: `SetTextStyle(self.Properties.<path>, UIStyle.<style>)` calls in the screen's Lua. The
  property name matches the element name; it goes into the spec's `"styles"`.
- Buttons: `slices/button.lua` `SetButtonStyle`. Default: background `buttonbg2023`, frame `primarybuttonframe2023`,
  focus 0.2, no hero layers. Hero: black background, blue sequence at 1, bottom glow y −11 `#63d5ff`, hero border 0
  (only on focus), text `FONT_STYLE_BUTTON_HERO_2023`. Icon (`SetIconPath`): turns on `buttonIconHolder` with the
  sprite. In the spec: `"buttons": {"/path": "default" | "hero"}`.
- Position set by the Lua (`UiTransformBus.SetLocalPositionY`, `ScriptedEntityTweener.Set {x, y, w, h}`): look for it
  in the screen's Lua and apply it in C++ as `Translate` or `Offsets` (e.g. `UpdateRequirements` moves requirements
  and quantity up 28 px per requirement row; `SLIDER_STYLE_2` puts the value in a 250×40 box above the bar).
- Texts: new game keys go in `tools/strings/sistemas.json` (`chaves_do_jogo`). Station names: `@<type>_station` +
  `ui_tier` (the `<TradeSkill>T<n>_MasterName` keys are misaligned in the game: T2 = "Tier 3").
- Hold-to-repeat buttons: `button.lua` `repeatParamsDefault`. Space = action `tryskip` (`uishared.xml`).
