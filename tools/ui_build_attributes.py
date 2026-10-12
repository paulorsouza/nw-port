"""Aba de atributos da janela "Personagem" do jogo (lyshineui/skills/skills.dynamicuicanvas, subárvore Attributes).

    python3 tools/ui_build_attributes.py

Gera unreal/NWPort/Data/UI/attributes_layout.<idioma>.json e extract/ui/attributes_textures.txt. Aplica o que o Lua
do jogo faz e não está no arquivo: moldura padrão, nome de cada linha (Força, Destreza, Inteligência, Foco,
Constituição), textos dos botões (−, +, Confirmar), foco dos botões só com o mouse em cima (o C++ liga) e o valor
pendente desligado. Ficam de fora a barra de limiares (o Lua monta as seções; o FNWAttributesMenu desenha uma barra
simples com a textura do jogo), os ícones de bônus, o "?" e a coluna de estatísticas da direita.
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
KEEP = ["_root/Skills/ScreenHolder/FrameMultiBg", "_root/Skills/ScreenHolder/FrameHeader", "_root/Skills/ScreenHolder/Attributes"]
EXCLUDE = ["AttributeThresholdBonusBar", "GlowIconHolder", "AttributeIconHolder", "QuestionMark", "StatsSectionBg",
           "ValueTooltipSetter", "buttonTooltipSetter", "spinner", "buttonSecondaryIconSprite", "buttonSecondaryIconHolder",
           # Redistribuir (custa moedas no jogo), estatísticas de dano/vida e marcos da barra de XP: o port não tem.
           "Respec", "VitalsLabel", "VitalsLabelDivider", "BaseStats", "DamageLabel", "DabageLabelDivider", "DamageStats",
           "ViewMilestoneButton", "NextMilestoneLabel", "ExpansionBadgeWithText", "MultiplierBg", "MeterRestedFill",
           "IncreaseDelta", "DeltaXpBg", "WeaponLine"]
# Peças de foco/realce dos botões: o jogo liga só no hover.
HOVER_ONLY = {"buttonBgFocus", "buttonFocus", "frameFocus", "heroBorder", "buttonBottomGlow", "buttonFocusHero"}
ROWS = {"Attribute": "@ui_strength", "Attribute2": "@ui_dexterity", "Attribute3": "@ui_intelligence",
        "Attribute4": "@ui_focus", "Attribute5": "@ui_constitution"}
FRAME_BG = "lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.dds"
EXTRA = [FRAME_BG, "lyshineui/images/attributes/attributebarbg.dds", "lyshineui/images/attributes/attributebarbgoverlay.dds"]


def main():
    tmp = os.path.join(UI, "attributes.layout.json")
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"),
                    os.path.join(UI, "lyshineui", "skills", "skills.dynamicuicanvas.json"), tmp, "--tex", os.path.join(UI, "tex"),
                    "--keep", ",".join(KEEP), "--enable", "Attributes", "--exclude", ",".join(EXCLUDE)],
                   check=True, capture_output=True)
    lay = json.load(open(tmp))
    for e in lay["elements"]:
        p, name = e["path"], e["name"]
        parts = p.split("/")
        if p.endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            e["image"]["texture"] = FRAME_BG
            e["image"]["type"] = "sliced"
            sp = os.path.join(UI, "tex", FRAME_BG.replace(".dds", ".sprite"))
            if os.path.exists(sp):
                m = re.search(r'm_left="([\d.]+)" m_right="([\d.]+)" m_top="([\d.]+)" m_bottom="([\d.]+)"', open(sp).read())
                if m:
                    e["image"]["borders"] = dict(zip(("left", "right", "top", "bottom"), map(float, m.groups())))
        if p.endswith("FrameHeader/FrameHeaderText") and e.get("text"):
            e["text"]["text"] = "@ui_attributes"
        if name in HOVER_ONLY:
            e["enabled"] = False
        if name == "Text" and len(parts) >= 2 and parts[-2] in ROWS and "AttributeModifiers" in p:
            e["text"]["text"] = ROWS[parts[-2]]
        if p.endswith("SubtractButton/buttonText") and e.get("text"):
            e["text"]["text"] = "−"
        if p.endswith("AddButton/buttonText") and e.get("text"):
            e["text"]["text"] = "+"
        if p.endswith("Attributes/Confirm/buttonText") and e.get("text"):
            e["text"]["text"] = "@ui_confirm"
        if name == "PendingValue":
            e["enabled"] = False
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"attributes_layout.{lang}.json")
        json.dump({"canvases": [{"name": "attributes", "size": lay["size"], "elements": elements}]}, open(out, "w"),
                  separators=(",", ":"))
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]} | set(EXTRA))
    open(os.path.join(UI, "attributes_textures.txt"), "w").write("\n".join(textures) + "\n")
    print(f"{len(lay['elements'])} elementos, {len(textures)} texturas")


if __name__ == "__main__":
    main()
