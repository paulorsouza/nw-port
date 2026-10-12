"""Monta o layout da HUD de combate (telas hud2024 do jogo) para o Unreal.

    python3 tools/ui_build_hud.py

Lê extract/ui/lyshineui/hud2024/*.dynamicuicanvas.json (nwbt --x-objects json), gera
unreal/NWPort/Data/UI/hud_layout.json (lido pelo ANWHUD em jogo) e a lista de texturas a importar
(extract/ui/hud_textures.txt), incluindo os ícones que o Lua do jogo põe em tempo de execução.
"""
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
CANVASES = ["vitals/vitals2024", "weaponquickslots/weaponquickslots", "consumablequickslots/consumablequickslots",
            "reticles/reticlescanvas"]
# Alças do modo de edição e elementos que o Lua só liga em situações específicas.
EXCLUDE = ["HudLayoutElementDraggable", "MountStaminaMeterLayoutElement", "ForceDismountTimerLayoutElement",
           "AbilityChannelLayoutElement", "HeartgemLayoutElement", "ComboPointsLayoutElement"]
# Imagens que o jogo troca por script (ícones de habilidade e de arma).
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
    for c in CANVASES:
        src = os.path.join(UI, "lyshineui", "hud2024", c + ".dynamicuicanvas.json")
        tmp = os.path.join(UI, os.path.basename(c) + ".layout.json")
        subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), src, tmp, "--tex", os.path.join(UI, "tex"),
                        "--exclude", ",".join(EXCLUDE)], check=True)
        lay = json.load(open(tmp))
        canvases.append({"name": os.path.basename(c), "size": lay["size"], "elements": lay["elements"]})
        textures |= {e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]}
    out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI")  # fora de Content (o editor importaria)
    os.makedirs(out, exist_ok=True)
    json.dump({"canvases": canvases}, open(os.path.join(out, "hud_layout.json"), "w"), separators=(",", ":"))
    open(os.path.join(UI, "hud_textures.txt"), "w").write("\n".join(sorted(textures)))
    import shutil
    fonts = os.path.join(out, "Fonts")
    os.makedirs(fonts, exist_ok=True)
    for f in ("nimbussannov-reg.otf", "nimbussannov-sembol.otf", "nimbussannov-bol.otf"):
        shutil.copy(os.path.join(UI, "tex", "lyshineui", "fonts", f), fonts)
    print(f"{sum(len(c['elements']) for c in canvases)} elementos em {len(canvases)} telas; {len(textures)} texturas")


if __name__ == "__main__":
    main()
