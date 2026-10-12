"""Janela do inventário do jogo (lyshineui/inventory/newinventory.dynamicuicanvas, Inventory/Content).

    python3 tools/ui_build_inventory.py [--preview prévia.png]

Fica a janela (moldura, cabeçalho, ícone, título e moedas); a grade de itens e o equipamento o C++ desenha com as
texturas de espaço do jogo (itembgsquare<raridade>) e os ícones dos itens. Gera
unreal/NWPort/Data/UI/inventory_layout.<idioma>.json e extract/ui/inventory_textures.txt (inclui os ícones do
catálogo, Data/Items).
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
EXCLUDE = ["FrameHeaderSecondary"]
# Peças que o C++ desenha (FNWInventoryMenu): item retangular e quadrado do jogo, painel, abas e espaços do equipamento.
IL = "lyshineui/images/slices/itemlayout/"
EQ = "lyshineui/images/equipment/"
SLOT_TEXTURES = ([f"{IL}{k}{shape}{r}.dds" for k in ("itembg", "itemraritybg") for shape in ("large", "square") for r in range(5)]
                 + [f"{IL}{n}.dds" for n in ("itemhighlightlarge", "itemhighlightsquare", "itemnumberbg", "itemframeselected")]
                 + [f"{EQ}{n}.dds" for n in ("framesquare", "equipmentbg", "tabweaponsarmorselected", "tabweaponsarmorunselected",
                                             "tabtoolsselected", "tabtoolsunselected", "iconweapon", "iconshield", "iconpickaxe",
                                             "iconsickle", "iconlogging", "iconskinning")]
                 + [f"lyshineui/images/inventory/{n}.dds" for n in ("contentbg", "headerbg", "iconweight", "icon_inventory")])


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
    print(f"{len(lay['elements'])} elementos, {len(textures)} texturas")
    if a.preview:
        import ui_canvas
        relayout(lay["elements"], lay["size"])
        ui_canvas.preview(lay["elements"], tuple(lay["size"]), TEX, a.preview)


if __name__ == "__main__":
    main()
