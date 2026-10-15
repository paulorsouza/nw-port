#!/usr/bin/env python3
"""A game screen from the canvas and a state file (area 4: refining, Storage, ...).

    python3 tools/ui_build_screen.py tools/ui_screens/<screen>.json [--preview output.png]

The canvas has the pieces of every mode; the game's Lua (extract/lua/lyshineui/, decompiled) decides what shows.
The state file says that for the screen the port shows:

    {
      "canvas": "lyshineui/crafting/craftingscreenv4",   # extract/ui/<canvas>.dynamicuicanvas.json
      "name": "refining",                                # Data/UI/<name>_layout.<language>.json, canvas "<name>"
      "keep": ["_root/CraftingScreen/..."],              # subtrees that stay (empty = everything)
      "exclude": ["MultiCraft"],                         # names whose subtrees are removed
      "show": ["/Refining"], "hide": ["/QualityComplex"],# path suffixes turned on/off (what the Lua does)
      "set": {"/FrameHeaderText": {"text": "@ui_x", "color": [1, 1, 1, 1], "texture": "lyshineui/...dds"}},
      "extra_textures": ["lyshineui/images/...dds"],     # textures the C++ uses outside the layout
      "buttons": {"/FilterButton": "default"},           # slices/button.lua style (layers, textures, font)
      "styles": {"/EntryText": "FONT_STYLE_X"}           # text style from _common/uistyle.lua (tools/ui_styles.py)
    }

Output: unreal/NWPort/Data/UI/<name>_layout.<language>.json and extract/ui/<name>_textures.txt (import: tools/ue/import_ui.py).
@key texts resolved per language; the C++ fills in the data (FNWUILayout, TextOverride/Visible/DrawSubtree).
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_build_mastery import load_loc, LANGS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")


def sprite_borders(texture):
    sp = os.path.join(UI, "tex", texture.replace(".dds", ".sprite"))
    if os.path.exists(sp):
        m = re.search(r'm_left="([\d.]+)" m_right="([\d.]+)" m_top="([\d.]+)" m_bottom="([\d.]+)"', open(sp).read())
        if m:
            return dict(zip(("left", "right", "top", "bottom"), map(float, m.groups())))
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("--preview")
    a = ap.parse_args()
    spec = json.load(open(a.spec))
    name = spec["name"]
    tmp = os.path.join(UI, f"{name}.layout.json")
    cmd = ["python3", os.path.join(ROOT, "tools", "ui_canvas.py"),
           os.path.join(UI, spec["canvas"] + ".dynamicuicanvas.json"), tmp, "--tex", os.path.join(UI, "tex"),
           "--keep", ",".join(spec.get("keep", [])), "--enable", ",".join(spec.get("enable", [])),
           "--exclude", ",".join(["HudLayoutElementDraggable"] + spec.get("exclude", []))]
    subprocess.run(cmd, check=True, capture_output=True)
    lay = json.load(open(tmp))

    def match(path, suffixes):
        return any(path.endswith(s) or ("/" + s.strip("/") + "/") in (path + "/") for s in suffixes)

    # Canvas root: in the file it is a 100×100 box in the center; in the game the HUD stretches it to the whole screen.
    # Without this, what is anchored top/bottom/corners (title bar, footer, panels) ends up near the center.
    for e in lay["elements"]:
        if e["parent"] < 0:
            e["anchors"], e["offsets"] = [0, 0, 1, 1], [0, 0, 0, 0]
    show, hide, sets = spec.get("show", []), spec.get("hide", []), spec.get("set", {})
    for e in lay["elements"]:
        p = e["path"]
        if match(p, hide):
            e["enabled"] = False
        if any(p.endswith(s) for s in show):
            e["enabled"] = True
        for suffix, ch in sets.items():
            if not p.endswith(suffix):
                continue
            if "text" in ch and e.get("text"):
                e["text"]["text"] = ch["text"]
            if "color" in ch:
                if e.get("text"):
                    e["text"]["color"] = ch["color"]
                elif e.get("image"):
                    e["image"]["color"] = ch["color"]
            if "texture" in ch and e.get("image") is not None:
                e["image"]["texture"] = ch["texture"]
                b = sprite_borders(ch["texture"])
                if b:
                    e["image"]["type"] = "sliced"
                    e["image"]["borders"] = b
            for k in ("anchors", "offsets"):
                if k in ch:
                    e[k] = ch[k]
    # Lua styles (uistyle.lua via tools/ui_styles.py): font, size, color, spacing, case.
    styles = json.load(open(os.path.join(UI, "uistyle.json"))) if (spec.get("styles") or spec.get("buttons")) else {}
    cases = {"eUiTextCaseSetting_Normal": 0, "eUiTextCaseSetting_Lower": 1, "eUiTextCaseSetting_Upper": 2}

    def apply_style(e, name):
        st, t = styles.get(name), e.get("text")
        if not st or not t:
            return
        if st.get("fontFamily"):
            t["font"] = st["fontFamily"].replace("\\", "/").lower()
        for src, dst in (("fontSize", "size"), ("fontColor", "color"), ("characterSpacing", "spacing")):
            if st.get(src) is not None:
                t[dst] = st[src]
        if st.get("textCasing") in cases:
            t["case"] = cases[st["textCasing"]]

    for suffix, style in spec.get("styles", {}).items():
        for e in lay["elements"]:
            if e["path"].endswith(suffix):
                apply_style(e, style)
    # Buttons (slices/button.lua, SetButtonStyle): the canvas carries every layer; the default style uses the 2023
    # background and frame, no hero border/sequence, focus at 0.2 and glows off.
    extra = set()
    for suffix, kind in spec.get("buttons", {}).items():
        roots = [e["path"] for e in lay["elements"] if e["path"].endswith(suffix)]
        for root in roots:
            for e in lay["elements"]:
                rel = e["path"][len(root):] if e["path"].startswith(root + "/") else None
                if rel is None:
                    continue
                if rel in ("/buttonText/buttonHintHolder", "/buttonText/buttonSecondaryIconHolder",
                           "/buttonText/buttonSecondaryIconSprite", "/spinner"):
                    e["enabled"] = False   # key hint, secondary icon and spinner: only when the Lua turns them on
                if kind == "hero":
                    # BUTTON_STYLE_HERO: black background, blue sequence at 1, bottom glow at y −11, hero border at 0
                    # (lights up on focus), heroframe2023 frame, FONT_STYLE_BUTTON_HERO_2023 text.
                    if rel == "/buttonBg" and e.get("image"):
                        e["image"]["color"] = [0, 0, 0, 1]
                    elif rel in ("/heroBorder", "/buttonFocus", "/buttonFocusHero", "/heroImageSequenceDisabled", "/frameFocus",
                                 "/buttonBgFocus"):
                        e["fade"] = 0
                    elif rel == "/heroImageSequence":
                        # Flipbook (UiFlipbookAnimation, no image in the canvas): the C++ swaps the blue_loop/blue_NN frame.
                        e["fade"] = 1
                        e["image"] = {"texture": "lyshineui/images/slices/button/blue_loop/blue_00.dds", "color": [1, 1, 1, 1],
                                      "alpha": 1, "type": "stretched", "blend": 0, "fill": "none", "fill_amount": 1,
                                      "fill_start_angle": 0, "fill_clockwise": True, "fill_edge_origin": 0,
                                      "fill_center": True, "borders": None}
                    elif rel == "/buttonBottomGlow" and e.get("image"):
                        e["fade"] = 1
                        e["image"]["color"] = styles.get("COLOR_BUTTON_BOTTOM_GLOW_HERO", [1, 1, 1, 1])
                        e["offsets"] = [e["offsets"][0], e["offsets"][1] - 11, e["offsets"][2], e["offsets"][3] - 11]
                    elif rel == "/frame" and e.get("image"):
                        e["image"]["texture"] = "lyshineui/images/slices/button/heroframe2023.dds"
                    elif rel == "/buttonText":
                        apply_style(e, "FONT_STYLE_BUTTON_HERO_2023")
                elif rel in ("/heroBorder", "/ButtonFocusHero", "/heroImageSequence", "/heroImageSequenceDisabled"):
                    e["enabled"] = False
                elif rel in ("/buttonBgFocus", "/buttonBottomGlow", "/frameFocus"):
                    e["fade"] = 0
                elif rel == "/buttonFocus" and e.get("image"):
                    e["image"]["texture"] = "lyshineui/images/slices/button/focusnew2023.dds"
                    e["fade"] = 0.2 if kind == "default" else 0
                elif rel == "/buttonBg" and e.get("image"):
                    e["image"]["texture"] = "lyshineui/images/slices/button/buttonbg2023.dds"
                    e["image"]["color"] = [1, 1, 1, 1]
                elif rel == "/frame" and e.get("image"):
                    e["image"]["texture"] = "lyshineui/images/slices/button/primarybuttonframe2023.dds"
                elif rel == "/buttonText":
                    apply_style(e, "FONT_STYLE_BUTTON_2023")
                if e.get("image") and e["image"].get("texture"):
                    b = sprite_borders(e["image"]["texture"])
                    if b:
                        e["image"]["type"], e["image"]["borders"] = "sliced", b
                    extra.add(e["image"]["texture"])
    # visible (preview) = parent visible and itself enabled, after the changes above.
    for e in lay["elements"]:
        par = lay["elements"][e["parent"]] if e["parent"] >= 0 else None
        e["visible"] = (par["visible"] if par else True) and e.get("enabled", True)
    json.dump(lay, open(tmp, "w"))
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"{name}_layout.{lang}.json")
        json.dump({"canvases": [{"name": name, "size": lay["size"], "elements": elements}]}, open(out, "w"),
                  separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]}
                      | {t.lower() for t in spec.get("extra_textures", [])} | {t.lower() for t in extra})
    open(os.path.join(UI, f"{name}_textures.txt"), "w").write("\n".join(textures) + "\n")
    if a.preview:
        subprocess.run(["python3", "-c", f"""
import json, sys
sys.path.insert(0, {os.path.join(ROOT, 'tools')!r})
import ui_canvas
lay = json.load(open({tmp!r}))
ui_canvas.preview(lay['elements'], lay['size'], {os.path.join(UI, 'tex')!r}, {a.preview!r})
"""], check=True)
    print(f"{name}: {len(lay['elements'])} elements, {len(textures)} textures")


if __name__ == "__main__":
    main()
