"""Listas de texturas das telas desenhadas direto pelo C++ (bússola, sobrecarga, cartão de interação, números de dano).

    python3 tools/ui_textures_extra.py

Lê os canvases do jogo (tools/ui_canvas.py) e grava extract/ui/<tela>_textures.txt, importadas por
tools/ue/import_ui.py.
"""
import glob
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
SCREENS = ["compass", "encumbrance", "interactscreen", "damagenumbers"]

for name in SCREENS:
    src = glob.glob(os.path.join(UI, "lyshineui", "**", f"{name}.dynamicuicanvas.json"), recursive=True)[0]
    tmp = os.path.join(UI, "survey", f"{name}.flat.json")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"), src, tmp, "--tex", os.path.join(UI, "tex")],
                   check=True, capture_output=True)
    els = json.load(open(tmp))["elements"]
    tex = sorted({e["image"]["texture"] for e in els if e.get("image") and e["image"].get("texture")})
    open(os.path.join(UI, f"{name}_textures.txt"), "w").write("\n".join(tex))
    print(f"{name}: {len(tex)} texturas")
