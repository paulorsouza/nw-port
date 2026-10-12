"""Janela de maestria de armas do jogo (lyshineui/skills/skills.dynamicuicanvas, subárvore MasteryTreeWindow).

    python3 tools/ui_build_mastery.py [--preview prévia.png]

Gera unreal/NWPort/Data/UI/mastery_layout.<idioma>.json (pt-br e en-us). Aplica o que o Lua do jogo faz ao abrir a tela e não está no
arquivo: estilo padrão da moldura (FrameMultiBg: FRAME_STYLE_DEFAULT_2023) e textos @chave resolvidos em pt-BR.
"""
import argparse
import glob
import json
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI = os.path.join(ROOT, "extract", "ui")
LOC_ROOT = os.path.join(ROOT, "extract", "loc", "localization")
# Idiomas do clã: português e inglês. Cada tela sai num arquivo por idioma (<nome>.<idioma>.json).
LANGS = ["pt-br", "en-us"]
KEEP = ["_root/Skills/ScreenHolder/FrameMultiBg", "_root/Skills/ScreenHolder/FrameHeader", "_root/Skills/ScreenHolder/MasteryTreeWindow"]
EXCLUDE = ["AbilitySelection", "SlotOptions", "ExpansionBadgeWithText", "AzothTextContainer", "FreeText", "Respec"]
# Texturas dos estados dos nós que o script troca (masterynode.luac).
EXTRA = ["lyshineui/images/skills/mastery/" + n for n in (
    "masterynodeactivebg.dds", "masterynodeactiveborder.dds", "masterynodeactiverune.dds", "masterynodeinfobg.dds",
    "masterynodeinfoborder.dds", "masterynodeinforune.dds", "masterynodelockiconglow.dds",
    "masterynodeultimatetickuncommitted.dds")] + ["lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.dds"]


def load_loc(lang="pt-br"):
    out = {}
    for f in glob.glob(os.path.join(LOC_ROOT, lang, "*.loc.xml")):
        for m in re.finditer(r'<string key="([^"]+)"[^>]*>(.*?)</string>', open(f, encoding="utf-8").read(), re.S):
            out[m.group(1).lower()] = re.sub(r"<[^>]+>", "", m.group(2))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    a = ap.parse_args()
    tmp = os.path.join(UI, "mastery.layout.json")
    subprocess.run(["python3", os.path.join(ROOT, "tools", "ui_canvas.py"),
                    os.path.join(UI, "lyshineui", "skills", "skills.dynamicuicanvas.json"), tmp, "--tex", os.path.join(UI, "tex"),
                    "--keep", ",".join(KEEP), "--enable", "MasteryTreeWindow", "--exclude", ",".join(EXCLUDE)], check=True)
    lay = json.load(open(tmp))
    for e in lay["elements"]:
        # FrameMultiBg com o estilo padrão de 2023 (o script aplica a textura e o 9-slice).
        if e["path"].endswith("FrameMultiBg/FrameBg") and e.get("image") and not e["image"]["texture"]:
            e["image"]["texture"] = "lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.dds"
            e["image"]["type"] = "sliced"
            sp = os.path.join(UI, "tex", "lyshineui/images/slices/framemultibg/framestyles/framebgdefault2023.sprite")
            if os.path.exists(sp):
                m = re.search(r'm_left="([\d.]+)" m_right="([\d.]+)" m_top="([\d.]+)" m_bottom="([\d.]+)"', open(sp).read())
                if m:
                    e["image"]["borders"] = dict(zip(("left", "right", "top", "bottom"), map(float, m.groups())))
    for lang in LANGS:
        loc = load_loc(lang)
        elements = json.loads(json.dumps(lay["elements"]))
        for e in elements:
            t = e.get("text")
            if t and t["text"].startswith("@"):
                t["text"] = loc.get(t["text"][1:].lower(), t["text"])
        out = os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", f"mastery_layout.{lang}.json")
        json.dump({"canvases": [{"name": "mastery", "size": lay["size"], "elements": elements}]}, open(out, "w"), separators=(",", ":"))
    # Ícones de todos os nós (ativas, passivas, ultimate) e fundos das ativas.
    abil = json.load(open(os.path.join(ROOT, "unreal", "NWPort", "Data", "UI", "abilities.pt-br.json")))
    icons = {n[k].lower() for w in abil.values() for n in w.get("nodes", []) for k in ("icon", "background") if n.get(k)}
    icons |= {w["hud_icon"].lower() for w in abil.values() if w.get("hud_icon")}
    textures = sorted({e["image"]["texture"].lower() for e in lay["elements"] if e.get("image") and e["image"]["texture"]} | set(EXTRA) | icons)
    open(os.path.join(UI, "mastery_textures.txt"), "w").write("\n".join(textures))
    print(f"{len(lay['elements'])} elementos, {len(textures)} texturas -> {out}")
    if a.preview:
        import sys
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import ui_canvas
        ui_canvas.preview(lay["elements"], tuple(lay["size"]), os.path.join(UI, "tex"), a.preview)


if __name__ == "__main__":
    main()
