#!/usr/bin/env python3
"""Tela do jogo a partir do canvas e de um arquivo de estado (esteira 4: refino, armazém, ...).

    python3 tools/ui_build_screen.py tools/ui_screens/<tela>.json [--preview saida.png]

O canvas tem as peças de todos os modos; o Lua do jogo (extract/lua/lyshineui/, decompilado) decide o que aparece.
O arquivo de estado diz isso para a tela que o port mostra:

    {
      "canvas": "lyshineui/crafting/craftingscreenv4",   # extract/ui/<canvas>.dynamicuicanvas.json
      "name": "refining",                                # Data/UI/<name>_layout.<idioma>.json, canvas "<name>"
      "keep": ["_root/CraftingScreen/..."],              # subárvores que ficam (vazio = tudo)
      "exclude": ["MultiCraft"],                         # nomes cujas subárvores saem
      "show": ["/Refining"], "hide": ["/QualityComplex"],# sufixos de caminho ligados/desligados (o que o Lua faz)
      "set": {"/FrameHeaderText": {"text": "@ui_x", "color": [1, 1, 1, 1], "texture": "lyshineui/...dds"}},
      "extra_textures": ["lyshineui/images/...dds"],     # texturas que o C++ usa fora do layout
      "buttons": {"/FilterButton": "default"},           # estilo do slices/button.lua (camadas, texturas, fonte)
      "styles": {"/EntryText": "FONT_STYLE_X"}           # estilo de texto do _common/uistyle.lua (tools/ui_styles.py)
    }

Saída: unreal/NWPort/Data/UI/<name>_layout.<idioma>.json e extract/ui/<name>_textures.txt (import: tools/ue/import_ui.py).
Textos @chave resolvidos por idioma; o C++ preenche os dados (FNWUILayout, TextOverride/Visible/DrawSubtree).
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

    # Raiz do canvas: no arquivo é uma caixa de 100×100 no centro; no jogo o HUD estica a tela inteira. Sem isso, o que
    # é ancorado em cima/embaixo/nos cantos (barra de título, rodapé, painéis) cai perto do centro.
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
    # Estilos do Lua (uistyle.lua via tools/ui_styles.py): fonte, tamanho, cor, espaçamento, caixa.
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
    # Botões (slices/button.lua, SetButtonStyle): o canvas traz todas as camadas; o estilo padrão usa fundo e moldura
    # 2023, sem borda/sequência de herói, foco a 0,2 e brilhos apagados.
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
                    e["enabled"] = False   # dica de tecla, ícone secundário e spinner: só quando o Lua liga
                if kind == "hero":
                    # BUTTON_STYLE_HERO: fundo preto, sequência azul a 1, brilho inferior em y −11, borda herói a 0
                    # (acende no foco), moldura heroframe2023, texto FONT_STYLE_BUTTON_HERO_2023.
                    if rel == "/buttonBg" and e.get("image"):
                        e["image"]["color"] = [0, 0, 0, 1]
                    elif rel in ("/heroBorder", "/buttonFocus", "/buttonFocusHero", "/heroImageSequenceDisabled", "/frameFocus",
                                 "/buttonBgFocus"):
                        e["fade"] = 0
                    elif rel == "/heroImageSequence":
                        # Flipbook (UiFlipbookAnimation, sem imagem no canvas): o C++ troca o quadro blue_loop/blue_NN.
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
    # visible (prévia) = pai visível e o próprio ligado, depois das trocas acima.
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
    print(f"{name}: {len(lay['elements'])} elementos, {len(textures)} texturas")


if __name__ == "__main__":
    main()
