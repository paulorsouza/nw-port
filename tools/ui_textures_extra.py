"""Texture lists of the screens drawn directly by the C++ (compass, encumbrance, interaction card, damage numbers).

    python3 tools/ui_textures_extra.py

Reads the game canvases (tools/ui_canvas.py) and writes extract/ui/<screen>_textures.txt, imported by
tools/ue/import_ui.py.
"""
import glob
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
SCREENS = ["compass", "encumbrance", "interactscreen", "damagenumbers", "unifiedinteract", "lootticker2024"]

for name in SCREENS:
    src = glob.glob(os.path.join(UI, "lyshineui", "**", f"{name}.dynamicuicanvas.json"), recursive=True)[0]
    tmp = os.path.join(UI, "survey", f"{name}.flat.json")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), src, tmp, "--tex", os.path.join(UI, "tex")],
                   check=True, capture_output=True)
    els = json.load(open(tmp))["elements"]
    tex = sorted({e["image"]["texture"] for e in els if e.get("image") and e["image"].get("texture")})
    open(os.path.join(UI, f"{name}_textures.txt"), "w").write("\n".join(tex))
    print(f"{name}: {len(tex)} textures")

# Item tooltip (tooltip.dynamicuicanvas has 3k elements, mostly sample icons): only the pieces used.
TOOLTIP = ["tooltip/tooltip_bg_new.dds", "tooltip/tooltip_bgframenew.dds", "tooltip/tooltip_divider.dds",
           "tooltip/tooltip_top_bg.dds", "tooltip/tooltip_legendary_overlay.dds",
           "slices/sectionheader/sectionheaderline.dds", "icons/misc/icon_gearscore.dds"]
TOOLTIP += [f"tooltip/tooltip_header_bg_{i}.dds" for i in range(5)] + [f"tooltip/tooltip_itemframe_rarity{i}.dds" for i in range(1, 5)] + ["tooltip/tooltip_itemframe_rarity_none.dds"]
TOOLTIP += ["tooltip/frame_gemsocket.dds"]   # gem slot (dynamictooltip_perk.lua)
# Ability perk icons (the port's item rules, tools/make_itens.py).
RULES = os.path.join(ROOT, "unreal", "NWPort", "Data", "Items", "regras_itens.json")
PERK_ICONS = sorted({p["icone"] for p in json.load(open(RULES))["perks"].values() if p.get("icone")}) if os.path.exists(RULES) else []
open(os.path.join(UI, "tooltip_textures.txt"), "w").write("\n".join(["lyshineui/images/" + t for t in TOOLTIP] + PERK_ICONS))
print(f"tooltip: {len(TOOLTIP)} textures")

# Level-up banner (lyshineui/banner/bannerlevelup.dynamicslice): only the XP part; trade skill icons.
LEVELUP = ["icons/tradeskills/icon_tradeskill_mining.dds", "icons/tradeskills/icon_tradeskill_logging.dds",
           "icons/tradeskills/icon_tradeskill_harvesting.dds", "icons/tradeskills/icon_tradeskill_skinning.dds",
           "navbar/navbar_bg_2023.dds", "navbar/highlight.dds", "navbar/navbar_icon_character.dds",
           "navbar/navbar_icon_map.dds", "navbar/unspentpoints.dds", "warningbar/background_warningbar.dds", "hitindicator/hitdirection.dds", "hitindicator/hiteffect_light_b.dds", "hitindicator/hiteffect_medium_b.dds",
           "banner/bannerleveluprune_xp.dds", "banner/bannerleveluprune_x.dds", "banner/banner_light_blue2.dds",
           "banner/banner_fog.dds", "notifications/notification_showline.dds"]
open(os.path.join(UI, "levelup_textures.txt"), "w").write("\n".join("lyshineui/images/" + t for t in LEVELUP))
print(f"levelup: {len(LEVELUP)} textures")
