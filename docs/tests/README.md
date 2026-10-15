# Test checklists

Manual tests, run in the Unreal editor. One file per area, numbered only within that file.

| File | Area | Map |
|---|---|---|
| [weapons.md](weapons.md) | weapons (area 1) | `L_Arena` |
| [ice-gauntlet.md](ice-gauntlet.md) | Ice Gauntlet (sub-area 1a) | `L_Arena` |
| [gathering.md](gathering.md) | gathering | `L_SouthWindsward` |
| [mounts.md](mounts.md) | mounts | `L_SouthWindsward` |
| [systems.md](systems.md) | systems, Everfall and game screens | `L_QuedaEterna` (Windsward for what already exists there) |
| [world.md](world.md) | Windsward region and lighting | `L_SouthWindsward`, `L_QuedaEterna` |
| [build.md](build.md) | Windows demo build (packaged client and servers, travel between maps) | the three maps |

Areas and their maps: [../workflow.md](../workflow.md).

## Format

- Numbering runs through the whole file, across sections. A new item takes the next number; existing numbers are
  kept.
- One item per line (two at most): what to do and what must happen, in the imperative, as
  "<action>: <expected result>."
- Sections group items by feature. A file may open with how to reach the feature (for example an F10 entry).

## Who runs them

- The maintainer (or whoever has the editor) runs the tests in the editor. Sessions never run the game, the client
  or automatic tests on their own; they open the editor on the area's map and ask for a screenshot
  ([../workflow.md](../workflow.md), "Editor and tests").
- Every test uses the local nw-core ([../tech/backend-testing.md](../tech/backend-testing.md)): start it before
  opening the editor. Play logs in with the editor's fixed account and keeps Inventory, position and levels between
  sessions.
- Closing an item of work adds its test item ([../workflow.md](../workflow.md), "Delivery checklist").
- A rule that rests on a grade H reverse-engineering answer says so in its test item.

## Screenshots

- For each item, take a screenshot if something is wrong.
- Game screens are compared first without the editor (`tools/linux/ui_shot.sh` against the original's screenshot,
  [../game/ui/README.md](../game/ui/README.md)); the screenshot from the editor comes last.
- With 8 GB of VRAM, editor + Play + Lumen do not fit with high-resolution captures; do not take high-resolution
  screenshots through a remote session.
