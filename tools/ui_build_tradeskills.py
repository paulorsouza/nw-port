"""The game's gathering trade skills tab (lyshineui/skills/skills.dynamicuicanvas, TradeSkills > GatheringSection).

    python3 tools/ui_build_tradeskills.py

Writes unreal/NWPort/Data/UI/tradeskills_layout.<language>.json and extract/ui/tradeskills_textures.txt. Applies what the
game's Lua does and is not in the file: default frame (FRAME_STYLE_DEFAULT_2023), level circle shrunk to fit the
cell (0.5 scale carried to the children, because FNWUILayout only scales its own rectangle), the gathering section
centered at the top of the window (the game stacks the sections in a list), "Trade Skills" title and @key texts resolved.
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import load_loc, LANGS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
SECTION = "_root/Skills/ScreenHolder/TradeSkills/SkillsScrollbox/SkillsContainer/GatheringSection"
KEEP = ["_root/Skills/ScreenHolder/FrameMultiBg", "_root/Skills/ScreenHolder/FrameHeader", SECTION]
EXCLUDE = ["FishingCell", "Tooltip", "MomentFlash", "MomentPulse", "CellPostCapIcon", "StationIcon", "PostCapLevelText",
           "ExpansionBadge"]
CIRCLE_SCALE = 0.5
# Gathering section: from the middle of the list (y 478) to the top of the window, centered in x.
SHIFT = (-113.0, -248.0)
FRAME_BG = "lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.dds"


def main():
    tmp = os.path.join(UI, "tradeskills.layout.json")
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"),
                    os.path.join(UI, "lyshineui", "skills", "skills.dynamicuicanvas.json"), tmp, "--tex", os.path.join(UI, "tex"),
                    "--keep", ",".join(KEEP), "--enable", "TradeSkills", "--exclude", ",".join(EXCLUDE)],
                   check=True, capture_output=True)
    lay = json.load(open(tmp))
    for e in lay["elements"]:
        p = e["path"]
        if p.endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            e["image"]["texture"] = FRAME_BG
            e["image"]["type"] = "sliced"
            sp = os.path.join(UI, "tex", FRAME_BG.replace(".dds", ".sprite"))
            if os.path.exists(sp):
                m = re.search(r'm_left="([\d.]+)" m_right="([\d.]+)" m_top="([\d.]+)" m_bottom="([\d.]+)"', open(sp).read())
                if m:
                    e["image"]["borders"] = dict(zip(("left", "right", "top", "bottom"), map(float, m.groups())))
        if p.endswith("FrameHeader/FrameHeaderText") and e.get("text"):
            e["text"]["text"] = "@ui_trade_skills"
        if p == SECTION:
            o = e["offsets"]
            e["offsets"] = [o[0] + SHIFT[0], o[1] + SHIFT[1], o[2] + SHIFT[0], o[3] + SHIFT[1]]
        if "/SkillLevelCircleProgress" in p:
            o = e["offsets"]
            if p.endswith("/SkillLevelCircleProgress"):
                # Scale around the pivot (center): only the size shrinks.
                cx, cy = (o[0] + o[2]) / 2, (o[1] + o[3]) / 2
                e["offsets"] = [cx + (o[0] - cx) * CIRCLE_SCALE, cy + (o[1] - cy) * CIRCLE_SCALE,
                                cx + (o[2] - cx) * CIRCLE_SCALE, cy + (o[3] - cy) * CIRCLE_SCALE]
            else:
                e["offsets"] = [v * CIRCLE_SCALE for v in o]
                if e.get("text"):
                    e["text"]["size"] = round(e["text"]["size"] * CIRCLE_SCALE, 1)
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"tradeskills_layout.{lang}.json")
        json.dump({"canvases": [{"name": "tradeskills", "size": lay["size"], "elements": elements}]}, open(out, "w"),
                  separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]}
                      | {FRAME_BG, "lyshineui/images/skills/tradeskills/skillcellhover.dds"})
    open(os.path.join(UI, "tradeskills_textures.txt"), "w").write("\n".join(textures) + "\n")
    print(f"{len(lay['elements'])} elements, {len(textures)} textures")


if __name__ == "__main__":
    main()
