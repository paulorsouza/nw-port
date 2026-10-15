"""The game's group member bar (lyshineui/hud/grouphealth/grouphud.dynamicuicanvas, GroupHealthBarSlice1).

    python3 tools/ui_build_group.py [--preview preview.png]

The game instantiates this template per member (Lua); here a "member" canvas comes out with the template at the origin
and the C++ draws one copy per member with Translate. Kept: name, health bar, group icon and death icon. Writes
unreal/NWPort/Data/UI/group_layout.json and extract/ui/group_textures.txt.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_login import ROOT, TEX, UI, convert, relayout  # noqa: E402

SLICE = "_root/GroupHUD/MasterContainer/DisabledContainer/GroupHealthBarSlice1"
EXCLUDE = ["Bg", "StreamingStatusBg", "PlayerIcon", "VoipSpeaker", "IconRow", "MIcStatus", "RIcon", "HealthBarPulseFrame",
           "DeltaFill", "HealingStatusBg"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    a = ap.parse_args()
    lay = convert(os.path.join(UI, "lyshineui", "hud", "grouphealth", "grouphud.dynamicuicanvas.json"), [SLICE], EXCLUDE,
                  ["GroupHealthBarSlice1", "SmallDeathIcon"])
    for e in lay["elements"]:
        if e["path"] == SLICE:
            e["anchors"], e["offsets"] = [0, 0, 0, 0], [0, 0, 346, 56]
        elif not e["path"].startswith(SLICE):
            # Ancestors (root, GroupHUD, containers): the whole screen, no offset.
            e["anchors"], e["offsets"], e["scale"] = [0, 0, 1, 1], [0, 0, 0, 0], [1, 1]
    out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", "group_layout.json")
    json.dump({"canvases": [{"name": "member", "size": lay["size"], "elements": lay["elements"]}]}, open(out, "w"), separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]})
    open(os.path.join(UI, "group_textures.txt"), "w").write("\n".join(textures))
    print(f"{len(lay['elements'])} elements, {len(textures)} textures -> {out}")
    if a.preview:
        import ui_canvas
        relayout(lay["elements"], lay["size"])
        ui_canvas.preview(lay["elements"], tuple(lay["size"]), TEX, a.preview)


if __name__ == "__main__":
    main()
