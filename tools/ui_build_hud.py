"""Builds the combat HUD layout (the game's hud2024 screens) for Unreal.

    python3 tools/ui_build_hud.py

Reads extract/ui/lyshineui/hud2024/*.dynamicuicanvas.json (nwbt --x-objects json), writes
unreal/NWPort/Data/UI/hud_layout.json (read by ANWHUD in game) and the list of textures to import
(extract/ui/hud_textures.txt), including the icons the game's Lua sets at run time.
"""
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
CANVASES = ["vitals/vitals2024", "weaponquickslots/weaponquickslots", "consumablequickslots/consumablequickslots",
            "reticles/reticlescanvas", "xp/xp"]
# Edit-mode handles and elements the Lua only turns on in specific situations.
EXCLUDE = ["HudLayoutElementDraggable", "MountStaminaMeterLayoutElement", "ForceDismountTimerLayoutElement",
           "AbilityChannelLayoutElement", "HeartgemLayoutElement", "ComboPointsLayoutElement",
           # XP bar: rested XP, bonus multiplier and the gain glow (the port does not have them).
           "MeterRestedFill", "MultiplierBg", "IncreaseDelta"]
# Images the game swaps by script (ability and weapon icons).
EXTRA = ["lyshineui/images/icons/abilities/sword_ability02.dds", "lyshineui/images/icons/abilities/sword_ability04.dds",
         "lyshineui/images/icons/abilities/sword_ability05.dds", "lyshineui/images/icons/abilities/sword_ability06.dds",
         "lyshineui/images/icons/abilities/sword_ability07.dds", "lyshineui/images/icons/abilities/sword_ability03.dds",
         "lyshineui/images/icons/abilities/abilities_bg1.dds", "lyshineui/images/icons/abilities/abilities_bg2.dds",
         "lyshineui/images/icons/abilities/abilities_bg3.dds",
         "lyshineui/images/icons/items/drawing/1hsword.dds",
         "lyshineui/images/hud2024/weaponquickslots/weaponsetbginactive.dds",
         *[f"lyshineui/images/icons/abilities/hatchet_ability0{i}.dds" for i in range(1, 7)], "lyshineui/images/icons/items/drawing/1hhatchet.dds"]


def main():
    canvases = []
    textures = set(EXTRA)
    # Ability and weapon icons of every weapon from tools/make_abilities.py (runs earlier, in make_data.sh).
    abil = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", "abilities.pt-br.json")
    if os.path.exists(abil):
        data = json.load(open(abil))
        for w in data.get("weapons", data).values() if isinstance(data, dict) else []:
            if not isinstance(w, dict):
                continue
            if w.get("hud_icon"):
                textures.add(w["hud_icon"])
            for a in (w.get("abilities") or {}).values() if isinstance(w.get("abilities"), dict) else w.get("abilities") or []:
                if isinstance(a, dict) and a.get("icon"):
                    textures.add(a["icon"])
    for c in CANVASES:
        src = os.path.join(UI, "lyshineui", "hud2024", c + ".dynamicuicanvas.json")
        tmp = os.path.join(UI, os.path.basename(c) + ".layout.json")
        subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), src, tmp, "--tex", os.path.join(UI, "tex"),
                        "--exclude", ",".join(EXCLUDE)], check=True)
        lay = json.load(open(tmp))
        canvases.append({"name": os.path.basename(c), "size": lay["size"], "elements": lay["elements"]})
        textures |= {e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]}
    out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI")  # outside Content (the editor would import it)
    os.makedirs(out, exist_ok=True)
    json.dump({"canvases": canvases}, open(os.path.join(out, "hud_layout.json"), "w"), separators=(",", ":"))
    open(os.path.join(UI, "hud_textures.txt"), "w").write("\n".join(sorted(textures)))
    import shutil
    fonts = os.path.join(out, "Fonts")
    os.makedirs(fonts, exist_ok=True)
    # Pica (pica_regular.font = IM Fell, game titles) and Caslon: styles 3, 4 and 5 of FNWUILayout::GameFont.
    for f in ("nimbussannov-reg.otf", "nimbussannov-sembol.otf", "nimbussannov-bol.otf", "imfepirm28p.ttf", "imfepiit28p.ttf",
              "caslonant.otf", "nimbussannov-regita.otf", "nimbussannov-med.otf"):
        shutil.copy(os.path.join(UI, "tex", "lyshineui", "fonts", f), fonts)
    print(f"{sum(len(c['elements']) for c in canvases)} elements in {len(canvases)} screens; {len(textures)} textures")


if __name__ == "__main__":
    main()
