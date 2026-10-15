"""The game's inventory window (lyshineui/inventory/newinventory.dynamicuicanvas, Inventory/Content).

    python3 tools/ui_build_inventory.py [--preview preview.png]

Kept: the window (frame, header, icon, title and coins); the item grid and the equipment are drawn by the C++ with the
game's slot textures (itembgsquare<rarity>) and the item icons. Writes
unreal/NWPort/Data/UI/inventory_layout.<language>.json and extract/ui/inventory_textures.txt (includes the catalog
icons, Data/Items).
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_login import FRAME_BG, ROOT, TEX, UI, convert, relayout, set_image  # noqa: E402
from ui_build_mastery import LANGS, load_loc  # noqa: E402

C = "_root/Inventory/Content"
KEEP = [f"{C}/FrameMultiBg", f"{C}/FrameHeader", f"{C}/InventoryIcon", f"{C}/InventoryTitle", f"{C}/CurrencyText"]
# Coin gain effects (the Lua only enables them in the animation) and azoth, which the port does not have.
EXCLUDE = ["FrameHeaderSecondary", "WhooshAnim1", "WhooshAnim2", "BurstParticle", "RepairPartsGlow", "Flash", "CurrencyText2"]
# Pieces the C++ draws (FNWInventoryMenu): the game's rectangular and square item, panel, tabs and equipment slots.
IL = "lyshineui/images/slices/itemlayout/"
EQ = "lyshineui/images/equipment/"
SLOT_TEXTURES = ([f"{IL}{k}{shape}{r}.dds" for k in ("itembg", "itemraritybg") for shape in ("large", "square") for r in range(5)]
                 + [f"{IL}{n}.dds" for n in ("itemhighlightlarge", "itemhighlightsquare", "itemnumberbg", "itemframeselected")]
                 + [f"{EQ}{n}.dds" for n in ("framesquare", "equipmentbg", "tabweaponsarmorselected", "tabweaponsarmorunselected",
                                             "tabtoolsselected", "tabtoolsunselected", "iconweapon", "iconshield", "iconpickaxe",
                                             "iconsickle", "iconlogging", "iconskinning", "equipmentstatsbackdrop2023")]
                 + [f"lyshineui/images/inventory/{n}.dds" for n in ("contentbg", "headerbg", "iconweight", "icon_inventory")]
                 # Storage inside the inventory (catcontainer.lua STORAGE_ICON_PATH).
                 + ["lyshineui/images/catcontainer/icon_storage_shed.dds", "lyshineui/images/catcontainer/icon_dropped_items.dds"]
                 # Game-style screen (docs/game/ui/inventory.md): round tiles, equipment, sorting, drag.
                 + [f"{IL}{k}circle{r}.dds" for k in ("itembg", "itemraritybg") for r in range(5)]
                 + [f"{IL}itemhighlightcircle.dds"]
                 + [f"{EQ}{n}.dds" for n in ("iconhead", "iconchest", "iconhand", "iconlegs", "iconfeet", "iconrune", "iconamulet",
                                             "iconring", "icontoken", "iconfishing", "equipmentstatsbackdrop")]
                 + [f"lyshineui/images/icons/misc/{n}.dds" for n in ("icon_weight", "icon_filter_chrono", "icon_filter_gearscore",
                                                                    "icon_filter_tier", "dropdownarrowwhite", "icon_smallarrownarrow",
                                                                    "icon_gearscore_dropshadow")]
                 + ["lyshineui/images/inventory/dropzoneglow.dds", "lyshineui/images/icon_item_selected.dds",
                    "lyshineui/images/frame_lootticker.dds", "lyshineui/images/slices/button/buttonbg.dds",
                    "lyshineui/images/slices/button/primarybuttonframe.dds", "lyshineui/images/icons/objectives/reward_xp.dds",
                    "lyshineui/images/slices/hint/hintframefilled.dds",
                    "lyshineui/images/inventory/dropitemtrim.dds", "lyshineui/images/popup/popupbg.dds",
                    "lyshineui/images/slices/slider/sliderhandle2023.dds", "lyshineui/images/slices/slider/sliderhandlefocus.dds",
                    "lyshineui/images/slices/textinputsearch/primarysearchbg.dds",
                    "lyshineui/images/slices/textinputsearch/primarysearchbgfocus.dds",
                    "lyshineui/images/socialpane/icon_magnifying_glass.dds"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    a = ap.parse_args()
    lay = convert(os.path.join(UI, "lyshineui", "inventory", "newinventory.dynamicuicanvas.json"), KEEP, EXCLUDE, [])
    for e in lay["elements"]:
        if e["path"] == "_root":
            e["anchors"], e["offsets"] = [0, 0, 1, 1], [0, 0, 0, 0]
        if e["path"].endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            set_image(e, FRAME_BG)
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"inventory_layout.{lang}.json")
        json.dump({"canvases": [{"name": "inventory", "size": lay["size"], "elements": elements}]}, open(out, "w"), separators=(",", ":"))
    items = json.load(open(os.path.join(ROOT, "unreal", "NWPort", "Data", "Items", "items.pt-br.json")))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]}
                      | set(SLOT_TEXTURES) | {i["icon"] for i in items if i.get("icon")})
    open(os.path.join(UI, "inventory_textures.txt"), "w").write("\n".join(textures))
    print(f"{len(lay['elements'])} elements, {len(textures)} textures")
    if a.preview:
        import ui_canvas
        relayout(lay["elements"], lay["size"])
        ui_canvas.preview(lay["elements"], tuple(lay["size"]), TEX, a.preview)


if __name__ == "__main__":
    main()
